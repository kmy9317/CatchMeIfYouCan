#include "CYCharacterMovementComponent.h"

#include "Actors/CYLadderBase.h"
#include "Character/CYCharacterBase.h"
#include "Character/CYStatusGameplayTags.h"
#include "GameFramework/Character.h"

namespace
{
	static int8 ComputeClimbInputFromAccel(const FVector& InAccel, const ACharacter* Character)
	{
		if (!Character || !Character->Controller)
		{
			return 0;
		}

		const FRotator Ctrl = Character->Controller->GetControlRotation();
		const FRotator YawOnly(0.f, Ctrl.Yaw, 0.f);
		const FVector ControllerForward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);

		const FVector Accel2D = InAccel.GetSafeNormal2D();
		const float RawAxis = FVector::DotProduct(Accel2D, ControllerForward);

		constexpr float DeadZone = 0.2f;

		if (RawAxis > DeadZone)
		{
			return 1;
		}
		if (RawAxis < -DeadZone)
		{
			return -1;
		}
		return 0;
	}
}

UCYCharacterMovementComponent::UCYCharacterMovementComponent()
{
	SetNetworkMoveDataContainer(CYNetworkMoveDataContainer);
}

void UCYCharacterMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	CYCharacterOwner = Cast<ACYCharacterBase>(GetOwner());
}

void UCYCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	// 서버에서 클라이언트 패킷을 처리할 때 사다리 데이터 적용
	if (GetNetMode() != NM_Client)
	{
		const FCYCharacterNetworkMoveData* MoveData = static_cast<const FCYCharacterNetworkMoveData*>(GetCurrentNetworkMoveData());

		if (MoveData)
		{
			// 시작 요청이 있으면 서버에서 동일한 Request로 climbing 시작
			if (MoveData->bNetworkHasClimbStartRequest)
			{
				FCYLadderStartRequest ServerRequest;
				ServerRequest.LadderActor = MoveData->NetworkLadderActor;
				ServerRequest.EntryType = MoveData->NetworkEntryType;
				ServerRequest.InitialAttachSpot = MoveData->NetworkInitialAttachSpot;
				ServerRequest.LadderStandOff = MoveData->NetworkLadderStandOff;
				ServerRequest.bUseInterpolation = MoveData->bNetworkUseInterpolation;
				ServerRequest.bClimbUp = MoveData->bNetworkClimbUp;
				ServerRequest.LadderStart = MoveData->NetworkLadderStart;
				ServerRequest.LadderEnd = MoveData->NetworkLadderEnd;
				ServerRequest.LadderFacing = MoveData->NetworkLadderFacing;

				// 서버 검증: 사다리 액터가 유효한지
				if (ServerRequest.LadderActor.IsValid())
				{
					RequestStartClimb(ServerRequest);

					UE_LOG(LogTemp, Log, TEXT("[MoveAutonomous Server] Received ClimbRequest: EntryType=%d AttachSpot=%.1f"),
						ServerRequest.EntryType, ServerRequest.InitialAttachSpot);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[MoveAutonomous Server] Invalid LadderActor in ClimbRequest"));
				}
			}

			// climbing 중이면 ClimbInput 동기화
			if (MoveData->bNetworkWantsToClimb)
			{
				SetClimbInput(MoveData->NetworkClimbInput);
				bClimbInputSetFromNetwork = true;
			}
		}
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

float UCYCharacterMovementComponent::GetMaxSpeed() const
{
	// 사다리 타기 모드 체크
	if (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(CMOVE_Climbing))
	{
		return MaxClimbSpeed;
	}

	// 기본 이동 모드는 부모 클래스 로직 사용
	return Super::GetMaxSpeed();
}

void UCYCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	
	const bool bNowClimbing = (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(CMOVE_Climbing));
	const bool bWasClimbing = (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == static_cast<uint8>(CMOVE_Climbing));

	if (CYCharacterOwner)
	{
		// SimulatedProxy는 제외 (OnRep으로 받음)
		if (CYCharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
		{
			if (bNowClimbing != bWasClimbing)
			{
				CYCharacterOwner->SetIsClimbing(bNowClimbing);
			}
		}
	}
	
	if (bNowClimbing)
	{
		// 캐시 (사다리 "진입"시에만)
		DefaultGravityScale = GravityScale;
		DefaultBrakingFrictionFactor = BrakingFrictionFactor;
		bSavedOrientRotationToMovement = bOrientRotationToMovement;

		// 컨트롤러 회전 사용 여부는 Character 쪽 플래그를 저장/비활성화
		if (CharacterOwner)
		{
			bSavedUseControllerDesiredRotation = CharacterOwner->bUseControllerRotationYaw;
			CharacterOwner->bUseControllerRotationYaw = false;
		}

		GravityScale = 0.f;
		BrakingFrictionFactor = 0.f;
		bOrientRotationToMovement = false;
		Velocity = FVector::ZeroVector;
	}
	else if (bWasClimbing)
	{
		// 캐시 복원 (사다리 "이탈"시에만)
		GravityScale = DefaultGravityScale;
		BrakingFrictionFactor = DefaultBrakingFrictionFactor;
		bOrientRotationToMovement = bSavedOrientRotationToMovement;

		if (CharacterOwner)
		{
			CharacterOwner->bUseControllerRotationYaw = bSavedUseControllerDesiredRotation;
		}

		// 안전장치
		bWantsToClimb = false;
		LadderActor.Reset();
		RailLength = 0.f;
		RailDirection = FVector::UpVector;
		LadderPhase = ECYLadderPhase::None;
	}
}

void UCYCharacterMovementComponent::SetLadderPhase(ECYLadderPhase NewPhase)
{
	if (LadderPhase == NewPhase)
	{
		return;
	}

	const ECYLadderPhase OldPhase = LadderPhase;
	LadderPhase = NewPhase;

	UE_LOG(LogTemp, Warning, TEXT("[%s] LadderPhase: %d -> %d"),
		CharacterOwner && CharacterOwner->HasAuthority() ? TEXT("Server") : TEXT("Client"),
		static_cast<uint8>(OldPhase), static_cast<uint8>(NewPhase));

	const bool bIsReplay = CharacterOwner && CharacterOwner->bClientUpdating;
	if (!bIsReplay)
	{
		OnLadderPhaseChanged.Broadcast(NewPhase);
	}
}

void UCYCharacterMovementComponent::SetClimbInput(int8 NewInput)
{
	ClimbInput = FMath::Clamp(NewInput, static_cast<int8>(-1), static_cast<int8>(1));
}

void UCYCharacterMovementComponent::RequestStartClimb(const FCYLadderStartRequest& Request)
{
	bHasPendingLadderStart = true;
	PendingLadderStartRequest = Request;
}

void UCYCharacterMovementComponent::BeginClimbLadder(AActor* InLadder, const FVector& InStart, const FVector& InEnd, const FVector& InFacing, float InLadderStandOff, float InAttachSpot,  bool bUseInterpolation)
{
	FCYLadderStartRequest Request;
	Request.LadderActor = InLadder;
	Request.LadderStart = InStart;
	Request.LadderEnd = InEnd;
	Request.LadderFacing = InFacing;
	Request.LadderStandOff = InLadderStandOff;
	Request.InitialAttachSpot = InAttachSpot;
	Request.bUseInterpolation = bUseInterpolation;

	// 하위 호환: 직접 시작 (Request 경유 없이)
	BeginClimbFromRequest(Request);
}

void UCYCharacterMovementComponent::EndClimbLadder(bool bStepOffTop)
{
	bWantsToClimb = false;
	
	bIsInterpolatingToLadder = false;
	InterpElapsedTime = 0.f;

	SetLadderPhase(ECYLadderPhase::None);
	ClimbInput = 0;

	// 이탈 판정 캐시 리셋
	BottomExitThreshold = 0.f;
	TopExitThreshold = 0.f;
	MaxAllowedHorizontalDistance = 0.f;
	EntryTargetLocation = FVector::ZeroVector;
	
	// 현재는 간단히 이동 모드만 전환
	// TODO : 상단 탈출 시 추가 처리 가능 (예: 약간의 전방 임펄스)
	if (CharacterOwner && CharacterOwner->GetMovementBase())
	{
		// 바닥이 있으면 걷기 모드로 전환
		SetMovementMode(MOVE_Walking);
	}
	else
	{
		// 바닥이 없으면 낙하 모드로 전환
		SetMovementMode(MOVE_Falling);
	}
}

bool UCYCharacterMovementComponent::IsClimbingLadder() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(CMOVE_Climbing) && bWantsToClimb;
}

void UCYCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	// 사다리 타기 모드 체크
	if (CustomMovementMode == static_cast<uint8>(CMOVE_Climbing))
	{
		// 사다리 물리 처리
		PhysLadder(DeltaTime, Iterations);
		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void UCYCharacterMovementComponent::PhysLadder(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !UpdatedComponent || !LadderActor.IsValid() || !bWantsToClimb)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	switch (LadderPhase)
    {
    case ECYLadderPhase::Entering:
        {
            // 진입 중: 보간만 처리, 레일 회전/이동 제약 없음
            if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
            {
                if (bIsInterpolatingToLadder)
                {
                    UpdateLadderEntryInterpolation(DeltaTime);
                    Velocity = FVector::ZeroVector;
                    return;
                }
                // 몽타주 진입 중: root motion이 처리하므로 레일 제약 없이 velocity만 0
                Velocity = FVector::ZeroVector;
            }

            ApplyRootMotionToVelocity(DeltaTime);

            Iterations++;
            bJustTeleported = false;

            const FVector MoveDelta = Velocity * DeltaTime;
    		// 사다리를 바라보는 회전은 Entering에서도 적용
    		const FQuat DesiredRot = FRotationMatrix::MakeFromXZ(CharToLadderFacing, RailDirection).ToQuat();
    		
            FHitResult Hit;
            SafeMoveUpdatedComponent(MoveDelta, DesiredRot, true, Hit);

            if (Hit.IsValidBlockingHit())
            {
                SlideAlongSurface(MoveDelta, 1.f - Hit.Time, Hit.Normal, Hit, true);
            }
            break;
        }

    case ECYLadderPhase::Climbing:
        {
            // 정상 등반: 레일 기반 이동 + 회전 강제
            if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
            {
            	Velocity = RailDirection * (static_cast<float>(ClimbInput) * MaxClimbSpeed);
            }

            ApplyRootMotionToVelocity(DeltaTime);

            Iterations++;
            bJustTeleported = false;

            const FVector MoveAlongRail = Velocity * DeltaTime;
            const FVector OldLocation = UpdatedComponent->GetComponentLocation();
            const FQuat DesiredRot = FRotationMatrix::MakeFromXZ(CharToLadderFacing, RailDirection).ToQuat();

            FHitResult Hit;
            SafeMoveUpdatedComponent(MoveAlongRail, DesiredRot, true, Hit);

            // Climbing 단계에서만 충돌 시 슬라이드 처리
            if (Hit.IsValidBlockingHit())
            {
                SlideAlongSurface(MoveAlongRail, 1.f - Hit.Time, Hit.Normal, Hit, true);
            }

            // 실제 이동량으로 AttachSpot 갱신
            const FVector NewLocation = UpdatedComponent->GetComponentLocation();
            const FVector ActualDelta = NewLocation - OldLocation;
            const float ActualMoveS = FVector::DotProduct(ActualDelta, RailDirection);
            LadderAttachSpot = FMath::Clamp(LadderAttachSpot + ActualMoveS, 0.f, RailLength);

            if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
            {
                Velocity = (DeltaTime >= KINDA_SMALL_NUMBER) ? (ActualDelta / DeltaTime) : FVector::ZeroVector;
            }

    		// 이탈 조건 체크 (physics tick 기준으로 서버/클라 일관 판정)
    		CheckLadderExitConditions();
            break;
        }

    case ECYLadderPhase::ExitingTop:
    case ECYLadderPhase::ExitingBottom:
        {
            // 탈출 중: root motion 우선, 레일 회전/이동 제약 없음
            if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
            {
                Velocity = FVector::ZeroVector;
            }

            ApplyRootMotionToVelocity(DeltaTime);

            Iterations++;
            bJustTeleported = false;

            const FVector MoveDelta = Velocity * DeltaTime;
            FHitResult Hit;
            // 탈출 중에는 현재 회전 유지 (root motion이 제어)
            SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, Hit);

            if (Hit.IsValidBlockingHit())
            {
                SlideAlongSurface(MoveDelta, 1.f - Hit.Time, Hit.Normal, Hit, true);
            }
            break;
        }

    default:
        break;
    }
	
}

void UCYCharacterMovementComponent::UpdateLadderEntryInterpolation(float DeltaTime)
{
	if (!bIsInterpolatingToLadder)
	{
		return;
	}
    
	InterpElapsedTime += DeltaTime;
    
	// 보간 비율 계산 (0.0 ~ 1.0)
	float Alpha = FMath::Clamp(InterpElapsedTime / LadderEntryInterpDuration, 0.f, 1.f);
    
	// Ease In-Out 곡선 적용
	Alpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, LadderEntryInterpEase);
    
	// 위치 보간
	const FVector CurrentLocation = FMath::Lerp(InterpStartLocation, InterpTargetLocation, Alpha);
    
	// 회전 보간 (Slerp)
	const FQuat CurrentRotation = FQuat::Slerp(InterpStartRotation, InterpTargetRotation, Alpha);

	const FVector Delta = CurrentLocation - UpdatedComponent->GetComponentLocation();

	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, CurrentRotation, /*bSweep=*/true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, /*bHandleImpact=*/true);
	}

	if (Alpha >= 1.f)
	{
		bIsInterpolatingToLadder = false;
		InterpElapsedTime = 0.f;

		SetLadderPhase(ECYLadderPhase::Climbing);
		OnLadderEntryInterpolationComplete.Broadcast();
	}
}

void UCYCharacterMovementComponent::UpdateClimbInputFromAcceleration()
{
	if (LadderPhase != ECYLadderPhase::Climbing)
	{
		ClimbInput = 0;
		return;
	}

	ClimbInput = ComputeClimbInputFromAccel(Acceleration, CharacterOwner);
}

void UCYCharacterMovementComponent::ConsumeAndStartClimb()
{
	if (!bHasPendingLadderStart)
	{
		return;
	}

	BeginClimbFromRequest(PendingLadderStartRequest);

	bHasPendingLadderStart = false;
	PendingLadderStartRequest = FCYLadderStartRequest();
}

void UCYCharacterMovementComponent::BeginClimbFromRequest(const FCYLadderStartRequest& Request)
{
	if (!Request.LadderActor.IsValid() || !UpdatedComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginClimbFromRequest: Invalid Ladder or UpdatedComponent"));
		return;
	}

	LadderActor = Request.LadderActor;
	LadderStart = Request.LadderStart;
	LadderEnd = Request.LadderEnd;
	LadderStandOff = Request.LadderStandOff;

	bWantsToClimb = true;

	RailDirection = (LadderEnd - LadderStart).GetSafeNormal();
	RailLength = (LadderEnd - LadderStart).Size();

	ACYLadderBase* Ladder = Cast<ACYLadderBase>(Request.LadderActor.Get());
	FVector TargetPosition = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;

	if (Ladder)
	{
		Ladder->CalculateEntryTransform(Request.InitialAttachSpot, Request.LadderStandOff, TargetPosition, TargetRotation);
		CharToLadderFacing = Ladder->CalculateCharacterToLadderFacing();
	}

	if (CharToLadderFacing.IsNearlyZero())
	{
		CharToLadderFacing = FVector::ForwardVector;
	}

	LadderAttachSpot = FMath::Clamp(Request.InitialAttachSpot, 0.f, RailLength);

	SetBase(nullptr);
	bJustTeleported = true;

	const bool bShouldInterpolate = Request.bUseInterpolation &&
		(CharacterOwner->HasAuthority() || CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy);

	if (Ladder)
	{
		const float CapsuleHalfHeight = CharacterOwner ? CharacterOwner->GetSimpleCollisionHalfHeight() : 88.0f;

		BottomExitThreshold = Ladder->GetBottomSafetyMargin() + CapsuleHalfHeight;
		TopExitThreshold = Ladder->GetTopSafetyMargin();

		// 진입 타겟 위치 캐시 (수평 거리 기준)
		EntryTargetLocation = TargetPosition;

		// 최대 허용 수평 거리 = 진입 위치에서의 기본 거리 + 안전 마진
		const float BaseDistance = CalculateHorizontalDistanceFromRail(TargetPosition);

		// 안전 마진은 고정값으로 사용 (기존 GA에서 20.0f 사용)
		constexpr float HorizontalSafetyMargin = 20.0f;
		MaxAllowedHorizontalDistance = BaseDistance + HorizontalSafetyMargin;
	}

	if (bShouldInterpolate)
	{
		bIsInterpolatingToLadder = true;
		InterpStartLocation = UpdatedComponent->GetComponentLocation();
		InterpStartRotation = UpdatedComponent->GetComponentQuat();
		InterpTargetLocation = TargetPosition;
		InterpTargetRotation = TargetRotation.Quaternion();
		InterpElapsedTime = 0.f;
	}

	SetLadderPhase(ECYLadderPhase::Entering);
	SetMovementMode(MOVE_Custom, CMOVE_Climbing);
}

void UCYCharacterMovementComponent::CheckLadderExitConditions()
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const FVector CharacterLocation = UpdatedComponent->GetComponentLocation();

	// 수평 거리 이탈 체크
	const float CurrentHorizontalDistance = CalculateHorizontalDistanceFromRail(CharacterLocation);
	if (MaxAllowedHorizontalDistance > 0.f && CurrentHorizontalDistance > MaxAllowedHorizontalDistance)
	{
		EndClimbLadder(false);
		return;
	}

	// 상단 이탈: 상단 임계값 + 상향 입력
	const float TopThreshold = RailLength - TopExitThreshold;
	if (LadderAttachSpot >= TopThreshold && ClimbInput > 0)
	{
		SetLadderPhase(ECYLadderPhase::ExitingTop);
		return;
	}
	
	// 하단 이탈: 하단 임계값 + 하향 입력
	if (LadderAttachSpot <= BottomExitThreshold && ClimbInput < 0)
	{
		UE_LOG(LogTemp, Warning,
TEXT("[%s] BottomCheck Attach=%.2f Threshold=%.2f Input=%d Phase=%d"),
CharacterOwner->HasAuthority() ? TEXT("Server") : TEXT("Client"),
LadderAttachSpot,
BottomExitThreshold,
ClimbInput,
(int32)LadderPhase);
		SetClimbInput(0);
		Velocity = FVector::ZeroVector;
		SetLadderPhase(ECYLadderPhase::ExitingBottom);
		return;
	}
}

float UCYCharacterMovementComponent::CalculateHorizontalDistanceFromRail(const FVector& Location) const
{
	const FVector ToLocation = Location - LadderStart;
	const float ProjectedDistance = FVector::DotProduct(ToLocation, RailDirection);
	const float ClampedDistance = FMath::Clamp(ProjectedDistance, 0.0f, RailLength);
	const FVector ClosestPointOnRail = LadderStart + (RailDirection * ClampedDistance);

	return FVector::Dist(Location, ClosestPointOnRail);
}

void UCYCharacterMovementComponent::NotifyEntryComplete()
{
	if (LadderPhase == ECYLadderPhase::Entering)
	{
		SetLadderPhase(ECYLadderPhase::Climbing);
	}
}

void UCYCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// FLAG_Custom_0 비트 체크 (사다리 타기 의도)
	bWantsToClimb = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

void UCYCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// replay 중에는 PrepMoveFor()에서 복원된 ClimbInput을 유지
	if (CharacterOwner && CharacterOwner->bClientUpdating)
	{
		// replay 중 pending request가 있으면 소비 (replay에서도 시작 프레임 재현)
		if (bHasPendingLadderStart)
		{
			ConsumeAndStartClimb();
		}
		return;
	}

	if (bHasPendingLadderStart)
	{
		ConsumeAndStartClimb();
	}

	// 새로운 move에서만 입력 계산
	if (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(CMOVE_Climbing))
	{
		// 네트워크에서 이미 설정된 경우 재계산 스킵
		if (bClimbInputSetFromNetwork)
		{
			bClimbInputSetFromNetwork = false;
		}
		else
		{
			UpdateClimbInputFromAcceleration();
		}
	}
	else
	{
		ClimbInput = 0;
		bClimbInputSetFromNetwork = false;
	}
}

FNetworkPredictionData_Client* UCYCharacterMovementComponent::GetPredictionData_Client() const
{
	// 필요할 때만 할당
	if (ClientPredictionData == nullptr)
	{
		UCYCharacterMovementComponent* MutableThis = const_cast<UCYCharacterMovementComponent*>(this);
		// 예측 데이터 생성
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_CY(*this);

	}

	return ClientPredictionData;
}

FSavedMove_CY::FSavedMove_CY()
	: bWantsToClimb(false)
	, bSavedHasClimbStartRequest(false)
{
}

void FSavedMove_CY::Clear()
{
	Super::Clear();
	bWantsToClimb = false;
	SavedLadderAttachSpot = 0.f;
	SavedLadderPhase = 0;
	SavedClimbInput = 0;
	bSavedHasClimbStartRequest = false;
	SavedClimbStartRequest = FCYLadderStartRequest();

	// climbing runtime
	SavedLadderActor.Reset();
	SavedRailDirection = FVector::UpVector;
	SavedRailLength = 0.f;
	SavedCharToLadderFacing = FVector::ForwardVector;
	SavedLadderStart = FVector::ZeroVector;
	SavedLadderEnd = FVector::ZeroVector;
	SavedLadderStandOff = 0.f;

	SavedBottomExitThreshold = 0.f;
	SavedTopExitThreshold = 0.f;
	SavedMaxAllowedHorizontalDistance = 0.f;
}

uint8 FSavedMove_CY::GetCompressedFlags() const
{
	// 부모 클래스의 플래그 가져오기
	uint8 Result = Super::GetCompressedFlags();

	// 사다리 타기 의도를 FLAG_Custom_0 비트로 설정
	if (bWantsToClimb)
	{
		Result |= FLAG_Custom_0;  // 비트 OR 연산으로 플래그 설정
	}

	return Result;
}

bool FSavedMove_CY::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	// NewMove를 FSavedMove_CY로 캐스팅
	const FSavedMove_CY* NewCY = static_cast<const FSavedMove_CY*>(NewMove.Get());

	if (bWantsToClimb || NewCY->bWantsToClimb)
	{
		return false;
	}

	// climb 시작 요청이 있는 move는 combine 금지
	if (bSavedHasClimbStartRequest || NewCY->bSavedHasClimbStartRequest)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_CY::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	// 부모 클래스의 기본 데이터 저장 (위치, 속도, 회전 등)
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	// 커스텀 MovementComponent 캐스팅
	if (const auto* CYMovementComponent = Cast<UCYCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		// 현재 사다리 타는 중인지 저장
		bWantsToClimb = CYMovementComponent->bWantsToClimb;
		SavedLadderAttachSpot = CYMovementComponent->GetLadderAttachSpot();
		SavedLadderPhase = static_cast<uint8>(CYMovementComponent->GetLadderPhase());
		if (bWantsToClimb && CYMovementComponent->GetLadderPhase() == ECYLadderPhase::Climbing)
		{
			SavedClimbInput = ComputeClimbInputFromAccel(NewAccel, Character);
		}
		else
		{
			SavedClimbInput = 0;
		}
		bSavedHasClimbStartRequest = CYMovementComponent->HasPendingClimbRequest();
		if (bSavedHasClimbStartRequest)
		{
			SavedClimbStartRequest = CYMovementComponent->GetPendingClimbRequest();
		}

		// climbing runtime 상태 (climbing 중일 때만 저장)
		if (bWantsToClimb)
		{
			SavedLadderActor = CYMovementComponent->GetLadderActor();
			SavedRailDirection = CYMovementComponent->GetRailDirection();
			SavedRailLength = CYMovementComponent->GetRailLength();
			SavedCharToLadderFacing = CYMovementComponent->GetCharToLadderFacing();
			SavedLadderStart = CYMovementComponent->GetLadderStart();
			SavedLadderEnd = CYMovementComponent->GetLadderEnd();
			SavedLadderStandOff = CYMovementComponent->GetLadderStandOff();

			// 이탈 임계값
			SavedBottomExitThreshold = CYMovementComponent->GetBottomExitThreshold();
			SavedTopExitThreshold = CYMovementComponent->GetTopExitThreshold();
			SavedMaxAllowedHorizontalDistance = CYMovementComponent->GetMaxAllowedHorizontalDistance();
		}

		// 디버그 로그
		if (bWantsToClimb || bSavedHasClimbStartRequest)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[SetMoveFor] HasRequest=%d Phase=%d AttachSpot=%.1f ClimbInput=%d Ladder=%s"),
				bSavedHasClimbStartRequest, SavedLadderPhase, SavedLadderAttachSpot, SavedClimbInput,
				*GetNameSafe(SavedLadderActor.Get()));
		}
	}
}

void FSavedMove_CY::PrepMoveFor(ACharacter* Character)
{
	// 부모 클래스의 기본 복원 (위치, 속도, 회전 등)
	Super::PrepMoveFor(Character);

	if (UCYCharacterMovementComponent* CYMovementComponent = Cast<UCYCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		CYMovementComponent->bWantsToClimb = bWantsToClimb;
		CYMovementComponent->SetLadderAttachSpot(SavedLadderAttachSpot);
		CYMovementComponent->SetLadderPhase(static_cast<ECYLadderPhase>(SavedLadderPhase));
		CYMovementComponent->SetClimbInput(SavedClimbInput);
		if (bSavedHasClimbStartRequest)
		{
			CYMovementComponent->RequestStartClimb(SavedClimbStartRequest);
		}

		// climbing runtime 상태 복원 (climbing 중인 move만)
		if (bWantsToClimb)
		{
			CYMovementComponent->SetLadderActorForReplay(SavedLadderActor.Get());
			CYMovementComponent->SetRailDirection(SavedRailDirection);
			CYMovementComponent->SetRailLength(SavedRailLength);
			CYMovementComponent->SetCharToLadderFacing(SavedCharToLadderFacing);
			CYMovementComponent->SetLadderStart(SavedLadderStart);
			CYMovementComponent->SetLadderEnd(SavedLadderEnd);
			CYMovementComponent->SetLadderStandOff(SavedLadderStandOff);

			CYMovementComponent->SetBottomExitThreshold(SavedBottomExitThreshold);
			CYMovementComponent->SetTopExitThreshold(SavedTopExitThreshold);
			CYMovementComponent->SetMaxAllowedHorizontalDistance(SavedMaxAllowedHorizontalDistance);
		}
	}
}

FNetworkPredictionData_Client_CY::FNetworkPredictionData_Client_CY(const UCharacterMovementComponent& ClientMovement)
	: FNetworkPredictionData_Client_Character(ClientMovement)
{
	
}

FSavedMovePtr FNetworkPredictionData_Client_CY::AllocateNewMove()
{
	// 커스텀 SavedMove 할당 (스마트 포인터로 반환)
	return FSavedMovePtr(new FSavedMove_CY());
}


