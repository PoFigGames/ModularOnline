// Copyright PoFig Games Studio. All Rights Reserved.

#include "Server/ModularServerSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineSubsystem.h"
#include "Core/ModularOnlineTags.h"
#include "Engine/GameInstance.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularServerSubsystem)

void UModularServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UModularOnlineSubsystem>();

	const auto GameInstance = GetGameInstance();
	bIsDedicatedServer = GameInstance && GameInstance->IsDedicatedServerInstance();
}

void UModularServerSubsystem::Deinitialize()
{
	ServerAccount = UE::Online::FAccountId { };

	Super::Deinitialize();
}

bool UModularServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is not a game specific subclass
	return ChildClasses.Num() == 0;
}

UModularOnlineSubsystem* UModularServerSubsystem::GetOnline() const
{
	const auto GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UModularOnlineSubsystem>() : nullptr;
}

EModularOnlineRole UModularServerSubsystem::GetServerRole() const
{
	const auto Settings = GetDefault<UModularOnlineSettings>();

	// A server publishes where matches are published, which the project already says once. Guessing it
	// from whichever role happens to carry sessions put the server and everything else on different
	// providers the moment a project had two.
	return Settings ? Settings->MatchRole : EModularOnlineRole::Service;
}

bool UModularServerSubsystem::IsServerLoggedIn() const
{
	return ServerAccount.IsValid();
}

UE::Online::FAccountId UModularServerSubsystem::GetServerAccountId() const
{
	return ServerAccount;
}

void UModularServerSubsystem::AnnounceServerLogin(const FModularOnlineResult& Result, const FModularServerLoginDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Result);
	OnServerLoginComplete.Broadcast(Result);
	K2_OnServerLoginComplete.Broadcast(Result);
}

bool UModularServerSubsystem::LoginServer(FModularServerLoginDelegate OnComplete)
{
	if (IsServerLoggedIn())
	{
		// Asking again for a login this machine already has is not a failure: the services would answer
		// AlreadyLoggedIn, which reads in a log as though the server had lost its account.
		UE_LOG(LogModularOnline, Log, TEXT("The server is already signed in as %s."), *ToLogString(ServerAccount));

		AnnounceServerLogin(FModularOnlineResult::Success(), OnComplete);

		return true;
	}

	if (!bIsDedicatedServer)
	{
		// The credentials this signs in with belong to a machine, not to a person, and a machine that has
		// players sitting at it signs them in instead. Asked of a listen server, this would take the one
		// login the process has away from whoever is playing on it.
		UE_LOG(LogModularOnline, Warning, TEXT("A server login was asked for on a machine that is not a dedicated server."));

		return false;
	}

	const auto Online = GetOnline();
	const auto Role = GetServerRole();
	const auto Auth = Online ? Online->GetInterface<UE::Online::IAuth>(Role) : nullptr;

	if (!Auth.IsValid())
	{
		UE_LOG(LogModularOnline, Warning, TEXT("A server login was asked for, but no provider here signs anybody in."));

		return false;
	}

	const auto Settings = GetDefault<UModularServerSettings>();
	const auto Credentials = Settings ? Settings->GetServerCredentialsType(Online->GetProviderName(Role)) : FName { };

	if (Credentials.IsNone())
	{
		UE_LOG(LogModularOnline, Warning, TEXT("No server credentials are named for provider '%s'."), *Online->GetProviderName(Role));

		return false;
	}

	UE::Online::FAuthLogin::Params Params;

	// A server has no system user behind it; the credentials type is what tells the services that this is
	// a machine and not a person.
	Params.PlatformUserId = PLATFORMUSERID_NONE;
	Params.CredentialsType = Credentials;

	UE_LOG(LogModularOnline, Log, TEXT("Signing the server in to %s as '%s'."), *Online->GetProviderName(Role), *Credentials.ToString());

	Auth->Login(MoveTemp(Params)).OnComplete(this, [this, OnComplete](const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result)
	{
		auto Answer = FModularOnlineResult::Success();

		if (Result.IsOk())
		{
			ServerAccount = Result.GetOkValue().AccountInfo->AccountId;

			UE_LOG(LogModularOnline, Log, TEXT("The server signed in as %s."), *ToLogString(ServerAccount));
		}
		else
		{
			Answer = FModularOnlineResult::FromOnlineError(Result.GetErrorValue());

			UE_LOG(LogModularOnline, Error, TEXT("The server could not sign in: %s"), *Answer.ToLogString());
		}

		AnnounceServerLogin(Answer, OnComplete);
	});

	return true;
}
