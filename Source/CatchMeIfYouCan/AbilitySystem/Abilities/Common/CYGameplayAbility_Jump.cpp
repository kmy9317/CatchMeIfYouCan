// Fill out your copyright notice in the Description page of Project Settings.


#include "CYGameplayAbility_Jump.h"

#include "AbilitySystemComponent.h"
#include "CYLogChannels.h"
#include "AbilitySystem/CYCombatGameplayTags.h"
#include "Character/CYCharacterBase.h"
#include "Character/CYStatusGameplayTags.h"
#include "Character/Components/CYCharacterMovementComponent.h"

UCYGameplayAbility_Jump::UCYGameplayAbility_Jump(const FObjectInitializer& ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ECYAbilityActivationPolicy::OnInputTriggered;
}

bool UCYGameplayAbility_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// ActorInfo에서 CYCharacter 가져오기
	const ACYCharacterBase* CYCharacter = GetCYCharacterFromActorInfo();
	if (!CYCharacter)
	{
		return false;
	}

	if (CYCharacter->HasGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder))
	{
		return false;
	}
	
	// 기본 점프 조건 체크 || 사다리 타는 중이면 점프 허용 
	return CYCharacter->CanJump() || CYCharacter->HasGameplayTag(CYGameplayTags::Status_Movement_Climbing);
}

void UCYGameplayAbility_Jump::PreActivate(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	bWasClimbingBeforeJump = IsClimbingLadder();
	
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
}

void UCYGameplayAbility_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Prediction Key 체크
	if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		return;
	}
	
	// Ability Cost와 Cooldown 커밋
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (bWasClimbingBeforeJump && LadderJumpAbilityClass)
	{
		// 사다리 점프 어빌리티 활성화
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (ASC)
		{
			if (ASC->TryActivateAbilityByClass(LadderJumpAbilityClass))
			{
				UE_LOG(LogCY, Log, TEXT("[%s] Jump: Activated LadderJump Ability"),
					HasAuthority(&ActivationInfo) ? TEXT("Server") : TEXT("Client"));
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			}
			else
			{
				UE_LOG(LogCY, Warning, TEXT("[%s] Jump: Failed to activate LadderJump Ability, using fallback"),
					HasAuthority(&ActivationInfo) ? TEXT("Server") : TEXT("Client"));
				StartJump();
			}
		}
	}
	else
	{
		// 일반 점프
		StartJump();
	}
}

void UCYGameplayAbility_Jump::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UCYGameplayAbility_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	StopJump();

	bWasClimbingBeforeJump = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCYGameplayAbility_Jump::StartJump()
{
	ACYCharacterBase* CYCharacter = GetCYCharacterFromActorInfo();
	if (!CYCharacter)
	{
		return;
	}

	if (CYCharacter->IsLocallyControlled() && !CYCharacter->bPressedJump)
	{
		CYCharacter->UnCrouch();
		CYCharacter->Jump();
	}
}

void UCYGameplayAbility_Jump::StopJump()
{
	ACYCharacterBase* CYCharacter = GetCYCharacterFromActorInfo();
	if (!CYCharacter)
	{
		return;
	}
	
	if (CYCharacter->IsLocallyControlled() && CYCharacter->bPressedJump)
	{
		CYCharacter->StopJumping();
	}
	UE_LOG(LogCY, Warning, TEXT("Character Jump Stopped"));
}

bool UCYGameplayAbility_Jump::IsClimbingLadder() const
{
	const ACYCharacterBase* CYCharacter = GetCYCharacterFromActorInfo();
	if (!CYCharacter)
	{
		return false;
	}

	const UCYCharacterMovementComponent* MovementComp = Cast<UCYCharacterMovementComponent>(CYCharacter->GetCharacterMovement());
	if (!MovementComp)
	{
		return false;
	}

	return MovementComp->IsClimbingLadder();
}
