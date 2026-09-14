// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineContext.h"
#include "Core/ModularOnlineSubsystem.h"
#include "Core/ModularOnlineTypes.h"
#include "GameplayTagContainer.h"
#include "Online/CoreOnline.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ModularFeatureSubsystem.generated.h"

class UModularUserSubsystem;


/**
 * @class UModularFeatureSubsystem
 *
 * @brief What every facade over an online component needs, written once.
 */
UCLASS(MinimalAPI, Abstract)
class UModularFeatureSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

#pragma region UGameInstanceSubsystem

	MODULARONLINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma endregion UGameInstanceSubsystem

	/** True when the provider of this platform implements what this facade is built on. */
	MODULARONLINE_API bool IsAvailable() const;

	/** The component this facade stands on, which is also what a refusal names. */
	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const PURE_VIRTUAL(UModularFeatureSubsystem::GetFeatureTag, return FGameplayTag { };);

protected:
	/** The online layer, which owns the contexts. */
	MODULARONLINE_API UModularOnlineSubsystem* GetOnline() const;

	/** The player layer, which knows who is signed in locally. */
	MODULARONLINE_API UModularUserSubsystem* GetUsers() const;

	/** Whether a subscription still has to be made, and forgets one made against services that are gone. */
	MODULARONLINE_API bool ShouldStartListening(bool& bListening);

	/** The role these calls are addressed to. */
	MODULARONLINE_API virtual EModularOnlineRole GetFeatureRole() const;

	/** The account of a local player on that role, invalid when they are not signed in there. */
	MODULARONLINE_API UE::Online::FAccountId GetLocalAccount(int32 LocalPlayerIndex) const;

	/** The component of the addressed role, or nothing when the provider has none. */
	template <typename InterfaceType>
	TSharedPtr<InterfaceType> GetInterface() const
	{
		const auto Online = GetOnline();

		return Online ? Online->GetInterface<InterfaceType>(GetFeatureRole()) : nullptr;
	}

	/** The refusal this facade answers with while its component is missing. */
	MODULARONLINE_API FModularOnlineResult MissingFeature() const;

	/** The refusal for a call made for a player who is not signed in. */
	MODULARONLINE_API FModularOnlineResult NotSignedIn() const;

private:
	/** The services the subscription above was made against, so that a rebuild is noticed. */
	TWeakPtr<UE::Online::IOnlineServices> ListeningServices { };
};
