#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

/**
 * 사다리 관련 데이터를 서버로 직렬화하는 커스텀 네트워크 이동 데이터
 * ClientFillNetworkMoveData(): SavedMove에서 값을 꺼내 채움
 * Serialize(): 네트워크를 통해 서버로 전송/수신
 */
class FCYCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
public:
	typedef FCharacterNetworkMoveData Super;

	FCYCharacterNetworkMoveData();

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	// ── 공통 (climbing 중이면 항상 전송) ──
	uint8 bNetworkWantsToClimb : 1;
	uint8 NetworkLadderPhase;
	int8 NetworkClimbInput;
	float NetworkLadderAttachSpot;

	// ── 시작 요청 (bNetworkHasClimbStartRequest일 때만 전송) ──
	uint8 bNetworkHasClimbStartRequest : 1;
	uint8 NetworkEntryType;
	uint8 bNetworkUseInterpolation : 1;
	uint8 bNetworkClimbUp : 1;
	float NetworkInitialAttachSpot;
	float NetworkLadderStandOff;

	// 사다리 액터 참조 (시작 요청 시에만 전송)
	TWeakObjectPtr<AActor> NetworkLadderActor;

	// 사다리 위치 데이터 (시작 요청 시에만 전송)
	FVector NetworkLadderStart;
	FVector NetworkLadderEnd;
	FVector NetworkLadderFacing;
};

/**
 * 커스텀 NetworkMoveData 컨테이너
 * UE는 매 패킷에 최대 3개의 move를 담을 수 있음 (New, Pending, Old)
 */
class CATCHMEIFYOUCAN_API FCYCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
public:
	FCYCharacterNetworkMoveDataContainer();

	FCYCharacterNetworkMoveData CYDefaultMoveData[3];
};