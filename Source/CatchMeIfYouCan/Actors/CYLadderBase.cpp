// Fill out your copyright notice in the Description page of Project Settings.


#include "CYLadderBase.h"

#include "AbilitySystemComponent.h"
#include "CYLogChannels.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystem/Abilities/CYAbilityGameplayTags.h"
#include "Character/CYStatusGameplayTags.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Physics/CYCollisionChannels.h"

ACYLadderBase::ACYLadderBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    // 루트
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    SetRootComponent(RootSceneComponent);

    // 위치 마커 (MovementComponent에서 사용)
    BottomPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BottomPoint"));
    BottomPoint->SetupAttachment(RootSceneComponent);
    
    TopPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TopPoint"));
    TopPoint->SetupAttachment(RootSceneComponent);
    
    // 방향
    FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingDirection"));
    FacingArrow->SetupAttachment(RootSceneComponent);
    FacingArrow->SetHiddenInGame(true);

    // 진입 박스들
    TopEntryBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TopEntryBox"));
    TopEntryBox->SetupAttachment(RootSceneComponent);
    TopEntryBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TopEntryBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    TopEntryBox->SetCollisionResponseToChannel(CY_TraceChannel_Interaction, ECR_Block);
    TopEntryBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    MiddleEntryBox = CreateDefaultSubobject<UBoxComponent>(TEXT("MiddleEntryBox"));
    MiddleEntryBox->SetupAttachment(RootSceneComponent);
    MiddleEntryBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MiddleEntryBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    MiddleEntryBox->SetCollisionResponseToChannel(CY_TraceChannel_Interaction, ECR_Block);
    MiddleEntryBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    BottomEntryBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BottomEntryBox"));
    BottomEntryBox->SetupAttachment(RootSceneComponent);
    BottomEntryBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BottomEntryBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    BottomEntryBox->SetCollisionResponseToChannel(CY_TraceChannel_Interaction, ECR_Block);
    BottomEntryBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    
    LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
    LadderMesh->SetupAttachment(RootSceneComponent);
    LadderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    LadderCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryClearanceBox"));
    LadderCollisionBox->SetupAttachment(RootSceneComponent);
    LadderCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    LadderCollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
    LadderCollisionBox->SetCollisionResponseToChannel(CY_TraceChannel_Interaction, ECR_Ignore);
    LadderCollisionBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    LadderCollisionBox->SetBoxExtent(FVector(100.f, 100.f, 150.f));
    LadderCollisionBox->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
    LadderCollisionBox->ShapeColor = FColor::Cyan; 
}

void ACYLadderBase::BeginPlay()
{
    Super::BeginPlay();

    if (TopEntryBox)
    {
        TopEntryBox->OnComponentBeginOverlap.AddDynamic(this, &ACYLadderBase::OnEntryBoxBeginOverlap);
        TopEntryBox->OnComponentEndOverlap.AddDynamic(this, &ACYLadderBase::OnEntryBoxEndOverlap);
    }
    
    if (MiddleEntryBox)
    {
        MiddleEntryBox->OnComponentBeginOverlap.AddDynamic(this, &ACYLadderBase::OnEntryBoxBeginOverlap);
        MiddleEntryBox->OnComponentEndOverlap.AddDynamic(this, &ACYLadderBase::OnEntryBoxEndOverlap);
    }
    
    if (BottomEntryBox)
    {
        BottomEntryBox->OnComponentBeginOverlap.AddDynamic(this, &ACYLadderBase::OnEntryBoxBeginOverlap);
        BottomEntryBox->OnComponentEndOverlap.AddDynamic(this, &ACYLadderBase::OnEntryBoxEndOverlap);
    }
}

FVector ACYLadderBase::GetBottomWorldLocation() const
{
    return BottomPoint ? BottomPoint->GetComponentLocation() : GetActorLocation();
}

FVector ACYLadderBase::GetTopWorldLocation() const
{
    return TopPoint ? TopPoint->GetComponentLocation() : GetActorLocation() + FVector(0, 0, 200);
}

FVector ACYLadderBase::GetHorizontalFacingDirection() const
{
    if (!FacingArrow)
    {
        return FVector::ForwardVector;
    }
    
    FVector Facing = FacingArrow->GetForwardVector();
    Facing.Z = 0.f;
    return Facing.GetSafeNormal();
}

FVector ACYLadderBase::GetClimbingDirection() const
{
    return (GetTopWorldLocation() - GetBottomWorldLocation()).GetSafeNormal();
}

float ACYLadderBase::GetTotalHeight() const
{
    return FVector::Distance(GetBottomWorldLocation(), GetTopWorldLocation());
}

ELadderEntryType ACYLadderBase::GetPlayerEntryType(const AActor* Player) const
{
    if (!Player)
    {
        return ELadderEntryType::None;
    }

    // 우선순위: Top > Bottom > Middle (캐시가 아닌 즉시 조회)
    if (TopEntryBox && TopEntryBox->IsOverlappingActor(Player))
    {
        return ELadderEntryType::Top;
    }
    if (BottomEntryBox && BottomEntryBox->IsOverlappingActor(Player))
    {
        return ELadderEntryType::Bottom;
    }
    if (MiddleEntryBox && MiddleEntryBox->IsOverlappingActor(Player))
    {
        return ELadderEntryType::Middle;
    }

    return ELadderEntryType::None;
}

float ACYLadderBase::CalculateInitialRailParameter(ELadderEntryType EntryType, const ACharacter* Character) const
{
    if (!Character)
    {
        return 0.f;
    }
    
    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.0f;
    const float Height = GetTotalHeight();
    
    switch (EntryType)
    {
    case ELadderEntryType::Top:
        // 상단 진입: 
        return Height -TopSafetyMargin  - 5.0f;
            
    case ELadderEntryType::Bottom:
        // 하단 진입: 아래에서 (캡슐높이 + 바텀 마진)만큼 위
        return FMath::Clamp(
            CapsuleHalfHeight + BottomSafetyMargin,
            CapsuleHalfHeight,
            Height - CapsuleHalfHeight
        );
            
    case ELadderEntryType::Middle:
        {
            // 중간 진입: 현재 위치 투영 (기존 로직)
            const FVector CharPos = Character->GetActorLocation();
            const FVector Bottom = GetBottomWorldLocation();
            const FVector ClimbDir = GetClimbingDirection();
            const FVector RelativePos = CharPos - Bottom;
            float ProjectedHeight = FVector::DotProduct(RelativePos, ClimbDir);
            
            return FMath::Clamp(
                ProjectedHeight, 
                CapsuleHalfHeight, 
                Height - CapsuleHalfHeight
            );
        }
            
    default:
        return CapsuleHalfHeight;
    }
}

void ACYLadderBase::DetermineClimbDirection(const ELadderEntryType EntryType, const ACharacter* Character, bool& OutIsClimbingUp) const
{
    switch (EntryType)
    {
        case ELadderEntryType::Top:
            OutIsClimbingUp = false;
            break;
        case ELadderEntryType::Bottom:
            OutIsClimbingUp = true;
            break;
        case ELadderEntryType::Middle:
            DetermineClimbDirectionForAutoGrab(Character, OutIsClimbingUp);
            break;
        default:
            OutIsClimbingUp = true;
        break;
    }
}

void ACYLadderBase::DetermineClimbDirectionForAutoGrab(const ACharacter* Character, bool& OutIsClimbingUp) const
{
    if (!Character)
    {
        OutIsClimbingUp = true;
        return;
    }
    
    // 1. 수직 속도 기반
    const float VerticalVelocity = Character->GetVelocity().Z;
    if (FMath::Abs(VerticalVelocity) > 50.0f)
    {
        OutIsClimbingUp = VerticalVelocity > 0;
        return;
    }
    
    // 2. 입력 방향 기반
    const FVector InputDir = Character->GetLastMovementInputVector().GetSafeNormal2D();
    if (InputDir.SizeSquared() > 0.1f)
    {
        const FVector CharFacingOnLadder = -GetHorizontalFacingDirection();
        const float Dot = FVector::DotProduct(InputDir, CharFacingOnLadder);
        OutIsClimbingUp = Dot >= 0.0f;
        return;
    }
    
    // 3. 캐릭터 높이 기반
    const float CharHeight = Character->GetActorLocation().Z;
    const float LadderCenter = (GetBottomWorldLocation().Z + GetTopWorldLocation().Z) * 0.5f;
    OutIsClimbingUp = CharHeight < LadderCenter;
}

FVector ACYLadderBase::CalculateCharacterToLadderFacing() const
{
    const FVector RailDirection = GetClimbingDirection();
    FVector RawFacing = GetHorizontalFacingDirection();
    
    if (!RawFacing.Normalize())
    {
        RawFacing = FVector::ForwardVector;
    }
    
    // 레일축에 직교하도록 투영
    FVector FacingOnPlane = RawFacing - FVector::DotProduct(RawFacing, RailDirection) * RailDirection;
    
    if (!FacingOnPlane.Normalize())
    {
        // 평행한 경우 임의의 직교축 생성
        const FVector AnyPerp = FVector::CrossProduct(
            RailDirection,
            (FMath::Abs(RailDirection.Z) < 0.99f ? FVector::UpVector : FVector::RightVector)
        );
        FacingOnPlane = AnyPerp.GetSafeNormal();
    }
    
    return -FacingOnPlane; 
}

bool ACYLadderBase::CanAutoGrabFromMiddle(const ACharacter* Character) const
{
    if (!Character || !bEnableAutoGrab)
    {
        return false;
    }
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement)
    {
        return false;
    }
    
    // 1. 공중에 있어야 함
    if (!Movement->IsFalling())
    {
        return false;
    }
    
    // 2. 최소한의 상승/하강 속도 체크
    if (FMath::Abs(Movement->Velocity.Z) < MinVerticalSpeedForAutoGrab)
    {
        return false;
    }
    
    // 3. 사다리를 바라보고 있어야 함
    const FVector CharForward = Character->GetActorForwardVector();
    const FVector ToLadder = (GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
    const float ClampedDot = FMath::Clamp(FVector::DotProduct(CharForward, ToLadder), -1.f, 1.f);
    const float Angle = FMath::RadiansToDegrees(FMath::Acos(ClampedDot));
    if (Angle > MaxAutoGrabAngle)
    {
        return false;
    }
    
    // 4. 충분히 가까워야 함
    const float Distance = FVector::Dist2D(Character->GetActorLocation(), GetActorLocation());
    if (Distance > EntryBoxRadius * AutoGrabDistanceRatio)
    {
        return false;
    }
    
    return true;
}

void ACYLadderBase::CalculateEntryTransform(float RailParameter, float StandOffDistance, FVector& OutLocation, FRotator& OutRotation) const
{
    const FVector RailDir = GetClimbingDirection();
    const FVector CharToLadderDir = CalculateCharacterToLadderFacing();
    
    // 레일 상 위치 계산
    const FVector RailPos = GetBottomWorldLocation() + RailDir * RailParameter;
    
    // 사다리로부터 StandOff 거리만큼 떨어진 위치
    OutLocation = RailPos - CharToLadderDir * StandOffDistance;
    
    // 사다리를 바라보는 회전
    OutRotation = FRotationMatrix::MakeFromXZ(CharToLadderDir, RailDir).Rotator();
}

void ACYLadderBase::TryAutoGrabLadder(ACharacter* Character)
{
    if (!Character || !CanAutoGrabFromMiddle(Character))
    {
        UpdatePlayerEntryType(Character);
        return;
    }
    
    FGameplayEventData EventData;
    EventData.Instigator = Character;
    EventData.Target = this;  // 사다리 액터 전달

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Character, 
        CYGameplayTags::Ability_Action_Climbing, 
        EventData
    );

    UE_LOG(LogCY, Warning, TEXT("Auto-grabbed ladder!"));
}

void ACYLadderBase::OnEntryBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!Character)
    {
        return;
    }
    // 중간 박스 특별 처리
    if (OverlappedComponent == MiddleEntryBox)
    {
        if (bEnableAutoGrab)
        {
            TryAutoGrabLadder(Character);
        }
        else
        {
            UpdatePlayerEntryType(Character);
        }
    }
    else
    {
        // 상/하단 박스: 일반 상호작용
        UpdatePlayerEntryType(Character);
    }
}

void ACYLadderBase::OnEntryBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (ACharacter* Character = Cast<ACharacter>(OtherActor))
    {
        // 모든 박스를 벗어났는지 체크
        if (!TopEntryBox->IsOverlappingActor(Character) &&
            !MiddleEntryBox->IsOverlappingActor(Character) &&
            !BottomEntryBox->IsOverlappingActor(Character))
        {
            PlayerEntryTypeMap.Remove(Character);
        }
        else
        {
            UpdatePlayerEntryType(Character);
        }
    }
}

void ACYLadderBase::UpdatePlayerEntryType(AActor* Player)
{
    if (!Player)
    {
        return;
    }
    
    // 우선순위: Top > Bottom > Middle
    if (TopEntryBox && TopEntryBox->IsOverlappingActor(Player))
    {
        PlayerEntryTypeMap.Add(Player, ELadderEntryType::Top);
    }
    else if (BottomEntryBox && BottomEntryBox->IsOverlappingActor(Player))
    {
        PlayerEntryTypeMap.Add(Player, ELadderEntryType::Bottom);
    }
    else if (MiddleEntryBox && MiddleEntryBox->IsOverlappingActor(Player))
    {
        PlayerEntryTypeMap.Add(Player, ELadderEntryType::Middle);
    }
}

FCYInteractionInfo ACYLadderBase::GetPreInteractionInfo(const FCYInteractionQuery& InteractionQuery) const
{
    const ELadderEntryType EntryType = GetPlayerEntryType(InteractionQuery.RequestingAvatar.Get());
    
    // 중간 박스 + 자동 그랩 = 상호작용 정보 없음
    if (EntryType == ELadderEntryType::Middle && bEnableAutoGrab)
    {
        return FCYInteractionInfo();
    }
    
    switch (EntryType)
    {
        case ELadderEntryType::Top:
            return ClimbDownInteractionInfo;
        case ELadderEntryType::Bottom:
            return ClimbUpInteractionInfo;
        case ELadderEntryType::Middle:
            return ClimbMiddleInteractionInfo;
        default:
            return FCYInteractionInfo();
    }
}

void ACYLadderBase::GetMeshComponents(TArray<UMeshComponent*>& OutMeshComponents) const
{
    if (LadderMesh && LadderMesh->GetStaticMesh())
    {
        OutMeshComponents.Add(LadderMesh);
    }
}

bool ACYLadderBase::CanInteraction(const FCYInteractionQuery& InteractionQuery) const
{
    if (!Super::CanInteraction(InteractionQuery))
    {
        return false;
    }

    if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InteractionQuery.RequestingAvatar.Get()))
    {
        if (ASC->HasMatchingGameplayTag(CYGameplayTags::Status_Movement_Climbing))
        {
            return false;
        }
    }
    
    const ELadderEntryType EntryType = GetPlayerEntryType(InteractionQuery.RequestingAvatar.Get());
    
    // 중간 박스는 자동 그랩 활성화시 상호작용 불가
    if (EntryType == ELadderEntryType::Middle && bEnableAutoGrab)
    {
        return false;
    }
    
    return EntryType != ELadderEntryType::None;
}






