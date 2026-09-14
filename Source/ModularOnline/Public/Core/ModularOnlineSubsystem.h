// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineContext.h"
#include "Core/ModularOnlineTypes.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "ModularOnlineSubsystem.generated.h"


/**
 * @class UModularOnlineSubsystem
 *
 * @brief The floor every other part of this plugin stands on: which services answer, and what they can do.
 */
UCLASS(MinimalAPI, BlueprintType)
class UModularOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UModularOnlineSubsystem() { }

#pragma region UGameInstanceSubsystem

	MODULARONLINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	MODULARONLINE_API virtual void Deinitialize() override;

	MODULARONLINE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

#pragma endregion UGameInstanceSubsystem

	/**
	 * The services filling a role, with the fallbacks applied: a project with one provider gets that
	 * provider for every role. Null only when no provider answered at all, which is a legitimate state -
	 * an offline build, a commandlet, a platform with nothing configured - and not a failure to assert on.
	 */
	MODULARONLINE_API const PoFigGames::Online::FModularOnlineContext* GetContext(EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Any component of the services filling a role, standard or provider specific. */
	template <typename InterfaceType>
	TSharedPtr<InterfaceType> GetInterface(const EModularOnlineRole Role = EModularOnlineRole::Default) const
	{
		if (const auto Context = GetContext(Role))
		{
			return Context->GetInterface<InterfaceType>();
		}

		return nullptr;
	}

	/** The role that actually serves another one. */
	MODULARONLINE_API EModularOnlineRole ResolveRole(EModularOnlineRole Role) const;

	/** The services instance the contexts currently belong to. */
	MODULARONLINE_API FName GetBoundInstanceName() const;

	/** Whether the services filling a role implement a component. This is what a screen hides itself on. */
	MODULARONLINE_API bool HasFeature(FGameplayTag Feature, EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Everything the services filling a role implement, as Online.Feature.* tags. */
	MODULARONLINE_API FGameplayTagContainer GetFeatures(EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Name of the provider filling a role: Steam, Epic, Null. Empty when the role has no provider. */
	MODULARONLINE_API FString GetProviderName(EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** True when this role is served by a provider of its own rather than falling back to the default one. */
	MODULARONLINE_API bool HasDedicatedProvider(EModularOnlineRole Role) const;

	/** True when any provider answered at all. False in a build that ships with no online services. */
	MODULARONLINE_API bool HasAnyProvider() const;

	/** Asks every context again what it implements, and logs the result. Debug aid. */
	MODULARONLINE_API void RefreshCapabilities();

	/** One line per role: provider and component count. Debug aid, also used by the console commands. */
	MODULARONLINE_API FString DescribeCapabilities() const;

protected:

	/** Contexts that were actually created. A role missing from here falls back to Default. */
	mutable TMap<EModularOnlineRole, TSharedPtr<PoFigGames::Online::FModularOnlineContext>> Contexts { };

	/** The world the contexts were built for, so that a different one rebuilds them. */
	mutable FName BoundInstanceName { };

	/** The world the name above was answered for; the same world cannot have a different one. */
	mutable TWeakObjectPtr<const UWorld> BoundWorld { nullptr };

	/** False until the contexts have been built once. */
	mutable bool bContextsBuilt { false };

	/** Builds the contexts for the world this game instance is running, if that has not happened yet. */
	MODULARONLINE_API void EnsureContexts() const;

	/** Creates one context per role, applying the configuration and the fallbacks. */
	MODULARONLINE_API virtual void CreateContexts() const;

	/** The provider named in the settings for the service role, or None when there is none. */
	MODULARONLINE_API UE::Online::EOnlineServices GetConfiguredServiceProvider() const;

	/** What Blueprint may ask of this subsystem. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline", meta = (DisplayName = "Has Feature"))
	bool K2_HasFeature(FGameplayTag Feature, EModularOnlineRole Role = EModularOnlineRole::Default) const { return HasFeature(Feature, Role); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline", meta = (DisplayName = "Get Features"))
	FGameplayTagContainer K2_GetFeatures(EModularOnlineRole Role = EModularOnlineRole::Default) const { return GetFeatures(Role); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline", meta = (DisplayName = "Get Provider Name"))
	FString K2_GetProviderName(EModularOnlineRole Role = EModularOnlineRole::Default) const { return GetProviderName(Role); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline", meta = (DisplayName = "Has Dedicated Provider"))
	bool K2_HasDedicatedProvider(EModularOnlineRole Role) const { return HasDedicatedProvider(Role); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline", meta = (DisplayName = "Has Any Provider"))
	bool K2_HasAnyProvider() const { return HasAnyProvider(); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline", meta = (DisplayName = "Refresh Capabilities"))
	void K2_RefreshCapabilities() { RefreshCapabilities(); }
};
