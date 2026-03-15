#include "GameModes/Lobby/CYLobbyGameMode.h"
#include "GameInstance/CYGameInstance.h"

void ACYLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	
}

void ACYLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle,
		this,
		&ACYLobbyGameMode::Test,
		30.0f,
		false
	);
}

void ACYLobbyGameMode::Test()
{
	UCYGameInstance* CYGameInstance = Cast<UCYGameInstance>(GetGameInstance());

	if (CYGameInstance)
	{
		//CYGameInstance->CallAddSessionMember();
	}
}
