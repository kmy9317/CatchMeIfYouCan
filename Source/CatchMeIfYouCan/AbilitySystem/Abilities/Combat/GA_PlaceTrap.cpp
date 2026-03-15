#include "AbilitySystem/Abilities/Combat/GA_PlaceTrap.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CYCombatGameplayTags.h"
#include "AbilitySystem/Effects/CYCombatGameplayEffects.h"
#include "Character/CYPlayerCharacter.h"
#include "Items/Traps/CYTrapBase.h"
#include "Components/Items/CYInventoryComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UGA_PlaceTrap::UGA_PlaceTrap()
{
	// 어빌리티마다 새 인스턴스 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	
	// LocalPredicted 클라이언트에서 예측 실행하고 서버에서 검증
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 태그 설정
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(CYGameplayTags::Ability_Combat_PlaceTrap);
	SetAssetTags(AssetTags);
    
	// 블로킹 태그 설정
	FGameplayTagContainer BlockedTags;
	BlockedTags.AddTag(CYGameplayTags::State_Stunned);
	BlockedTags.AddTag(CYGameplayTags::State_Captured);
	BlockedTags.AddTag(CYGameplayTags::State_Jail);
	BlockedTags.AddTag(CYGameplayTags::Ability_Combat_PlaceTrap);
	BlockedTags.AddTag(CYGameplayTags::State_Combat_Attacking);
	ActivationBlockedTags = BlockedTags;
    
	// 쿨다운 GE 클래스 설정
	CooldownGameplayEffectClass = UGE_TrapPlaceCooldown::StaticClass();
    
	UE_LOG(LogTemp, Warning, TEXT("PlaceTrap GA created"));
}

void UGA_PlaceTrap::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
	// 서버 권한 또는 예측 키 체크 (멀티플레이 동기화)
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

	if (IsOnCooldown(ActorInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("Trap placement on cooldown"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	ApplyTrapCooldown(Handle, ActorInfo, ActivationInfo);

	// 소스 오브젝트(들고 있는 트랩)에서 트랩 아이템 가져오기
    ACYTrapBase* TrapItem = GetTrapItemFromSource();
    if (!TrapItem)
    {
    	// 소스에 없으면 인벤토리에서 찾기 (폴백)
        TrapItem = Cast<ACYTrapBase>(FindTrapItemInInventory());
        if (!TrapItem)
        {
            UE_LOG(LogTemp, Error, TEXT("No trap item found"));
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

	// 수량 체크
	if (TrapItem->ItemCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Trap item count is 0"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 정보 캐시 (몽타주 완료 후 사용)
	CachedHandle = Handle;
	CachedActorInfo = ActorInfo;
	CachedActivationInfo = ActivationInfo;
	CachedTrapItem = TrapItem;
	CachedSpawnLocation = CalculateSpawnLocation();

	// 애니메이션 몽타주 재생
	if (PlaceTrapMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("PlayPlaceTrapMontage"),
			PlaceTrapMontage,
			1.0f
		);

		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UGA_PlaceTrap::OnPlaceTrapMontageCompleted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_PlaceTrap::OnPlaceTrapMontageCancelled);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_PlaceTrap::OnPlaceTrapMontageCancelled);
			MontageTask->ReadyForActivation();
			
			UE_LOG(LogTemp, Warning, TEXT("🎬 Place trap montage started via AbilityTask"));
			return;
		}
	}
	
	// 몽타주가 없거나 재생 실패 시 즉시 트랩 설치
	OnPlaceTrapMontageCompleted();
}

void UGA_PlaceTrap::OnPlaceTrapMontageCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT("Place trap montage completed - placing trap"));
	
	// 실제 트랩 설치 로직 실행
	PerformTrapPlacement();
	
	// 어빌리티 종료
	EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);

	// 태스크 정리
	MontageTask = nullptr;
}

void UGA_PlaceTrap::OnPlaceTrapMontageCancelled()
{
	UE_LOG(LogTemp, Warning, TEXT("Place trap montage cancelled"));
	EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, true);
	MontageTask = nullptr;
}

bool UGA_PlaceTrap::IsOnCooldown(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(CYGameplayTags::Cooldown_Combat_TrapPlace);
}

void UGA_PlaceTrap::ApplyTrapCooldown(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(UGE_TrapPlaceCooldown::StaticClass(), 1);
	if (CooldownSpec.IsValid())
	{
		FGameplayTag CooldownTag = CYGameplayTags::Cooldown_Combat_TrapPlace;
		if (CooldownTag.IsValid())
		{
			CooldownSpec.Data->DynamicGrantedTags.AddTag(CooldownTag);
		}

		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
		UE_LOG(LogTemp, Warning, TEXT("Trap cooldown applied"));
	}
}

void UGA_PlaceTrap::PerformTrapPlacement()
{
	// 서버에서만 실제 트랩 생성
	if (!GetCurrentActorInfo()->IsNetAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("Client prediction - trap will be created on server"));
		return;
	}
	
	// 다시 한번 수량 체크 (서버 검증)
	if (!CachedTrapItem || CachedTrapItem->ItemCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Trap item no longer available on server"));
		return;
	}
	
	// 트랩 아이템 정보로 실제 트랩 액터 생성
	ACYTrapBase* NewTrap = CreateTrapFromItem(CachedTrapItem, CachedSpawnLocation);
	
	if (NewTrap)
	{
		UE_LOG(LogTemp, Warning, TEXT("Trap placed: %s at %s"), 
			   *NewTrap->ItemName.ToString(), *CachedSpawnLocation.ToString());
		
		// 인벤토리에서 트랩 아이템 1개 소모
		ConsumeItemFromInventory(CachedTrapItem);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create trap"));
	}
}

ACYTrapBase* UGA_PlaceTrap::GetTrapItemFromSource() const
{
	// 현재 실행 중인 어빌리티의 스펙 정보 가져오기
    const FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec();
    if (AbilitySpec && AbilitySpec->SourceObject.IsValid())
    {
    	// 소스 오브젝트를 트랩으로 캐스팅 (들고 있는 트랩 아이템)
        ACYTrapBase* TrapItem = Cast<ACYTrapBase>(AbilitySpec->SourceObject.Get());
        if (TrapItem)
        {
            UE_LOG(LogTemp, Warning, TEXT("Using trap from source: %s"), *TrapItem->ItemName.ToString());
            return TrapItem;
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("No trap source found, searching inventory..."));
    return nullptr;
}

ACYItemBase* UGA_PlaceTrap::FindTrapItemInInventory()
{
	// 어빌리티를 실행하는 액터(플레이어) 가져오기
    AActor* OwnerActor = GetAvatarActorFromActorInfo();
    if (!OwnerActor) return nullptr;

	// 플레이어의 인벤토리 컴포넌트 찾기
    UCYInventoryComponent* InventoryComp = OwnerActor->FindComponentByClass<UCYInventoryComponent>();
    if (!InventoryComp) return nullptr;

    // 아이템 슬롯에서 트랩 찾기
    for (ACYItemBase* Item : InventoryComp->ItemSlots)
    {
    	// 아이템이 존재하고, 트랩 타입이고, 수량이 있는지 체크
        if (Item && Item->ItemType == EItemType::Trap && Item->ItemCount > 0)
        {
            return Item;
        }
    }

    return nullptr;
}

ACYTrapBase* UGA_PlaceTrap::CreateTrapFromItem(ACYItemBase* TrapItem, const FVector& Location)
{
	if (!TrapItem || !GetWorld()) 
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid TrapItem or World"));
		return nullptr;
	}
    
	// 트랩 아이템을 트랩 액터로 캐스팅
	ACYTrapBase* SourceTrap = Cast<ACYTrapBase>(TrapItem);
	if (!SourceTrap)
	{
		UE_LOG(LogTemp, Error, TEXT("TrapItem is not ACYTrapBase! Item: %s, Class: %s"), 
			   *TrapItem->ItemName.ToString(), 
			   *TrapItem->GetClass()->GetName());
		return nullptr;
	}
    
	// 같은 클래스의 새 트랩 생성
	TSubclassOf<ACYTrapBase> TrapClass = SourceTrap->GetClass();

	// 액터 생성을 위한 파라미터 설정
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetAvatarActorFromActorInfo();
	SpawnParams.Instigator = Cast<APawn>(GetAvatarActorFromActorInfo());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 새 트랩 액터 생성
	ACYTrapBase* NewTrap = GetWorld()->SpawnActor<ACYTrapBase>(TrapClass, Location, FRotator::ZeroRotator, SpawnParams);
    
	if (!NewTrap)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn trap of class: %s"), *TrapClass->GetName());
		return nullptr;
	}

	NewTrap->OverridePrimaryValue = SourceTrap->OverridePrimaryValue;
	NewTrap->OverrideDuration = SourceTrap->OverrideDuration;
    
	// 플레이어가 설치한 트랩으로 변환 (맵 트랩 -> 플레이어 트랩)
	NewTrap->PlaceTrap(Location, Cast<ACYPlayerCharacter>(GetAvatarActorFromActorInfo()));
    
	UE_LOG(LogTemp, Warning, TEXT("Created trap: %s at %s"), 
		   *NewTrap->ItemName.ToString(), *Location.ToString());
    
	return NewTrap;
}

FVector UGA_PlaceTrap::CalculateSpawnLocation()
{
	// 어빌리티 실행자(플레이어) 가져오기
	AActor* OwnerActor = GetAvatarActorFromActorInfo();
	if (!OwnerActor) return FVector::ZeroVector;

	// 플레이어 앞쪽 바닥 찾기
	FVector PlayerLocation = OwnerActor->GetActorLocation();
	FVector ForwardDirection = OwnerActor->GetActorForwardVector();
    
	// 플레이어 앞쪽 100유닛 지점에서 시작
	FVector TraceStart = PlayerLocation + (ForwardDirection * 100.0f);
	TraceStart.Z += 50.0f;  // 시작점을 약간 위로 (바닥 찾기 위해)
    
	// 아래쪽으로 레이캐스트해서 바닥 찾기
	FVector TraceEnd = TraceStart + FVector(0, 0, -200.0f);  // 200유닛 아래까지
    
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);  // 플레이어는 무시
	Params.bTraceComplex = false;        // 단순 충돌만 체크
    
	FHitResult HitResult;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult, 
		TraceStart, 
		TraceEnd, 
		ECC_WorldStatic,  // 정적 월드 오브젝트만 (바닥, 벽 등)
		Params
	);
    
	if (bHit)
	{
		// 바닥을 찾았으면 그 위치 반환
		FVector GroundLocation = HitResult.Location;
		GroundLocation.Z += 2.0f;  // 바닥에서 2유닛 위
        
		UE_LOG(LogTemp, Warning, TEXT("Trap placement: Found ground at %s"), 
			   *GroundLocation.ToString());
        
		return GroundLocation;
	}
    
	// 바닥을 못 찾으면 플레이어 위치 기준으로
	FVector FallbackLocation = PlayerLocation + (ForwardDirection * 100.0f);
	FallbackLocation.Z = PlayerLocation.Z;  // 플레이어와 같은 높이
    
	UE_LOG(LogTemp, Warning, TEXT("Trap placement: Using fallback location %s"), 
		   *FallbackLocation.ToString());
    
	return FallbackLocation;
}

void UGA_PlaceTrap::ConsumeItemFromInventory(ACYItemBase* Item)
{
	if (!Item) return;

	// 서버에서만 소비
	if (!GetCurrentActorInfo()->IsNetAuthority())
	{
		return;
	}

	AActor* OwnerActor = GetAvatarActorFromActorInfo();
	UCYInventoryComponent* InventoryComp = OwnerActor->FindComponentByClass<UCYInventoryComponent>();
	if (!InventoryComp) return;

	if (Item->ItemCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item count already 0, skipping consume"));
		return;
	}

	// 아이템 수량 감소
	Item->ItemCount--;
    
	if (Item->ItemCount <= 0)
	{
		// 아이템이 모두 소모되면 슬롯에서 제거
		for (int32 i = 0; i < InventoryComp->ItemSlots.Num(); i++)
		{
			if (InventoryComp->ItemSlots[i] == Item)
			{
				InventoryComp->ItemSlots[i] = nullptr;
				InventoryComp->OnInventoryChanged.Broadcast(i + 4, nullptr);

				// CurrentHeldItem이 소진된 아이템이면 null로 설정
				if (InventoryComp->CurrentHeldItem == Item)
				{
					InventoryComp->CurrentHeldItem = nullptr;
					InventoryComp->OnHeldItemChanged.Broadcast(Item, nullptr);
				}
            	
				Item->Destroy();
				break;
			}
		}
	}
	else
	{
		// 수량만 감소한 경우
		for (int32 i = 0; i < InventoryComp->ItemSlots.Num(); i++)
		{
			if (InventoryComp->ItemSlots[i] == Item)
			{
				InventoryComp->OnInventoryChanged.Broadcast(i + 4, Item);
				break;
			}
		}
	}
    
	UE_LOG(LogTemp, Warning, TEXT("Consumed trap item: %s (Remaining: %d)"), 
		   *Item->ItemName.ToString(), Item->ItemCount);
}