// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CYGameplayAbility.h"
#include "CYGameplayAbility_ClimbLadder_Enter.generated.h"


enum class ECYLadderPhase : uint8;
enum class ELadderEntryType : uint8;

class ACYLadderBase;
class UCYCharacterMovementComponent;
class UCYAbilityTask_WaitForLadderExit;

/**
 * 사다리 등반 진입 및 관리 어빌리티
 * - 상호작용 시스템 또는 자동 그랩으로 활성화
 * - Movement Mode 전환 및 이탈 조건 모니터링
 */
UCLASS()
class CATCHMEIFYOUCAN_API UCYGameplayAbility_ClimbLadder_Enter : public UCYGameplayAbility
{
    GENERATED_BODY()
    
public:
    UCYGameplayAbility_ClimbLadder_Enter();

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    /** 사다리 상단으로 이탈 */
    UFUNCTION()
    void HandleLadderExitFromTop();

    /** 사다리 하단으로 이탈 */
    UFUNCTION()
    void HandleLadderExitFromBottom();

    /** 사다리 등반 취소 (점프 등) */
    UFUNCTION()
    void HandleLadderClimbingCancelled();
    
private:
    /**
     * 이벤트 데이터에서 사다리 정보 추출 및 검증
     */
    bool CanExtractLadderInfo(const FGameplayEventData* TriggerEventData, ACYLadderBase*& OutLadder, FVector& OutBottomLocation, FVector& OutTopLocation, FVector& OutFacingDirection, float& OutLadderStandOff, float& OutInitialRailParameter, bool& OutIsClimbingUp) const;

    /** 진입 타입에 따른 진입 몽타주 반환 */
    UAnimMontage* GetEntryMontageForType(ELadderEntryType EntryType) const;
    
    /** 상단 탈출 몽타주 완료 콜백 */
    UFUNCTION()
    void OnTopExitMontageCompleted();
    
    /** 상단 탈출 몽타주 취소/중단 콜백 */
    UFUNCTION()
    void OnTopExitMontageCancelled();

    void SetupExitMotionWarpingTarget();

    void SetupEntryMotionWarpingTarget(const FVector& TargetLocation, const FQuat& TargetRotation);

    /** 진입 몽타주 완료 콜백 */
    UFUNCTION()
    void OnEntryMontageCompleted();
    
    /** 진입 몽타주 취소/중단 콜백 */
    UFUNCTION()
    void OnEntryMontageCancelled();

    void StartLadderExitMonitoring();

    UFUNCTION()
    void OnLadderEntryInterpolationComplete();

    /** CMC phase 변경 콜백 */
    UFUNCTION()
    void OnLadderPhaseChanged(ECYLadderPhase NewPhase);
    
protected:

    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Animation")
    TObjectPtr<UAnimMontage> TopEntryMontage;

    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Animation")
    TObjectPtr<UAnimMontage> BottomEntryMontage;
    
    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Animation")
    TObjectPtr<UAnimMontage> TopExitMontage;

    /** 상단 탈출 몽타주의 시작 섹션 이름 */
    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Animation")
    FName TopExitMontageStartSection = NAME_None;

    /** 상단 탈출 몽타주 재생 속도 */
    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Animation", meta=(ClampMin="0.1", ClampMax="3.0"))
    float TopExitMontagePlayRate = 1.0f;

    bool bIsPlayingExitMontage = false;
    
private:
    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|MotionWarping")
    FName LadderExitWarpTargetName = FName("LadderExit");

    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|MotionWarping")
    FName LadderEntryWarpTargetName = FName("LadderEntry");

    /** 사다리에서 전방으로 이동할 거리 (cm) */
    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|MotionWarping", meta=(ClampMin="0.0", ClampMax="300.0"))
    float ForwardExitOffset = 40.0f;

    /** 사다리 이탈 판정 여유 거리 (cm) */
    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Exit", meta=(ClampMin="0.0", ClampMax="300.0"))
    float HorizontalDistanceSafetyMargin = 20.0f;

    /** 캐싱된 MovementComponent */
    UPROPERTY(Transient)
    TObjectPtr<UCYCharacterMovementComponent> CachedMovementComponent;

    /** 진입 시 계산된 타겟 위치  */
    UPROPERTY(Transient)
    FVector CachedEntryTargetLocation;

    /** 현재 등반 중인 사다리 */
    UPROPERTY(Transient)
    TObjectPtr<ACYLadderBase> CurrentLadder;

    /** 이탈 모니터링 태스크 */
    UPROPERTY(Transient)
    TObjectPtr<UCYAbilityTask_WaitForLadderExit> ExitMonitorTask;

    UPROPERTY(EditDefaultsOnly, Category="CY|Ladder|Debug")
    bool bShowDebugWarpTarget = true;
    
};
