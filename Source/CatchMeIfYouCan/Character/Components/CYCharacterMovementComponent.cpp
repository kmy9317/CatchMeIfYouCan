#include "CYCharacterMovementComponent.h"

#include "Actors/CYLadderBase.h"
#include "Character/CYCharacterBase.h"
#include "Character/CYStatusGameplayTags.h"
#include "GameFramework/Character.h"

UCYCharacterMovementComponent::UCYCharacterMovementComponent()
{
	      
}

void UCYCharacterMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	CYCharacterOwner = Cast<ACYCharacterBase>(GetOwner());
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
	}
}

void UCYCharacterMovementComponent::BeginClimbLadder(AActor* InLadder, const FVector& InStart, const FVector& InEnd, const FVector& InFacing, float InLadderStandOff, float InAttachSpot,  bool bUseInterpolation)
{
	if (!IsValid(InLadder) || !UpdatedComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginClimbLadder: Invalid Ladder or UpdatedComponent"));
		return;
	}

	LadderActor = InLadder;      
	LadderStart = InStart;    
	LadderEnd   = InEnd;
	LadderStandOff = InLadderStandOff;

	bWantsToClimb = true;
	
	// 레일 방향/길이 계산
	// RailDirection: 사다리 레일의 정규화된 방향 벡터 (입력 투영에 사용)
	RailDirection = (LadderEnd - LadderStart).GetSafeNormal();

	// RailLength: 사다리 레일의 총 길이 (cm)
	RailLength = (LadderEnd - LadderStart).Size();

	ACYLadderBase* Ladder = Cast<ACYLadderBase>(InLadder);
	FVector TargetPosition;
	FRotator TargetRotation;
	
	if (Ladder)
	{
		Ladder->CalculateEntryTransform(InAttachSpot,InLadderStandOff,TargetPosition,TargetRotation);
		CharToLadderFacing = Ladder->CalculateCharacterToLadderFacing();
	}

	// 안전성 체크: LadderFacing이 0벡터면 기본값 사용
	if (CharToLadderFacing.IsNearlyZero())
	{
		CharToLadderFacing = FVector::ForwardVector;
	}
	
	LadderAttachSpot = FMath::Clamp(InAttachSpot, 0.f, RailLength);

	SetBase(nullptr);         
	//bJustTeleported = true;

	const bool bShouldInterpolate = bUseInterpolation && 
		(CharacterOwner->HasAuthority() || CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy);

	if (bShouldInterpolate)
	{
		bIsInterpolatingToLadder = true;
		InterpStartLocation = UpdatedComponent->GetComponentLocation();
		InterpStartRotation = UpdatedComponent->GetComponentQuat();
		InterpTargetLocation = TargetPosition;
		InterpTargetRotation = TargetRotation.Quaternion();
		InterpElapsedTime = 0.f;
	}

	SetMovementMode(MOVE_Custom, CMOVE_Climbing);
}

void UCYCharacterMovementComponent::EndClimbLadder(bool bStepOffTop)
{
	bWantsToClimb = false;
	
	bIsInterpolatingToLadder = false;
	InterpElapsedTime = 0.f;
	
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

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		// 보간 중이면 보간만 처리
		if (bIsInterpolatingToLadder)
		{
			UpdateLadderEntryInterpolation(DeltaTime);
			Velocity = FVector::ZeroVector;  // 보간 중에는 속도 0
			return;
		}

		FRotator ControlRot = FRotator::ZeroRotator;
		if (CharacterOwner && CharacterOwner->Controller)
		{
			const FRotator Ctrl = CharacterOwner->Controller->GetControlRotation();
			ControlRot = FRotator(0.f, Ctrl .Yaw, 0.f);
		}
	
		const FVector ControllerForward = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
	
		// 1) 입력 → +1/0/-1 
		float InputAxis = FVector::DotProduct(Acceleration.GetSafeNormal2D(), ControllerForward); // 크기 무시, 부호만
		constexpr float DeadZone = 0.2f;
	
		if (InputAxis > DeadZone)
		{
			InputAxis = 1.f;
		}
		else if (InputAxis < -DeadZone)
		{
			InputAxis = -1.f;
		}
		else
		{
			InputAxis = 0.f;
		}
 
		// 2) 목표 속도(축 방향) 구성
		Velocity = RailDirection * (InputAxis * MaxClimbSpeed);
	}

	ApplyRootMotionToVelocity(DeltaTime);

	Iterations++;
	bJustTeleported = false;
 
    // 3) 먼저 "축 방향 이동"만 스윕으로 수행 (충돌 고려)
    const FVector MoveAlongRail = Velocity * DeltaTime;
 
    const FVector OldLocation = UpdatedComponent->GetComponentLocation();
    const FQuat   DesiredRot  = FRotationMatrix::MakeFromXZ(CharToLadderFacing, RailDirection).ToQuat();
    FHitResult Hit;
    SafeMoveUpdatedComponent(MoveAlongRail, DesiredRot, /*bSweep*/true, Hit);
	
    if (Hit.IsValidBlockingHit() && CYCharacterOwner && CYCharacterOwner->HasGameplayTag(CYGameplayTags::Status_Animation_Montage_ClimbingLadder))
    {
        // 레일 표면 따라 미끄러지도록 (레일 방향 외 장애물 최소화)
        SlideAlongSurface(MoveAlongRail, 1.f - Hit.Time, Hit.Normal, Hit, true);
    }
 
    // 4) 실제로 움직인 결과로 속도/AttachSpot 갱신
    const FVector NewLocation  = UpdatedComponent->GetComponentLocation();
    const FVector ActualDelta  = NewLocation - OldLocation;
 
    // 실제 이동량의 레일 투영분만 누적 → 적분 드리프트 제거
    const float   ActualMoveS  = FVector::DotProduct(ActualDelta, RailDirection);
    LadderAttachSpot = FMath::Clamp(LadderAttachSpot + ActualMoveS, 0.f, RailLength);

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (DeltaTime >= KINDA_SMALL_NUMBER) ? (ActualDelta / DeltaTime) : FVector::ZeroVector;
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

		OnLadderEntryInterpolationComplete.Broadcast();
	}
}

void UCYCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// FLAG_Custom_0 비트 체크 (사다리 타기 의도)
	bWantsToClimb = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
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
{
}

void FSavedMove_CY::Clear()
{
	Super::Clear();
	bWantsToClimb = false;
	SavedLadderAttachSpot = 0.f;
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

	if (bWantsToClimb)
	{
		const float AttachSpotDelta = FMath::Abs(SavedLadderAttachSpot - NewCY->SavedLadderAttachSpot);
		if (AttachSpotDelta > 5.f)
		{
			return false;
		}
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


