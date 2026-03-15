#include "CYJailPoint.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CYCombatGameplayTags.h"
#include "Character/CYCharacterBase.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameModes/InGame/CYInGameState.h"
#include "Player/CYPlayerState.h"

ACYJailPoint::ACYJailPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	
	RootPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RootPoint"));
	SetRootComponent(RootPoint);

	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));
	ArrowComponent->SetupAttachment(RootComponent);
	
	// EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));
	// EditorSprite->SetupAttachment(RootComponent);
	// EditorSprite->SetHiddenInGame(true);
	// EditorSprite->bIsEditorOnly = true;

	ExitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitVolume"));
	ExitVolume->SetupAttachment(RootComponent);
	ExitVolume->SetBoxExtent(FVector(200.f, 200.f, 200.f));
	ExitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	//ExitVolume->SetCollisionObjectType(ECC_WorldDynamic);
	//ExitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	//ExitVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

}

void ACYJailPoint::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ExitVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnVolumeBeginOverlap);
		ExitVolume->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OnVolumeEndOverlap);
	}
}

void ACYJailPoint::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TODO : 이 부분에 추가적으로 UI 알림이나 다른 이펙트 효과 추가 가능(그럴 경우 위의 HasAuthority 제거)
}

void ACYJailPoint::OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}
	
	ACYCharacterBase* CYCharacter = Cast<ACYCharacterBase>(OtherActor);
	if (!CYCharacter)
	{
		return;
	}
	
	ACYPlayerState* CYPS = CYCharacter->GetPlayerState<ACYPlayerState>();
	if (!CYPS || CYPS->GetTeamRole() != ECYTeamRole::Robber)
	{
		return;
	}
	
	if (!CYCharacter->HasGameplayTag(CYGameplayTags::State_Jail))
	{
		return;
	}
	
	ReleasePrisoner(CYCharacter);
}

void ACYJailPoint::ReleasePrisoner(ACYCharacterBase* Robber)
{
	if (UAbilitySystemComponent* RobberASC = Robber->GetAbilitySystemComponent())
	{
		// 태그 기반으로 해당 태그를 부여한 GE들을 제거
		if (RobberASC->RemoveActiveEffectsWithSourceTags(FGameplayTagContainer(CYGameplayTags::State_Jail)))
		{
			// 제거 성공시 생존 도둑 수 증가
			if (ACYInGameState* CYGS = GetWorld()->GetGameState<ACYInGameState>())
			{
				CYGS->UpdateAliveRobberCount(CYGS->GetAliveRobberCount() + 1);
			}
		}
	}
}
