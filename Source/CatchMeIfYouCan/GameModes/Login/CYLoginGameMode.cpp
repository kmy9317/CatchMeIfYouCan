#include "GameModes/Login/CYLoginGameMode.h"
#include "GameInstance/CYGameInstance.h"
#include "Blueprint/UserWidget.h"

ACYLoginGameMode::ACYLoginGameMode()
{
	LoginLevelWidgetClass = nullptr;
}

void ACYLoginGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		ENetMode NetMode = GetWorld()->GetNetMode();
		FString NetModeString;
		if (NetMode == NM_Standalone)
		{
			NetModeString = TEXT("Standalone");
		}
		else if (NetMode == NM_ListenServer)
		{
			NetModeString = TEXT("Host");
		}
		else if (NetMode == NM_Client)
		{
			NetModeString = TEXT("Client");
		}
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Current NetMode: %s"), *NetModeString));
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		PC->bShowMouseCursor = true;               
		PC->bEnableClickEvents = true;            
		PC->bEnableMouseOverEvents = true;         
		PC->SetInputMode(FInputModeUIOnly());
	}

	ShowLoginLevel();
}

void ACYLoginGameMode::HostButtonClick()
{
	UCYGameInstance* CYGameInstance = Cast<UCYGameInstance>(GetGameInstance());

	if (CYGameInstance)
	{
		CYGameInstance->ButtonType = EButtonType::Host;
		CYGameInstance->CallFindSessions();
	}
}

void ACYLoginGameMode::JoinButtonClick()
{
	UCYGameInstance* CYGameInstance = Cast<UCYGameInstance>(GetGameInstance());

	if (CYGameInstance)
	{
		CYGameInstance->ButtonType = EButtonType::Join;
		CYGameInstance->CallFindSessions();
	}
}

void ACYLoginGameMode::ShowLoginLevel()
{
	if (LoginLevelWidgetClass)
	{
		LoginLevelWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), LoginLevelWidgetClass);
		if (LoginLevelWidgetInstance)
		{
			LoginLevelWidgetInstance->AddToViewport();
		}
	}
}

