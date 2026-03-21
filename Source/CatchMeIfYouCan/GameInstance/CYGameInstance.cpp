#include "CYGameInstance.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "GameModes/Login/CYLoginGameMode.h"

void UCYGameInstance::Init()
{
	Super::Init();

	//InitializeOnlineSubsystems();
}

void UCYGameInstance::InitializeOnlineSubsystems()
{
	OSS = IOnlineSubsystem::Get();
	if (!OSS)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, "OSS Not Valid");	
		return;
	}

	Identity = OSS->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, "Identity Not Valid");	
		return;
	}

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("accountportal");
	Credentials.Id = TEXT("");
	Credentials.Token = TEXT("");

	Identity->OnLoginCompleteDelegates->AddUObject(this, &UCYGameInstance::OnLoginComplete);
	Identity->Login(0, Credentials);
	
	Sessions = OSS->GetSessionInterface();
	if (!Sessions.IsValid())
	{
		//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, "Sessions Not Valid");	
		return;
	}

	Sessions->OnCreateSessionCompleteDelegates.AddUObject(this, &UCYGameInstance::OnCreateSessionComplete);
	Sessions->OnFindSessionsCompleteDelegates.AddUObject(this, &UCYGameInstance::OnFindSessionsComplete);
	Sessions->OnJoinSessionCompleteDelegates.AddUObject(this, &UCYGameInstance::OnJoinSessionComplete);
}

void UCYGameInstance::Shutdown()
{
	IOnlineSubsystem* LocalOSS = Online::GetSubsystem(GetWorld());
	if (LocalOSS)
	{
		IOnlineSessionPtr LocalSessions = LocalOSS->GetSessionInterface();
		if (LocalSessions.IsValid() && LocalSessions->GetNamedSession(CurrentSessionName))
		{
			OnDestroySessionCompleteDelegate.BindUObject(this, &UCYGameInstance::OnDestroySessionComplete);
			LocalSessions->AddOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);
			LocalSessions->DestroySession(CurrentSessionName);
		}
	}

	Super::Shutdown();
}

void UCYGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		if (GEngine)
		{
			// GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			// 	FString::Printf(TEXT("EAS 로그인 성공: %s"), *UserId.ToString()));
		}

		IOnlineSubsystem* LocalOSS = Online::GetSubsystem(GetWorld());
		if (!LocalOSS) return;

		IOnlineUserPtr UserInterface = LocalOSS->GetUserInterface();
		if (!UserInterface.IsValid())
		{
			if (GEngine)
			{
				// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
				// 	FString::Printf(TEXT("유저인터페이스 가져오기 실패")));
			}
			/*
			UWorld* World = GetWorld();
			if (World)
			{
				AGameModeBase* GameMode = World->GetAuthGameMode();
				ACYLoginGameMode* CYLoginGameMode = Cast<ACYLoginGameMode>(GameMode);
				if (!IsValid(CYLoginGameMode))
				{
					return;
				}
				
				CYLoginGameMode->ShowLoginLevel();
			}
			*/
		}
	}
	else
	{
		if (GEngine)
		{
			// GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			// 	FString::Printf(TEXT("로그인 실패: %s"), *Error));
		}
	}
}

void UCYGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		GetWorld()->ServerTravel("/Game/Maps/PlayMap_v3?listen", true);
	}
}

void UCYGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSubsystem* LocalOSS = Online::GetSubsystem(GetWorld());
	if (LocalOSS)
	{
		IOnlineSessionPtr LocalSessions = LocalOSS->GetSessionInterface();
		if (LocalSessions.IsValid() && bWasSuccessful)
		{
			FDelegateHandle DelegateHandleToClear = OnDestroySessionCompleteDelegate.GetHandle();
			LocalSessions->ClearOnDestroySessionCompleteDelegate_Handle(DelegateHandleToClear);
		}
	}
}

void UCYGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (SearchSettings.IsValid())
		{		
			if (ButtonType == EButtonType::Host)
			{
				if (SearchSettings->SearchResults.Num() > 0)
				{
					if (GEngine)
					{
						//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("세션 이미 존재")));
					}
				}
				else
				{
					CallCreateSession();
				}
			}
			else if (ButtonType == EButtonType::Join)
			{
				if (SearchSettings->SearchResults.Num() > 0)
				{
					CallJoinSession(SearchSettings->SearchResults[0]);
				}
				else
				{
					if (GEngine)
					{
						//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("참여할 세션 없음")));
					}
				}
			}
		}
	}
}

void UCYGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success && Sessions.IsValid())
	{
		FString TravelURL;

		if (Sessions->GetResolvedConnectString(SessionName, TravelURL))
		{
			APlayerController* PC = GetFirstLocalPlayerController();
			if (PC)
			{
				// GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Green,
				// FString::Printf(TEXT("조인 성공")));
				// GEngine->AddOnScreenDebugMessage(-1, 30.f, FColor::Green,
				// FString::Printf(TEXT("커넥트 스트링: %s"), *TravelURL));
				// UE_LOG(LogTemp, Error, TEXT("커넥트스트링 : %s"), *TravelURL);
				PC->ClientTravel(TravelURL, TRAVEL_Absolute);
			}
		}
	}
}

void UCYGameInstance::CallCreateSession()
{
	if (!Sessions.IsValid())
	{
		return;
	}
	
	CurrentSessionName = TEXT("CYSession");
	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.NumPublicConnections = 6;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.Set(FName("SEARCHKEY"), FString("Lobby"), EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(FName(TEXT("SESSION_JOIN_NAME_KEY")), CurrentSessionName.ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	
	Sessions->CreateSession(0, CurrentSessionName, SessionSettings);
}

void UCYGameInstance::CallFindSessions()
{
	if (!Sessions.IsValid())
	{
		return;
	}
	
	SearchSettings = MakeShareable(new FOnlineSessionSearch());
	SearchSettings->bIsLanQuery = false;
	SearchSettings->MaxSearchResults = 5;
	//SearchSettings->QuerySettings.Set(FName(TEXT("PRESENCE")), true, EOnlineComparisonOp::Equals);
	SearchSettings->QuerySettings.Set(FName(TEXT("SEARCHKEY")), FString("Lobby"), EOnlineComparisonOp::Equals);

	if (!SearchSettings.IsValid())
	{
		return;
	}
	
	Sessions->FindSessions(0, SearchSettings.ToSharedRef());
}

void UCYGameInstance::CallJoinSession(const FOnlineSessionSearchResult& SearchResult)
{
	if (Sessions.IsValid())
	{
		FString JoinSessionNameStr;
		
		const FOnlineSessionSettings& Settings = SearchResult.Session.SessionSettings;
		
		if (Settings.Get(FName(TEXT("SESSION_JOIN_NAME_KEY")), JoinSessionNameStr))
		{
			FName SessionName = FName(*JoinSessionNameStr);

			Sessions->JoinSession(0, SessionName, SearchResult);
		}
	}
}
