#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CYTypes/CYInGameTypes.h"
#include "CYItemBase.generated.h"

class ACYPlayerCharacter;
class UStaticMeshComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class EItemType : uint8
{
    Base,
    Weapon,
    Trap,
    Consumable
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemPickedUpDelegate, ACYItemBase*, Item);

UCLASS(Abstract)
class CATCHMEIFYOUCAN_API ACYItemBase : public AActor
{
    GENERATED_BODY()

public:
    ACYItemBase();

    // 기본 아이템 정보
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FText ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText ItemNameKR;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText ItemDescriptionKR;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    EItemType ItemType = EItemType::Base;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    int32 MaxStackCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Team", meta = (Bitmask, BitmaskEnum = "ECYTeamRole"))
	TArray<ECYTeamRole> AllowedTeams;

    // 현재 수량 (네트워크 동기화)
    UPROPERTY(ReplicatedUsing = OnRep_ItemCount, BlueprintReadOnly, Category = "Item")
    int32 ItemCount = 1;

    // 픽업 상태 (네트워크 동기화)
    UPROPERTY(ReplicatedUsing = OnRep_IsPickedUp, BlueprintReadOnly, Category = "Item")
    bool bIsPickedUp = false;

    // 컴포넌트들
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ItemMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionSphere;

	// 아웃라인 전용 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* OutlineMesh;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnItemPickedUpDelegate OnItemPickedUpDelegate;

	// 스폰 시 설정되는 오버라이드 값들 (네트워크 동기화)
	// HealAmount, DamageAmount, SpeedAmount 오버라이드
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Item|Override")
	float OverridePrimaryValue = -1.0f; 

	// Duration 시간 오버라이드
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Item|Override")
	float OverrideDuration = -1.0f; 

    // 픽업/사용 함수들
    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual void OnPickup(ACYPlayerCharacter* Character);

    UFUNCTION(BlueprintCallable, Category = "Item")
    virtual bool UseItem(ACYPlayerCharacter* Character);

    UFUNCTION(BlueprintCallable, Category = "Item")
    bool CanStackWith(ACYItemBase* OtherItem) const;

	UFUNCTION(BlueprintCallable, Category = "Item|Team")
	bool CanBePickedUpBy(ACYPlayerCharacter* Character) const;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 네트워크 동기화 함수들
    UFUNCTION()
    void OnRep_ItemCount();

    UFUNCTION()
    void OnRep_IsPickedUp();

    // 충돌 이벤트
    UFUNCTION()
    void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};