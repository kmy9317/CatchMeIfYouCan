#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CYLoginGameMode.generated.h"

UCLASS()
class CATCHMEIFYOUCAN_API ACYLoginGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACYLoginGameMode();
	
private:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> LoginLevelWidgetClass;

	UPROPERTY()
	UUserWidget* LoginLevelWidgetInstance;

protected:
	virtual void BeginPlay() override;
	
public:
	UFUNCTION(BlueprintCallable)
	void HostButtonClick();

	UFUNCTION(BlueprintCallable)
	void JoinButtonClick();

	void ShowLoginLevel();
};
