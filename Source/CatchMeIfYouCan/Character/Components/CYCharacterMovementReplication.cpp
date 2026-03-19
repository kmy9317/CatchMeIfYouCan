#include "CYCharacterMovementReplication.h"
#include "CYCharacterMovementComponent.h"

FCYCharacterNetworkMoveData::FCYCharacterNetworkMoveData()
    : bNetworkWantsToClimb(false)
    , NetworkLadderPhase(0)
    , NetworkClimbInput(0)
    , NetworkLadderAttachSpot(0.f)
    , bNetworkHasClimbStartRequest(false)
    , NetworkEntryType(0)
    , bNetworkUseInterpolation(false)
    , bNetworkClimbUp(true)
    , NetworkInitialAttachSpot(0.f)
    , NetworkLadderStandOff(0.f)
    , NetworkLadderStart(FVector::ZeroVector)
    , NetworkLadderEnd(FVector::ZeroVector)
    , NetworkLadderFacing(FVector::ForwardVector)
{
}

void FCYCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
    Super::ClientFillNetworkMoveData(ClientMove, MoveType);

    const FSavedMove_CY& CYMove = static_cast<const FSavedMove_CY&>(ClientMove);

    // 공통
    bNetworkWantsToClimb = CYMove.bWantsToClimb;
    NetworkLadderPhase = CYMove.SavedLadderPhase;
    NetworkClimbInput = CYMove.SavedClimbInput;
    NetworkLadderAttachSpot = CYMove.SavedLadderAttachSpot;

    // 시작 요청
    bNetworkHasClimbStartRequest = CYMove.bSavedHasClimbStartRequest;
    if (bNetworkHasClimbStartRequest)
    {
        const FCYLadderStartRequest& Req = CYMove.SavedClimbStartRequest;
        NetworkEntryType = Req.EntryType;
        bNetworkUseInterpolation = Req.bUseInterpolation;
        bNetworkClimbUp = Req.bClimbUp;
        NetworkInitialAttachSpot = Req.InitialAttachSpot;
        NetworkLadderStandOff = Req.LadderStandOff;
        NetworkLadderActor = Req.LadderActor;
        NetworkLadderStart = Req.LadderStart;
        NetworkLadderEnd = Req.LadderEnd;
        NetworkLadderFacing = Req.LadderFacing;
    }
}

bool FCYCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
    if (!Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType))
    {
        return false;
    }
    
    // climbing 중이거나 시작 요청이 있으면 사다리 데이터 전송
    uint8 bHasLadderData = bNetworkWantsToClimb || bNetworkHasClimbStartRequest;
    Ar.SerializeBits(&bHasLadderData, 1);

    if (bHasLadderData)
    {
        // climbing 여부
        uint8 bTempWantsToClimb = bNetworkWantsToClimb;
        Ar.SerializeBits(&bTempWantsToClimb, 1);
        if (Ar.IsLoading()) { bNetworkWantsToClimb = bTempWantsToClimb; }

        // ── 공통 climbing 데이터 (실제 climbing 중일 때만) ──
        if (bNetworkWantsToClimb)
        {
            Ar << NetworkLadderPhase;
            Ar << NetworkClimbInput;
            Ar << NetworkLadderAttachSpot;
        }

        // ── 시작 요청 여부 ──
        uint8 bTempHasRequest = bNetworkHasClimbStartRequest;
        Ar.SerializeBits(&bTempHasRequest, 1);
        if (Ar.IsLoading()) { bNetworkHasClimbStartRequest = bTempHasRequest; }

        if (bNetworkHasClimbStartRequest)
        {
            Ar << NetworkEntryType;

            uint8 bTempUseInterp = bNetworkUseInterpolation;
            Ar.SerializeBits(&bTempUseInterp, 1);
            if (Ar.IsLoading()) { bNetworkUseInterpolation = bTempUseInterp; }

            uint8 bTempClimbUp = bNetworkClimbUp;
            Ar.SerializeBits(&bTempClimbUp, 1);
            if (Ar.IsLoading()) { bNetworkClimbUp = bTempClimbUp; }

            Ar << NetworkInitialAttachSpot;
            Ar << NetworkLadderStandOff;

            UObject* LadderObj = NetworkLadderActor.Get();
            Ar << LadderObj;
            if (Ar.IsLoading())
            {
                NetworkLadderActor = Cast<AActor>(LadderObj);
            }

            bool bSuccess = true;
            NetworkLadderStart.NetSerialize(Ar, PackageMap, bSuccess);
            NetworkLadderEnd.NetSerialize(Ar, PackageMap, bSuccess);
            NetworkLadderFacing.NetSerialize(Ar, PackageMap, bSuccess);
        }
    }
    else if (Ar.IsLoading())
    {
        bNetworkWantsToClimb = false;
        bNetworkHasClimbStartRequest = false;
        NetworkClimbInput = 0;
        NetworkLadderAttachSpot = 0.f;
        NetworkLadderPhase = 0;
        NetworkLadderActor.Reset();
    }
    
    return !Ar.IsError();
}

FCYCharacterNetworkMoveDataContainer::FCYCharacterNetworkMoveDataContainer()
{
    NewMoveData = &CYDefaultMoveData[0];
    PendingMoveData = &CYDefaultMoveData[1];
    OldMoveData = &CYDefaultMoveData[2];
}