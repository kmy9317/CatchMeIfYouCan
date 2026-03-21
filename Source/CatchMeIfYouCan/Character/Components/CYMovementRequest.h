#pragma once

#include "CoreMinimal.h"
#include "CYMovementRequest.generated.h"

/**
 * 사다리 등반 시작 요청 데이터
 * Ability가 계산한 시작값을 CMC에 전달하여
 * 예측 파이프라인(SavedMove/NetworkMoveData) 안에서 처리되도록 함
 */
USTRUCT()
struct FCYLadderStartRequest
{
	GENERATED_BODY()

	/** 타고 있는 사다리 액터 */
	UPROPERTY()
	TWeakObjectPtr<AActor> LadderActor;

	/** 진입 타입 (Top/Bottom/Middle) */
	UPROPERTY()
	uint8 EntryType = 0;

	/** 초기 레일 위치 (0 ~ RailLength) */
	UPROPERTY()
	float InitialAttachSpot = 0.f;

	/** 사다리 전방 오프셋 거리 */
	UPROPERTY()
	float LadderStandOff = 0.f;

	/** 보간 사용 여부 */
	UPROPERTY()
	uint8 bUseInterpolation : 1;

	/** 등반 방향 (true = 상향) */
	UPROPERTY()
	uint8 bClimbUp : 1;

	/** 사다리 하단 월드 위치 */
	UPROPERTY()
	FVector LadderStart = FVector::ZeroVector;

	/** 사다리 상단 월드 위치 */
	UPROPERTY()
	FVector LadderEnd = FVector::ZeroVector;

	/** 사다리 수평 정면 방향 */
	UPROPERTY()
	FVector LadderFacing = FVector::ForwardVector;

	FCYLadderStartRequest()
		: bUseInterpolation(0)
		, bClimbUp(1)
	{
	}
};