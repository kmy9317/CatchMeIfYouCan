// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CYJailPoint.generated.h"

class UBoxComponent;
class ACYDoorBase;
class UArrowComponent;
class ACYCharacterBase;

UCLASS()
class CATCHMEIFYOUCAN_API ACYJailPoint : public AActor
{
	GENERATED_BODY()

public:
	ACYJailPoint();

	UFUNCTION(BlueprintPure)
	FTransform GetSnapTransform() const { return GetActorTransform(); }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnVolumeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 실제 해제 처리(태그 제거 + AliveRobberCount 증가) */
	void ReleasePrisoner(ACYCharacterBase* Robber);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> RootPoint;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> ExitVolume;

	// UPROPERTY(VisibleAnywhere)
	// UBillboardComponent* EditorSprite;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UArrowComponent> ArrowComponent;
	
};
