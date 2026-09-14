// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularFeatureSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Engine/GameInstance.h"
#include "Online/OnlineErrorDefinitions.h"
#include "User/ModularUserInfo.h"
#include "User/ModularUserSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularFeatureSubsystem)

void UModularFeatureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UModularOnlineSubsystem>();
	Collection.InitializeDependency<UModularUserSubsystem>();
}

UModularOnlineSubsystem* UModularFeatureSubsystem::GetOnline() const
{
	const auto GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UModularOnlineSubsystem>() : nullptr;
}

UModularUserSubsystem* UModularFeatureSubsystem::GetUsers() const
{
	const auto GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UModularUserSubsystem>() : nullptr;
}

EModularOnlineRole UModularFeatureSubsystem::GetFeatureRole() const
{
	const auto Settings = GetDefault<UModularOnlineSettings>();

	// The platform, because these components answer about people rather than about matches. A facade whose
	// component lives elsewhere overrides this.
	return Settings ? Settings->FeatureRole : EModularOnlineRole::Platform;
}

bool UModularFeatureSubsystem::IsAvailable() const
{
	const auto Online = GetOnline();

	return Online && Online->HasFeature(GetFeatureTag(), GetFeatureRole());
}

UE::Online::FAccountId UModularFeatureSubsystem::GetLocalAccount(const int32 LocalPlayerIndex) const
{
	const auto Users = GetUsers();
	const auto User = Users ? Users->GetUserForLocalPlayerIndex(LocalPlayerIndex) : nullptr;

	return User ? User->GetAccountId(GetFeatureRole()) : UE::Online::FAccountId { };
}

bool UModularFeatureSubsystem::ShouldStartListening(bool& bListening)
{
	const auto Online = GetOnline();
	const auto Context = Online ? Online->GetContext(GetFeatureRole()) : nullptr;
	const auto Services = Context ? Context->GetServices() : nullptr;

	if (ListeningServices != Services)
	{
		// Different services from the ones subscribed to: whatever was listening belongs to a world that
		// has gone, and the handle that held it is no longer worth anything.
		ListeningServices = Services;
		bListening = false;
	}

	return !bListening && Services.IsValid();
}

FModularOnlineResult UModularFeatureSubsystem::MissingFeature() const
{
	return FModularOnlineResult::NotSupported(GetFeatureTag());
}

FModularOnlineResult UModularFeatureSubsystem::NotSignedIn() const
{
	return FModularOnlineResult::FromOnlineError(UE::Online::Errors::NotLoggedIn());
}
