#pragma once

#include "CoreMinimal.h"
#include "CYCharacterMovementReplication.h"
#include "CYMovementRequest.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CYCharacterMovementComponent.generated.h"

struct FCYLadderStartRequest;
class ACYCharacterBase;
class ACYLadderBase;

/**
 * 커스텀 이동 모드 열거형
 * UE의 기본 이동 모드(Walking, Falling, Flying, Swimming) 외에
 * MOVE_Custom 모드 사용 시 CustomMovementMode 값으로 구분
 */
UENUM(BlueprintType)
enum ECYCustomMovementMode
{
	CMOVE_None         UMETA(DisplayName = "None"),      // 커스텀 모드 없음
	CMOVE_Climbing     UMETA(DisplayName = "Climbing"),  // 사다리 타기 모드
	CMOVE_MAX          UMETA(Hidden),                    // 열거형 최대값 (내부용)
};

UENUM(BlueprintType)
enum class ECYLadderPhase : uint8
{
	None            UMETA(DisplayName = "None"),
	Entering        UMETA(DisplayName = "Entering"),       // 진입 보간/몽타주 중
	Climbing        UMETA(DisplayName = "Climbing"),       // 정상 등반 중
	ExitingTop      UMETA(DisplayName = "Exiting Top"),    // 상단 탈출 몽타주 중
	ExitingBottom   UMETA(DisplayName = "Exiting Bottom"), // 하단 탈출 중
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLadderEntryInterpolationComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLadderPhaseChanged, ECYLadderPhase, NewPhase);

/**
 * 주요 기능:
 * 1. 사다리 타기 커스텀 이동 모드 (CMOVE_Climbing)
 * 2. 선형 레일 기반 이동 (Start → End 직선 경로)
 * 3. 네트워크 예측 지원 (FSavedMove_CY)
 * 4. 입력 기반 상/하 이동
 * 동작 원리:
 * - BeginClimbLadder() 호출 시 MOVE_Custom 모드로 전환
 * - PhysLadder()에서 매 프레임 사다리 물리 처리
 * - LadderAttachSpot 파라미터로 사다리 상의 위치 추적 (0 ~ RailLength)
 * - 입력을 RailDir에 투영하여 상/하 이동 속도 계산
 */

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CATCHMEIFYOUCAN_API UCYCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UCYCharacterMovementComponent();

	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	
	/**
	 * 사다리 등반 시작
	 * @param InLadder - 타고 있는 사다리 액터 (참조 유지용)
	 * @param InStart - 사다리 시작점 (월드 좌표, 하단)
	 * @param InEnd - 사다리 끝점 (월드 좌표, 상단)
	 * @param InFacing - 사다리가 향하는 방향 (캐릭터가 바라볼 방향)
	 * @param InLadderStandOff - 사다리에서 떨어뜨리는 거리 (캐릭터가 사다리에서 떨어진 거리)
	 * @param InAttachSpot - 초기 부착 위치 (0 ~ RailLength), -1이면 현재 위치에서 자동 계산
	 * @param bUseInterpolation - false면 즉시 스냅
	 */
	UFUNCTION(BlueprintCallable, Category="CY|Movement|Ladder")
	void BeginClimbLadder(AActor* InLadder, const FVector& InStart, const FVector& InEnd, const FVector& InFacing, float InLadderStandOff, float InAttachSpot, bool bUseInterpolation = true);

	/**
	 * 사다리 등반 종료
	 * @param bStepOffTop - true면 상단 탈출, false면 하단 탈출
	 */
	UFUNCTION(BlueprintCallable, Category="CY|Movement|Ladder")
	void EndClimbLadder(bool bStepOffTop);

	/**
	 * 현재 사다리 타는 중인지 확인
	 */
	UFUNCTION(BlueprintPure, Category="CY|Movement|Ladder")
	bool IsClimbingLadder() const;

	virtual float GetMaxSpeed() const override;

	float GetLadderAttachSpot() const { return LadderAttachSpot; }
	void SetLadderAttachSpot(const float InAttachSpot) { LadderAttachSpot = InAttachSpot; }
	
	/** 보간 진행 중인지 확인 */
	UFUNCTION(BlueprintPure, Category="CY|Movement|Ladder")
	bool IsInterpolatingToLadder() const { return bIsInterpolatingToLadder; }

	// 현재 사다리 단계 조회
	UFUNCTION(BlueprintPure, Category="CY|Movement|Ladder")
	ECYLadderPhase GetLadderPhase() const { return LadderPhase; }

	// 사다리 단계 전환 (GA에서 탈출 연출 시작 시 호출) 
	void SetLadderPhase(ECYLadderPhase NewPhase);

	int8 GetClimbInput() const { return ClimbInput; }

	void SetClimbInput(int8 NewInput);

	/**
	 * 사다리 등반 시작 요청 (Ability에서 호출)
	 * 실제 movement mode 전환은 UpdateCharacterStateBeforeMovement()에서 수행
	 */
	void RequestStartClimb(const FCYLadderStartRequest& Request);

	/** pending request 존재 여부 */
	bool HasPendingClimbRequest() const { return bHasPendingLadderStart; }

	/** 현재 pending request 참조 (SavedMove 저장용) */
	const FCYLadderStartRequest& GetPendingClimbRequest() const { return PendingLadderStartRequest; }

	// ── SavedMove 저장용 접근자 ──
	AActor* GetLadderActor() const { return LadderActor.Get(); }
	FVector GetRailDirection() const { return RailDirection; }
	float GetRailLength() const { return RailLength; }
	FVector GetCharToLadderFacing() const { return CharToLadderFacing; }
	FVector GetLadderStart() const { return LadderStart; }
	FVector GetLadderEnd() const { return LadderEnd; }
	float GetLadderStandOff() const { return LadderStandOff; }
	float GetBottomExitThreshold() const { return BottomExitThreshold; }
	float GetTopExitThreshold() const { return TopExitThreshold; }
	float GetMaxAllowedHorizontalDistance() const { return MaxAllowedHorizontalDistance; }

	// ── PrepMoveFor 복원용 세터 ──
	void SetLadderActorForReplay(AActor* InActor) { LadderActor = InActor; }
	void SetRailDirection(const FVector& InDir) { RailDirection = InDir; }
	void SetRailLength(float InLength) { RailLength = InLength; }
	void SetCharToLadderFacing(const FVector& InFacing) { CharToLadderFacing = InFacing; }
	void SetLadderStart(const FVector& InStart) { LadderStart = InStart; }
	void SetLadderEnd(const FVector& InEnd) { LadderEnd = InEnd; }
	void SetLadderStandOff(float InStandOff) { LadderStandOff = InStandOff; }
	void SetBottomExitThreshold(float Value) { BottomExitThreshold = Value; }
	void SetTopExitThreshold(float Value) { TopExitThreshold = Value; }
	void SetMaxAllowedHorizontalDistance(float Value) { MaxAllowedHorizontalDistance = Value; }

	// 진입 연출 완료 알림
	void NotifyEntryComplete();

protected:
	/**
	 * 이동 모드 변경 시 호출되는 콜백
	 */
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/**
	 * 커스텀 이동 모드 물리 처리
	 */
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	/**
	 * 압축된 네트워크 플래그에서 상태 복원
	 * FLAG_Custom_0 비트를 사다리 타기 의도로 사용
	 * 클라이언트 예측 힌트용 (실제 모드 전환은 Ability/RPC에서 처리)
	 */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/**
	 * 클라이언트 예측 데이터 반환
	 * 커스텀 SavedMove(FSavedMove_CY)를 사용하기 위해
	 * FNetworkPredictionData_Client_CY 인스턴스 반환
	 */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
	/**
	 * 사다리 물리 처리 (매 프레임 호출)
	 * @param DeltaTime - 프레임 시간
	 * @param Iterations - 물리 반복 횟수
	 */
	void PhysLadder(float DeltaTime, int32 Iterations);

	void UpdateLadderEntryInterpolation(float DeltaTime);

	/** Acceleration과 ControllerYaw로부터 ClimbInput을 계산 */
	void UpdateClimbInputFromAcceleration();

	/** pending request를 소비하여 실제 climbing을 시작 */
	void ConsumeAndStartClimb();

	/** request 데이터로 실제 climbing 상태 초기화 */
	void BeginClimbFromRequest(const FCYLadderStartRequest& Request);

	/** PhysLadder에서 이탈 조건 체크 */
	void CheckLadderExitConditions();

	/** 레일 중심선과의 수평 거리 계산 */
	float CalculateHorizontalDistanceFromRail(const FVector& Location) const;

public:
	/**
	 * 사다리 타기 의도 플래그
	 * - 네트워크 동기화에 사용
	 * - BeginClimbLadder()에서 true 설정
	 * - EndClimbLadder()에서 false 설정
	 */
	UPROPERTY()
	uint8 bWantsToClimb : 1;

	UPROPERTY(BlueprintAssignable, Category="CY|Movement|Ladder")
	FOnLadderEntryInterpolationComplete OnLadderEntryInterpolationComplete;

	UPROPERTY(BlueprintAssignable, Category="CY|Movement|Ladder")
	FOnLadderPhaseChanged OnLadderPhaseChanged;

protected:
	UPROPERTY()
	TObjectPtr<ACYCharacterBase> CYCharacterOwner = nullptr;
	
private:

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LadderActor;
	
	FVector LadderStart = FVector::ZeroVector;
	
	FVector LadderEnd = FVector::ZeroVector;

	/** pending 사다리 시작 요청이 있는지 여부 */
	bool bHasPendingLadderStart = false;

	/** 사다리 시작 요청 데이터 (다음 movement 틱에서 소비) */
	FCYLadderStartRequest PendingLadderStartRequest;

	/**
	 * 사다리가 향하는 방향 (월드 수평 방향)
	 * 캐릭터가 사다리를 바라보는 방향
	 */
	FVector CharToLadderFacing = FVector::ForwardVector;

	/**
	 * 사다리 레일 상의 현재 위치 파라미터
	 * 범위: 0 (하단) ~ RailLength (상단)
	 * PhysLadder()에서 매 프레임 업데이트
	 */
	float LadderAttachSpot = 0.f;

	/**
	 * 사다리 평면에서 캐릭터를 떨어뜨리는 거리 
	 */
	float LadderStandOff = 0.f;

	/**
	 * 사다리 레일 길이 (cm)
	 * BeginClimbLadder()에서 설정
	 */
	float RailLength = 0.f;
	
	/**
	 * 사다리 레일 방향 
	 */
	FVector RailDirection = FVector::UpVector;

	/** 사다리 진입 보간 중인지 여부 */
	bool bIsInterpolatingToLadder = false;

	/** 현재 사다리 단계 (None/Entering/Climbing/ExitingTop/ExitingBottom) */
	ECYLadderPhase LadderPhase = ECYLadderPhase::None;

	// SavedMove/NetworkMoveData로 직렬화되어 서버/replay에서 동일 값 사용
	int8 ClimbInput = 0;

	FVector InterpStartLocation = FVector::ZeroVector;

	FQuat InterpStartRotation = FQuat::Identity;

	FVector InterpTargetLocation = FVector::ZeroVector;
 
	FQuat InterpTargetRotation = FQuat::Identity;
    
	/** 보간 경과 시간 */
	float InterpElapsedTime = 0.f;

	UPROPERTY(EditAnywhere, Category="CY|Movement|Ladder")
	float MaxClimbSpeed = 180.f;
	
	/** 보간 지속 시간 */
	UPROPERTY(EditAnywhere, Category="CY|Movement|Ladder")
	float LadderEntryInterpDuration = 0.3f;
    
	/** 보간 곡선 (Ease In Out) */
	UPROPERTY(EditAnywhere, Category="CY|Movement|Ladder")
	float LadderEntryInterpEase = 2.0f;

	/** MoveAutonomous에서 네트워크 데이터로 ClimbInput을 설정한 경우 재계산 방지 */
	bool bClimbInputSetFromNetwork = false;

	// ── 이탈 판정용 캐시 (BeginClimbFromRequest에서 설정) ──

	/** 하단 이탈 임계값 (BottomSafetyMargin + CapsuleHalfHeight) */
	float BottomExitThreshold = 0.f;

	/** 상단 이탈 임계값 (TopSafetyMargin) */
	float TopExitThreshold = 0.f;

	/** 최대 허용 수평 거리 (진입 위치 기준 + 안전 마진) */
	float MaxAllowedHorizontalDistance = 0.f;

	/** 진입 시 계산된 타겟 위치 (수평 거리 기준점) */
	FVector EntryTargetLocation = FVector::ZeroVector;
	
	/**
	 * 캐싱을 위한 변수들
	 */
	float DefaultGravityScale = 0.f;
	float DefaultBrakingFrictionFactor = 0.f;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;

	FCYCharacterNetworkMoveDataContainer CYNetworkMoveDataContainer;
};

/**
 * 커스텀 SavedMove 클래스
 * UE의 클라이언트 예측 시스템에서 사용하는 이동 데이터 저장 구조
 * 역할:
 * 1. 클라이언트가 서버에 전송할 이동 데이터 압축
 * 2. 서버 응답 후 재시뮬레이션 시 이동 복원
 * 3. 네트워크 대역폭 절약 (비트 플래그 사용)
 * 동작 흐름:
 * 1. SetMoveFor(): 현재 CMC 상태를 SavedMove에 저장
 * 2. GetCompressedFlags(): 상태를 비트 플래그로 압축
 * 3. 서버로 전송
 * 4. PrepMoveFor(): 재시뮬레이션 시 SavedMove에서 CMC로 복원
 */
class FSavedMove_CY : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FSavedMove_CY();
	
	/**
	 * 사다리 타기 의도 플래그 (1비트)
	 * true: 클라이언트가 사다리 타는 중
	 * false: 일반 이동 중
	 * FLAG_Custom_0 비트로 네트워크 전송
	 */
	uint8 bWantsToClimb : 1;

	uint8 SavedLadderPhase = 0;

	int8 SavedClimbInput = 0;

	float SavedLadderAttachSpot = 0.f;

	/** pending climb request 존재 여부 */
	uint8 bSavedHasClimbStartRequest : 1;

	/** climb request 데이터 */
	FCYLadderStartRequest SavedClimbStartRequest;

	// ── climbing runtime 상태 (replay 복원용) ──

	/** 사다리 액터 참조 */
	TWeakObjectPtr<AActor> SavedLadderActor;

	/** 레일 방향 (Bottom → Top 정규화) */
	FVector SavedRailDirection = FVector::UpVector;

	/** 레일 전체 길이 */
	float SavedRailLength = 0.f;

	/** 캐릭터가 사다리를 바라보는 방향 */
	FVector SavedCharToLadderFacing = FVector::ForwardVector;

	/** 사다리 하단 위치 */
	FVector SavedLadderStart = FVector::ZeroVector;

	/** 사다리 상단 위치 */
	FVector SavedLadderEnd = FVector::ZeroVector;

	/** 사다리 전방 오프셋 거리 */
	float SavedLadderStandOff = 0.f;

	// ── 이탈 판정 캐시 ──
	float SavedBottomExitThreshold = 0.f;
	float SavedTopExitThreshold = 0.f;
	float SavedMaxAllowedHorizontalDistance = 0.f;
	
	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
};

/**
 * 클라이언트 예측 데이터 팩토리
 * 역할:
 * - FSavedMove_CY 인스턴스를 생성하는 팩토리
 * - 이동 데이터 풀링 관리
 * UE의 클라이언트 예측 시스템에서 사용
 * GetPredictionData_Client()에서 반환
 */
class FNetworkPredictionData_Client_CY : public FNetworkPredictionData_Client_Character
{
public:

	FNetworkPredictionData_Client_CY(const UCharacterMovementComponent& ClientMovement);

	/**
	 * 새로운 SavedMove 할당
	 * @return FSavedMove_CY 인스턴스 (스마트 포인터)
	 * - 커스텀 SavedMove(FSavedMove_CY) 생성
	 * - UE의 이동 예측 시스템에서 자동으로 호출
	 * - 이 함수를 오버라이드하지 않으면 기본 FSavedMove_Character가 사용됨
	 * - 커스텀 필드(bWantsToClimb)를 사용하려면 반드시 오버라이드 필요
	 */
	virtual FSavedMovePtr AllocateNewMove() override;

};


