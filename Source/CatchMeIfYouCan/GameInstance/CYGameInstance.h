#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "CYGameInstance.generated.h"

enum class EButtonType : uint8
{
	Host,
	Join
};

UCLASS()

class CATCHMEIFYOUCAN_API UCYGameInstance : public UGameInstance
{
	GENERATED_BODY()

private:
	IOnlineSubsystem* OSS;
	IOnlineIdentityPtr Identity;
	IOnlineSessionPtr Sessions;
	TSharedPtr<FOnlineSessionSearch> SearchSettings;
	
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;

	FName CurrentSessionName;

public:
	EButtonType ButtonType;

private:
	void InitializeOnlineSubsystems();

protected:
	virtual void Init() override;
	
	virtual void Shutdown() override;

public:
	void CallCreateSession();
	
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	void CallFindSessions();
	
	void OnFindSessionsComplete(bool bWasSuccessful);

	void CallJoinSession(const FOnlineSessionSearchResult& SearchResult);
	
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
};
