#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CYLobbyGameMode.generated.h"

UCLASS()
class CATCHMEIFYOUCAN_API ACYLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

private:
	FTimerHandle TimerHandle;
	
protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void BeginPlay() override;

public:
	void Test();

};
