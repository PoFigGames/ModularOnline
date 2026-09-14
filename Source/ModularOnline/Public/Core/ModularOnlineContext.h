// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "GameplayTagContainer.h"
#include "Online/CoreOnline.h"
#include "Online/OnlineServices.h"


namespace PoFigGames::Online
{
	/**
	 * @class FModularOnlineContext
	 *
	 * @brief One online services instance, the role it fills and the components it turned out to have.
	 */
	class FModularOnlineContext
	{
	public:
		MODULARONLINE_API FModularOnlineContext(EModularOnlineRole InRole, UE::Online::IOnlineServicesPtr InServices);

		/** The role this instance was created for, which is not necessarily the only one it serves. */
		EModularOnlineRole GetRole() const { return Role; }

		/** False when no provider answered for this role. */
		bool IsValid() const { return Services.IsValid(); }

		/** Which provider is behind this instance. */
		MODULARONLINE_API UE::Online::EOnlineServices GetProvider() const;

		/** Provider name for logs and for a screen that tells the player where they are signed in. */
		MODULARONLINE_API FString GetProviderName() const;

		/** The services themselves, for the few places that need them whole. */
		const UE::Online::IOnlineServicesPtr& GetServices() const { return Services; }

		/** Every component this instance implements, as Online.Feature.* tags. */
		const FGameplayTagContainer& GetFeatures() const { return Features; }

		/** Whether this instance implements one particular component. */
		bool HasFeature(const FGameplayTag Feature) const { return Features.HasTagExact(Feature); }

		/** Any component of the online services, standard or provider specific. */
		template <typename InterfaceType>
		TSharedPtr<InterfaceType> GetInterface() const
		{
			return Services.IsValid() ? Services->GetInterface<InterfaceType>() : nullptr;
		}

		/** Asks the instance again what it implements. Cheap, and the answer can change with a provider. */
		MODULARONLINE_API void RefreshFeatures();

	private:
		/** The role this instance was created for. */
		EModularOnlineRole Role { EModularOnlineRole::Default };

		/** The services instance, bound to one world. */
		UE::Online::IOnlineServicesPtr Services { nullptr };

		/** What the instance implements, filled by RefreshFeatures. */
		FGameplayTagContainer Features { };
	};
}
