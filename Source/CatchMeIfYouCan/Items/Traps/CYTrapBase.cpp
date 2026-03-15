#include "Items/Traps/CYTrapBase.h"
#include "Character/CYPlayerCharacter.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemComponent.h"
#include "CYDamageTrap.h"
#include "AbilitySystem/CYAbilitySystemComponent.h"
#include "AbilitySystem/CYCombatGameplayTags.h"
#include "TimerManager.h"
#include "AbilitySystem/Effects/CYCombatGameplayEffects.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Player/CYPlayerState.h"

ACYTrapBase::ACYTrapBase()
{
    ItemName = FText::FromString("Base Trap");
    ItemType = EItemType::Trap;
    MaxStackCount = 5; // 트랩은 스택 가능
    
    TrapState = ETrapState::MapPlaced;
    bIsArmed = false;
    
    // 기본 설정
    TriggerRadius = 100.0f;
    ArmingDelay = 2.0f;
    TrapLifetime = 60.0f;
}

void ACYTrapBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(ACYTrapBase, TrapState);
    DOREPLIFETIME(ACYTrapBase, bIsArmed);
	DOREPLIFETIME(ACYTrapBase, bHasTriggered);
}

void ACYTrapBase::BeginPlay()
{
    Super::BeginPlay();
    
    // 기본 메시 설정
    if (ItemMesh && !ItemMesh->GetStaticMesh())
    {
        UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder"));
        if (CylinderMesh)
        {
            ItemMesh->SetStaticMesh(CylinderMesh);
            ItemMesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.1f));
        }
    }
}

void ACYTrapBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		
		if (ArmingTimer.IsValid())
		{
			TimerManager.ClearTimer(ArmingTimer);
		}
		
		if (LifetimeTimer.IsValid())
		{
			TimerManager.ClearTimer(LifetimeTimer);
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

bool ACYTrapBase::UseItem(ACYPlayerCharacter* Character)
{
	if (!Character) return false;
    
    // GA_PlaceTrap 어빌리티 실행
    UCYAbilitySystemComponent* ASC = Cast<UCYAbilitySystemComponent>(Character->GetAbilitySystemComponent());
    if (!ASC)
    {
        UE_LOG(LogTemp, Error, TEXT("Character has no AbilitySystemComponent"));
        return false;
    }
    
    // 트랩 설치 어빌리티를 이 트랩 아이템을 소스로 실행
    bool bActivated = ASC->TryActivateAbilityByTagWithSource(
        CYGameplayTags::Ability_Combat_PlaceTrap, 
        this  // 이 트랩 아이템을 소스로 전달
    );
    
    if (bActivated)
    {
        UE_LOG(LogTemp, Warning, TEXT("Trap placement ability activated: %s"), *ItemName.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to activate trap placement ability"));
    }
    
    return bActivated;
}

void ACYTrapBase::PlaceTrap(const FVector& Location, ACYPlayerCharacter* Placer)
{
    if (!HasAuthority()) return;
    
    TrapState = ETrapState::PlayerPlaced;
    SetOwner(Placer);
    SetActorLocation(Location);
    
    // 픽업 상태로 변경
    bIsPickedUp = true;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    
    // 충돌 이벤트 재설정
    if (InteractionSphere)
    {
        InteractionSphere->OnComponentBeginOverlap.Clear();
        InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &ACYTrapBase::OnTrapSphereOverlap);
    }
    
    // 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(ArmingTimer, this, &ACYTrapBase::ArmTrap, ArmingDelay, false);
    GetWorld()->GetTimerManager().SetTimer(LifetimeTimer, [this](){ Destroy(); }, TrapLifetime, false);
    
    UE_LOG(LogTemp, Warning, TEXT("Trap placed: %s at %s"), 
           *ItemName.ToString(), *Location.ToString());
}

void ACYTrapBase::ArmTrap()
{
	if (!IsValid(this) || !HasAuthority() || TrapState != ETrapState::PlayerPlaced) 
	{
		return;
	}
    
    bIsArmed = true;
    
    // 트리거 반경으로 변경
	if (InteractionSphere && IsValid(InteractionSphere))
	{
		InteractionSphere->SetSphereRadius(TriggerRadius);
	}
    
    UE_LOG(LogTemp, Warning, TEXT("Trap armed: %s"), *ItemName.ToString());
}

void ACYTrapBase::OnTrapSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
	// 이미 트리거됐으면 무시
	if (bHasTriggered)
	{
		return;
	}
	
    // 트랩이 활성화 상태가 아니면 무시
    if (TrapState != ETrapState::PlayerPlaced || !bIsArmed || !HasAuthority()) return;
    
    // 설치한 플레이어는 무시
    if (OtherActor == GetOwner()) return;
    
    ACYPlayerCharacter* Target = Cast<ACYPlayerCharacter>(OtherActor);
    if (!Target) return;

	// 팀롤이 Robber인 경우만 트랩 발동
	if (ACYPlayerState* TargetPS = Target->GetPlayerState<ACYPlayerState>())
	{
		if (TargetPS->GetTeamRole() != ECYTeamRole::Robber)
		{
			return;
		}
	}

	// 트리거 플래그 설정
	bHasTriggered = true;
    OnTrapTriggered(Target);
}

void ACYTrapBase::OnTrapTriggered(ACYPlayerCharacter* Target)
{
	if (!Target || !HasAuthority()) return;
    
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (TargetASC)
	{
		for (TSubclassOf<UGameplayEffect> EffectClass : TrapEffects)
		{
			if (!EffectClass) continue;
            
			FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
			EffectContext.AddInstigator(GetOwner(), Target);
            
			FGameplayEffectSpecHandle EffectSpec = TargetASC->MakeOutgoingSpec(EffectClass, 1, EffectContext);
			if (EffectSpec.IsValid())
			{
				if (OverrideDuration > 0.0f)
				{
					EffectSpec.Data->SetDuration(OverrideDuration, true);
				}
                
				if (EffectClass == UGE_DamageTrap::StaticClass())
				{
					if (OverridePrimaryValue > 0.0f)
					{
						EffectSpec.Data->SetSetByCallerMagnitude(FName("TrapDamage"), -OverridePrimaryValue);
					}
					else if (ACYDamageTrap* DamageTrap = Cast<ACYDamageTrap>(this))
					{
						EffectSpec.Data->SetSetByCallerMagnitude(FName("TrapDamage"), -DamageTrap->DamageAmount);
					}
				}
                
				TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
			}
		}
	}
    
	Destroy();
}

void ACYTrapBase::OnRep_TrapState()
{
    // 상태 변경 시 시각적 업데이트
    if (TrapState == ETrapState::PlayerPlaced)
    {
        SetActorHiddenInGame(false);
        SetActorEnableCollision(true);
    }
}

void ACYTrapBase::OnRep_IsArmed()
{
}