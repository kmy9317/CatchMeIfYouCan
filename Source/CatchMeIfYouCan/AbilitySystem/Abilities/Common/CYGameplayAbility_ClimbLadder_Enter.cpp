// Fill out your copyright notice in the Description page of Project Settings.


#include "CYGameplayAbility_ClimbLadder_Enter.h"

#include "AbilitySystemComponent.h"
#include "CYLogChannels.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/CYAbilityGameplayTags.h"
#include "Character/CYStatusGameplayTags.h"
#include "AbilitySystem/Abilities/Tasks/CYAbilityTask_WaitForLadderExit.h"
#include "Actors/CYLadderBase.h"
#include "Character/Components/CYCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UCYGameplayAbility_ClimbLadder_Enter::UCYGameplayAbility_ClimbLadder_Enter()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // GameplayEvent로만 활성화 (상호작용 또는 자동 그랩)
    ActivationPolicy = ECYAbilityActivationPolicy::Manual;

    AbilityTags.AddTag(CYGameplayTags::Ability_Action_Climbing);
    ActivationOwnedTags.AddTag(CYGameplayTags::Status_Movement_Climbing);

    ActivationBlockedTags.AddTag(CYGameplayTags::Status_Movement_Climbing);
    
    // TODO: 적절한 취소 태그
    // CancelAbilitiesWithTag.AddTag(CYGameplayTags::);
    
    // GameplayEvent 트리거 설정
    FAbilityTriggerData TriggerData;
    TriggerData.TriggerTag = CYGameplayTags::Ability_Action_Climbing;
    TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(TriggerData);
}

void UCYGameplayAbility_ClimbLadder_Enter::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    
    // 캐릭터 검증
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        return;
    }

    // MovementComponent 캐싱
    CachedMovementComponent = Cast<UCYCharacterMovementComponent>(Character->GetCharacterMovement());
    if (!CachedMovementComponent)
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        return;
    }

    // 이미 사다리 타는 중인지 체크
    if (CachedMovementComponent->IsClimbingLadder())
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        return;
    }
    
    // 사다리 정보 추출
    ACYLadderBase* TempLadder = nullptr;
    FVector LadderBottom, LadderTop, LadderFacing;
    float LadderStandOff = 0.0f;
    float InitialRailParameter = 0.0f;
    bool bIsClimbingUp = true;

    if (!CanExtractLadderInfo(TriggerEventData, TempLadder, LadderBottom, LadderTop, LadderFacing, LadderStandOff, InitialRailParameter, bIsClimbingUp))
    {
        UE_LOG(LogCY, Warning, TEXT("ClimbLadder_Enter: Failed to extract ladder info"));
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        return;
    }

    CurrentLadder = TempLadder;

    if (!CurrentLadder || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
        return;
    }
    
    FVector TargetLocation;
    FRotator TargetRotation;
    CurrentLadder->CalculateEntryTransform(InitialRailParameter, LadderStandOff, TargetLocation, TargetRotation);
    ELadderEntryType CurrentEntryType = CurrentLadder->GetPlayerEntryType(Character);
    CachedEntryTargetLocation = TargetLocation;

    bool bUseInterpolation = false;
    if (!CurrentLadder->ShouldUseEntryMontage())
    {
        // 몽타주 사용 안 함 → 항상 Interpolate
        bUseInterpolation = true;
    }
    else
    {
        // 몽타주 사용 → Middle/None만 Interpolate
        bUseInterpolation = (CurrentEntryType == ELadderEntryType::Middle || CurrentEntryType == ELadderEntryType::None);
    }

    FCYLadderStartRequest ClimbRequest;
    ClimbRequest.LadderActor = CurrentLadder;
    ClimbRequest.LadderStart = LadderBottom;
    ClimbRequest.LadderEnd = LadderTop;
    ClimbRequest.LadderFacing = LadderFacing;
    ClimbRequest.LadderStandOff = LadderStandOff;
    ClimbRequest.InitialAttachSpot = InitialRailParameter;
    ClimbRequest.bUseInterpolation = bUseInterpolation;
    ClimbRequest.bClimbUp = bIsClimbingUp;
    ClimbRequest.EntryType = static_cast<uint8>(CurrentEntryType);
    const bool bIsLocallyControlled = CurrentActorInfo->IsLocallyControlled();
    
    if (bIsLocallyControlled)
    {
        // 로컬 클라이언트: 예측 시작 (SavedMove → NetworkMoveData로 서버에 전달)
        // 리슨 서버 호스트: 직접 시작 (네트워크 경로 불필요)
        CachedMovementComponent->RequestStartClimb(ClimbRequest);
    }

    if (bUseInterpolation)
    {
        CachedMovementComponent->OnLadderEntryInterpolationComplete.AddDynamic(
            this, &ThisClass::OnLadderEntryInterpolationComplete);
        return;  
    }

    UAnimMontage* EntryMontage = nullptr;
    if (CurrentLadder->ShouldUseEntryMontage())
    {
        EntryMontage = GetEntryMontageForType(CurrentEntryType);
    }
    if (EntryMontage)
    {
        SetupEntryMotionWarpingTarget(TargetLocation, FQuat(TargetRotation));
        
        if (UAbilityTask_PlayMontageAndWait* EntryTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
               this, TEXT("LadderEntry"), EntryMontage, 1.0f, NAME_None, true))
        {
            if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
            {
                ASC->AddLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
            }
            
            EntryTask->OnCompleted.AddDynamic(this, &ThisClass::OnEntryMontageCompleted);
            EntryTask->OnBlendOut.AddDynamic(this, &ThisClass::OnEntryMontageCompleted);
            EntryTask->OnInterrupted.AddDynamic(this, &ThisClass::OnEntryMontageCancelled);
            EntryTask->OnCancelled.AddDynamic(this, &ThisClass::OnEntryMontageCancelled);
            EntryTask->ReadyForActivation();
        }
    }
    else
    {
        if (CachedMovementComponent)
        {
            CachedMovementComponent->NotifyEntryComplete();
        }
        StartLadderExitMonitoring();
    }
}

bool UCYGameplayAbility_ClimbLadder_Enter::CanExtractLadderInfo(const FGameplayEventData* TriggerEventData, ACYLadderBase*& OutLadder, FVector& OutBottomLocation,FVector& OutTopLocation,FVector& OutFacingDirection, float& OutLadderStandOff, float& OutInitialRailParameter, bool& OutIsClimbingUp) const
{
    // 이벤트 데이터 검증
    if (!TriggerEventData || !TriggerEventData->Target)
    {
        UE_LOG(LogCY, Warning, TEXT("ExtractLadderInfo: No event data or target"));
        return false;
    }

    // 사다리 캐스팅
    OutLadder = Cast<ACYLadderBase>(const_cast<AActor*>(TriggerEventData->Target.Get()));
    if (!OutLadder)
    {
        UE_LOG(LogCY, Warning, TEXT("ExtractLadderInfo: Target is not a ladder"));
        return false;
    }

    // 캐릭터 확인
    ACharacter* Character = Cast<ACharacter>(const_cast<AActor*>(TriggerEventData->Instigator.Get()));
    if (!Character)
    {
        UE_LOG(LogCY, Warning, TEXT("ExtractLadderInfo: No valid character"));
        return false;
    }

    // 사다리 기본 정보
    OutBottomLocation = OutLadder->GetBottomWorldLocation();
    OutTopLocation = OutLadder->GetTopWorldLocation();
    OutFacingDirection = OutLadder->GetHorizontalFacingDirection();
    OutLadderStandOff = OutLadder->GetLadderStandOffDistance();

    // 진입 타입 확인
    ELadderEntryType EntryType = OutLadder->GetPlayerEntryType(Character);
    
    // None이면 자동 그랩으로 간주
    if (EntryType == ELadderEntryType::None)
    {
        UE_LOG(LogCY, Log, TEXT("ExtractLadderInfo: No entry type found, treating as auto-grab (Middle)"));
        EntryType = ELadderEntryType::Middle;
    }

    // 등반 방향 결정 (EventMagnitude 우선, 없으면 사다리에서 결정)
    if (FMath::Abs(TriggerEventData->EventMagnitude) > KINDA_SMALL_NUMBER)
    {
        OutIsClimbingUp = TriggerEventData->EventMagnitude > 0;
    }
    else
    {
        OutLadder->DetermineClimbDirection(EntryType, Character, OutIsClimbingUp);
    }

    // 초기 레일 위치 계산
    OutInitialRailParameter = OutLadder->CalculateInitialRailParameter(EntryType, Character);
    
    return true;
}

UAnimMontage* UCYGameplayAbility_ClimbLadder_Enter::GetEntryMontageForType(ELadderEntryType EntryType) const
{
    switch (EntryType)
    {
    case ELadderEntryType::Top:
        return TopEntryMontage;
            
    case ELadderEntryType::Bottom:
        return BottomEntryMontage;
    default:
        return nullptr;
    }
}

void UCYGameplayAbility_ClimbLadder_Enter::HandleLadderExitFromTop()
{
    UE_LOG(LogCY, Log, TEXT("ClimbLadder_Enter: Exiting from top"));
    
    if (!CachedMovementComponent)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    if (CurrentLadder)
    {
        if (!CurrentLadder->ShouldUseExitMontage())
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
            return;
        }
    }
    
    // 루트 모션 활성화 플래그 설정
    bIsPlayingExitMontage = true;
    
    if (!TopExitMontage)
    {
        UE_LOG(LogCY, Warning, TEXT("ExecuteExitTop: No TopExitMontage assigned - ending climb directly"));
        CachedMovementComponent->EndClimbLadder(true);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    SetupExitMotionWarpingTarget();

    if (UAbilityTask_PlayMontageAndWait* ExitMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, TEXT("LadderTopExit"), TopExitMontage,TopExitMontagePlayRate ,TopExitMontageStartSection ,true,1.0f,0.0f,true))
    {
        ExitMontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnTopExitMontageCompleted);
        ExitMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnTopExitMontageCompleted);
        ExitMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnTopExitMontageCancelled);
        ExitMontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnTopExitMontageCancelled);
        ExitMontageTask->ReadyForActivation();

        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ASC->AddLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
        }
    }
    else
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
    
}

void UCYGameplayAbility_ClimbLadder_Enter::SetupExitMotionWarpingTarget()
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        return;
    }

    if (!CurrentLadder)
    {
        return;
    }

    UMotionWarpingComponent* MW = Character->FindComponentByClass<UMotionWarpingComponent>();
    if (!MW)
    {
        UE_LOG(LogCY, Warning, TEXT("SetupMotionWarpingTarget: No MotionWarpingComponent"));
        return;
    }

    // 1) 목표 착지 지점 계산
    const FVector LadderTop = CurrentLadder->GetTopWorldLocation();
    const FVector StepDir   = -CurrentLadder->GetHorizontalFacingDirection();  // 사다리 → 플랫폼 방향
    const float   StepAhead = ForwardExitOffset;  // 한 발 앞으로 (설정 가능)
    const float   TraceUp   = 60.f;   // 위에서 시작 (여유)
    const float   TraceDown = 200.f;  // 아래로 탐지 길이

    const FVector TraceStart = LadderTop + StepDir * StepAhead + FVector(0, 0, TraceUp);
    const FVector TraceEnd   = TraceStart - FVector(0, 0, TraceDown);

    // 2) 바닥 탐지
    FHitResult Hit;
    FCollisionQueryParams Q(TEXT("LadderTopExitTrace"), false, Character);
    Q.AddIgnoredActor(CurrentLadder.Get());  // 사다리 무시
    
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_WorldStatic, Q
    );
    FVector LandingPos = (bHit ? Hit.ImpactPoint : LadderTop);
    const FQuat LandingRot = FRotationMatrix::MakeFromX(StepDir).ToQuat();

    // 4) 모션워핑 타겟 등록
    FMotionWarpingTarget WarpTarget;
    WarpTarget.Name = LadderExitWarpTargetName; 
    WarpTarget.Location = LandingPos;
    WarpTarget.Rotation = LandingRot.Rotator();
    
    MW->AddOrUpdateWarpTarget(WarpTarget);

    if (bShowDebugWarpTarget)
    {
        constexpr float Duration = 100.f;
        // 사다리 상단 (파랑)
        DrawDebugSphere(GetWorld(), LadderTop, 50.0f, 12, FColor::Blue, false, Duration, 0, 3.0f);
        
        // Trace 시작점 (노랑)
        DrawDebugSphere(GetWorld(), TraceStart, 30.0f, 12, FColor::Yellow, false, Duration, 0, 2.0f);
        
        // 최종 착지 위치 (녹색)
        DrawDebugSphere(GetWorld(), LandingPos, 60.0f, 12, FColor::Green, false, Duration, 0, 4.0f);
    }
}

void UCYGameplayAbility_ClimbLadder_Enter::SetupEntryMotionWarpingTarget(const FVector& TargetLocation, const FQuat& TargetRotation)
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        return;
    }

    UMotionWarpingComponent* MW = Character->FindComponentByClass<UMotionWarpingComponent>();
    if (!MW)
    {
        UE_LOG(LogCY, Warning, TEXT("SetupEntryMotionWarpingTarget: No MotionWarpingComponent"));
        return;
    }

    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
    
    // Mesh Root는 발 위치이므로 캡슐 중심에서 반높이만큼 아래
    const FVector RailDirection = CurrentLadder->GetClimbingDirection();
    const FVector AdjustedLocation = TargetLocation - RailDirection * CapsuleHalfHeight;
    
    // Motion Warping 타겟 등록
    FMotionWarpingTarget WarpTarget;
    WarpTarget.Name = LadderEntryWarpTargetName;
    WarpTarget.Location = AdjustedLocation;
    WarpTarget.Rotation = TargetRotation.Rotator();
    
    MW->AddOrUpdateWarpTarget(WarpTarget);

    if (bShowDebugWarpTarget)
    {
        constexpr float Duration = 5.0f;
        
        // Capsule 중심 (시안)
        DrawDebugSphere(GetWorld(), TargetLocation, 60.0f, 12, FColor::Cyan, false, Duration, 0, 4.0f);
        
        // Mesh Root 위치 (녹색)
        DrawDebugSphere(GetWorld(), AdjustedLocation, 50.0f, 12, FColor::Green, false, Duration, 0, 4.0f);
        
        // 레일 방향 표시 (노란색)
        DrawDebugDirectionalArrow(GetWorld(), TargetLocation, 
            TargetLocation - RailDirection * CapsuleHalfHeight, 
            3.0f, FColor::Yellow, false, Duration, 0, 2.0f);
            
        // 캐릭터 정면 방향 (빨간색)
        DrawDebugDirectionalArrow(GetWorld(), AdjustedLocation, 
            AdjustedLocation + TargetRotation.GetForwardVector() * 100.f, 
            5.0f, FColor::Red, false, Duration, 0, 3.0f);
    }
}

void UCYGameplayAbility_ClimbLadder_Enter::OnEntryMontageCompleted()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
    }
    
    if (!CachedMovementComponent || !CurrentLadder)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    
    CachedMovementComponent->NotifyEntryComplete();
    
    
    StartLadderExitMonitoring();
}

void UCYGameplayAbility_ClimbLadder_Enter::OnEntryMontageCancelled()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
    }
    
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCYGameplayAbility_ClimbLadder_Enter::StartLadderExitMonitoring()
{
    if (!CachedMovementComponent || !CurrentLadder)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }
    
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // 중복 바인딩 방지: 이미 바인딩되어 있으면 먼저 제거
    CachedMovementComponent->OnLadderPhaseChanged.RemoveDynamic(this, &ThisClass::OnLadderPhaseChanged);
    
    // CMC의 phase 변경 델리게이트 구독
    CachedMovementComponent->OnLadderPhaseChanged.AddDynamic(this, &ThisClass::OnLadderPhaseChanged);
}

void UCYGameplayAbility_ClimbLadder_Enter::HandleLadderExitFromBottom()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCYGameplayAbility_ClimbLadder_Enter::HandleLadderClimbingCancelled()
{
    UE_LOG(LogCY, Log, TEXT("ClimbLadder_Enter: Climbing cancelled"));

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCYGameplayAbility_ClimbLadder_Enter::OnTopExitMontageCompleted()
{
    UE_LOG(LogCY, Log, TEXT("ClimbLadder_Enter: Top exit montage completed"));

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
    }
    
    bIsPlayingExitMontage = false;

    if (CachedMovementComponent && CachedMovementComponent->IsClimbingLadder())
    {
        CachedMovementComponent->EndClimbLadder(true);
    }

    // 어빌리티 정상 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UCYGameplayAbility_ClimbLadder_Enter::OnTopExitMontageCancelled()
{
    UE_LOG(LogCY, Warning, TEXT("ClimbLadder_Enter: Top exit montage cancelled"));

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
    }
    
    bIsPlayingExitMontage = false;

    // 어빌리티 취소
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UCYGameplayAbility_ClimbLadder_Enter::OnLadderEntryInterpolationComplete()
{
    if (CachedMovementComponent)
    {
        CachedMovementComponent->OnLadderEntryInterpolationComplete.RemoveDynamic(
            this, &ThisClass::OnLadderEntryInterpolationComplete);

        CachedMovementComponent->NotifyEntryComplete();
    }
    
    StartLadderExitMonitoring();
}

void UCYGameplayAbility_ClimbLadder_Enter::OnLadderPhaseChanged(ECYLadderPhase NewPhase)
{
    switch (NewPhase)
    {
    case ECYLadderPhase::ExitingTop:
        HandleLadderExitFromTop();
        break;

    case ECYLadderPhase::ExitingBottom:
        HandleLadderExitFromBottom();
        break;

    case ECYLadderPhase::None:
        // 외부 요인(점프 등)으로 climbing이 종료됨
        HandleLadderClimbingCancelled();
        break;

    default:
        break;
    }
}

void UCYGameplayAbility_ClimbLadder_Enter::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // 델리게이트 정리
    if (CachedMovementComponent)
    {
        CachedMovementComponent->OnLadderEntryInterpolationComplete.RemoveDynamic(
            this, &ThisClass::OnLadderEntryInterpolationComplete);

        CachedMovementComponent->OnLadderPhaseChanged.RemoveDynamic(
            this, &ThisClass::OnLadderPhaseChanged);
    }
    
    // 태스크 정리
    if (ExitMonitorTask)
    {
        ExitMonitorTask->EndTask();
        ExitMonitorTask = nullptr;
    }

    // 사다리 참조 정리
    CurrentLadder = nullptr;
    bIsPlayingExitMontage = false;

    // 애니메이션 몽타주 종료 직전 점프시 몽타주 제거 안되는 현상 때문에 어빌리티 종료시 수동 제거
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder);
    }
    
    // 안전 체크: 아직 사다리 타는 중이면 종료
    if (CachedMovementComponent && CachedMovementComponent->IsClimbingLadder())
    {
        UE_LOG(LogCY, Warning, TEXT("ClimbLadder_Enter: Force ending ladder climb in EndAbility"));
        CachedMovementComponent->EndClimbLadder(false);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
