#include "AbilitySystem/Abilities/Combat/GA_WeaponAttack.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CYCombatGameplayTags.h"
#include "AbilitySystem/Effects/CYCombatGameplayEffects.h"
#include "Character/CYPlayerCharacter.h"
#include "Components/Items/CYWeaponComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Items/CYWeaponBase.h"
#include "Player/CYPlayerState.h"

UGA_WeaponAttack::UGA_WeaponAttack()
{
	// 어빌리티마다 새 인스턴스 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	// 클라이언트에서 예측 실행하고 서버에서 검증
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 이 어빌리티를 식별할 태그 설정
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(CYGameplayTags::Ability_Combat_WeaponAttack);
	SetAssetTags(AssetTags);

	// 어빌리티 실행 중에 소유할 태그 (다른 시스템에서 상태 확인 가능)
	FGameplayTagContainer OwnedTags;
	OwnedTags.AddTag(CYGameplayTags::State_Combat_Attacking);
	ActivationOwnedTags = OwnedTags;

	// 블록 태그 설정
	FGameplayTagContainer BlockedTags;
	BlockedTags.AddTag(CYGameplayTags::State_Stunned);
	BlockedTags.AddTag(CYGameplayTags::State_Captured);
	BlockedTags.AddTag(CYGameplayTags::State_Jail);
	BlockedTags.AddTag(CYGameplayTags::State_Combat_Attacking);
	BlockedTags.AddTag(CYGameplayTags::Ability_Combat_PlaceTrap);
	ActivationBlockedTags = BlockedTags;
}

bool UGA_WeaponAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 무기가 장착되어 있는지 체크
	AActor* OwnerActor = ActorInfo->AvatarActor.Get();
	if (!OwnerActor)
	{
		return false;
	}

	UCYWeaponComponent* WeaponComp = OwnerActor->FindComponentByClass<UCYWeaponComponent>();
	if (!WeaponComp || !WeaponComp->CurrentWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot activate weapon attack: No weapon equipped"));
		return false;
	}

	return true;
}

void UGA_WeaponAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
	// 서버 권한 또는 예측 키 체크
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

	// 수동 쿨다운 체크
    if (IsOnCooldown(ActorInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
	
	// 몽타주가 있으면 재생하면서 이벤트 대기
	if (AttackMontage)
	{
		// 몽타주 재생 태스크 생성
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("PlayAttackMontage"),
			AttackMontage,
			1.0f
		);

		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UGA_WeaponAttack::OnMontageCompleted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_WeaponAttack::OnMontageCancelled);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_WeaponAttack::OnMontageCancelled);
		}

		// 공격 이벤트 대기 태스크 생성
		AttackEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			CYGameplayTags::Event_Combat_WeaponAttack,
			nullptr,
			false,
			true
		);

		if (AttackEventTask)
		{
			AttackEventTask->EventReceived.AddDynamic(this, &UGA_WeaponAttack::OnAttackEventReceived);
		}

		// 두 태스크 모두 활성화
		if (MontageTask && AttackEventTask)
		{
			MontageTask->ReadyForActivation();
			AttackEventTask->ReadyForActivation();
			
			UE_LOG(LogTemp, Warning, TEXT("Attack montage and event listener started"));
			return;
		}
	}
	
	// 몽타주가 없거나 태스크 생성 실패 시 즉시 공격
	PerformAttack();
	ApplyWeaponCooldown(Handle, ActorInfo, ActivationInfo);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_WeaponAttack::OnMontageCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT("Attack montage completed"));
	
	// 쿨다운 적용
	ApplyWeaponCooldown(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo());
	
	// 어빌리티 종료
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void UGA_WeaponAttack::OnMontageCancelled()
{
	UE_LOG(LogTemp, Warning, TEXT("Attack montage cancelled"));
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}

void UGA_WeaponAttack::OnAttackEventReceived(FGameplayEventData Payload)
{
	UE_LOG(LogTemp, Warning, TEXT("Attack event received from AnimNotify"));
	PerformAttack();
}

void UGA_WeaponAttack::PerformAttack()
{
	// 어빌리티를 실행하는 액터(플레이어) 가져오기
	AActor* OwnerActor = GetAvatarActorFromActorInfo();
	if (!OwnerActor) return;

	// WeaponComponent에서 현재 무기 정보 가져오기
	UCYWeaponComponent* WeaponComp = OwnerActor->FindComponentByClass<UCYWeaponComponent>();
	if (!WeaponComp || !WeaponComp->CurrentWeapon) return;

	ACYWeaponBase* CurrentWeapon = WeaponComp->CurrentWeapon;

	// 근접 공격 범위 설정
	float AttackRange = CurrentWeapon->AttackRange;   // 무기마다 다른 사거리
	float AttackRadius = 80.0f;    // 공격 반경
    
	// 캐릭터 위치와 방향
	FVector StartLocation = OwnerActor->GetActorLocation();
	FVector ForwardVector = OwnerActor->GetActorForwardVector();
	FVector EndLocation = StartLocation + (ForwardVector * AttackRange);
    
	// 스피어 스윕으로 범위 공격
	TArray<FHitResult> HitResults;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);  // 자기 자신은 제외
	Params.bTraceComplex = false;        // 단순 충돌만 체크
    
	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,  // 캐릭터만 타격
		FCollisionShape::MakeSphere(AttackRadius),
		Params
	);
    
	if (bHit && HitResults.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Melee attack HIT %d targets"), HitResults.Num());
        
		// 모든 적중된 대상에게 데미지 적용
		for (const FHitResult& HitResult : HitResults)
		{
			// 자기 자신 제외
			if (HitResult.GetActor() == OwnerActor) continue;
            
			ProcessHitTarget(HitResult);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Melee attack MISS"));
	}
    
	// 디버그 시각화 (개발 중에만 표시)
	//DrawMeleeAttackDebug(StartLocation, EndLocation, AttackRadius, bHit);
}

bool UGA_WeaponAttack::IsOnCooldown(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(CYGameplayTags::Cooldown_Combat_WeaponAttack);
}

void UGA_WeaponAttack::ProcessHitTarget(const FHitResult& HitResult)
{
    AActor* Target = HitResult.GetActor();
    if (!Target) return;

	// 팀 체크 Robber만 공격받도록
	if (ACYPlayerCharacter* TargetCharacter = Cast<ACYPlayerCharacter>(Target))
	{
		if (ACYPlayerState* TargetPS = TargetCharacter->GetPlayerState<ACYPlayerState>())
		{
			if (TargetPS->GetTeamRole() != ECYTeamRole::Robber)
			{
				return;
			}
		}
	}

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!TargetASC) 
    {
        UE_LOG(LogTemp, Warning, TEXT("Hit target has no ASC: %s"), *Target->GetName());
        return;
    }

    ApplyDamageToTarget(TargetASC, HitResult);
}

void UGA_WeaponAttack::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& HitResult)
{
	// 무기에서 데미지 가져오기
	float DamageAmount = 50.0f;
	if (UCYWeaponComponent* WeaponComp = GetAvatarActorFromActorInfo()->FindComponentByClass<UCYWeaponComponent>())
	{
		if (WeaponComp->CurrentWeapon)
		{
			DamageAmount = WeaponComp->CurrentWeapon->BaseDamage;
		}
	}
    
	// GameplayEffect 적용을 위한 컨텍스트 생성
    FGameplayEffectContextHandle EffectContext = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	// 공격자 정보 추가
    EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());
	// 충돌 정보 추가 (타격 위치 등)
    EffectContext.AddHitResult(HitResult);

	// 데미지 GameplayEffect 스펙 생성
    FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(UGE_WeaponDamage::StaticClass(), 1);
	if (DamageSpec.IsValid())
	{
		// 음수로 설정 (데미지)
		DamageSpec.Data->SetSetByCallerMagnitude(FName("Damage"), -DamageAmount);
        
		GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
	}
}

void UGA_WeaponAttack::ApplyWeaponCooldown(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, 
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// 현재 무기에서 쿨다운 값 가져오기
	AActor* OwnerActor = GetAvatarActorFromActorInfo();
	UCYWeaponComponent* WeaponComp = OwnerActor->FindComponentByClass<UCYWeaponComponent>();

	float CooldownDuration = 1.5f; // 기본값
	if (WeaponComp && WeaponComp->CurrentWeapon)
	{
		CooldownDuration = WeaponComp->CurrentWeapon->AttackCooldown; // 무기별 쿨다운 사용
	}
    
	// 쿨다운 GameplayEffect를 동적으로 생성
	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(UGE_WeaponAttackCooldown::StaticClass(), 1);
	if (CooldownSpec.IsValid())
	{
		FGameplayTag CooldownTag = CYGameplayTags::Cooldown_Combat_WeaponAttack;
		if (CooldownTag.IsValid())
		{
			CooldownSpec.Data->DynamicGrantedTags.AddTag(CooldownTag);
		}

		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec);
	}
}

void UGA_WeaponAttack::DrawMeleeAttackDebug(const FVector& Start, const FVector& End, float Radius, bool bHit)
{
	/*
	if (!GetWorld()) return;
    
	FColor DebugColor = bHit ? FColor::Red : FColor::Green;
	float DebugDuration = 1.0f;
    
	// 공격 방향 선 표시
	DrawDebugLine(GetWorld(), Start, End, DebugColor, false, DebugDuration, 0, 2.0f);
    
	// 공격 범위 구체 표시
	DrawDebugSphere(GetWorld(), End, Radius, 12, DebugColor, false, DebugDuration, 0, 2.0f);
    
	// 시작 지점 표시
	DrawDebugSphere(GetWorld(), Start, 20.0f, 8, FColor::Blue, false, DebugDuration);
	*/
}