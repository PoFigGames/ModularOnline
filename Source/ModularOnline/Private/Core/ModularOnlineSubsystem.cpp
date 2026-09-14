// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Misc/StringBuilder.h"
#include "Online/OnlineServicesEngineUtils.h"
#include "UObject/UObjectHash.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularOnlineSubsystem)

using PoFigGames::Online::FModularOnlineContext;
using PoFigGames::Online::LexToString;

namespace PoFigGames::Online::Private
{
	/** Every role, in the order the log and the console commands report them. */
	static constexpr EModularOnlineRole AllRoles[] = { EModularOnlineRole::Default, EModularOnlineRole::Platform, EModularOnlineRole::Service };
}

void UModularOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The contexts are built on first use rather than here; see EnsureContexts.
}

void UModularOnlineSubsystem::Deinitialize()
{
	// Holding a services instance past shutdown keeps the whole provider alive, so let go of all of them.
	Contexts.Reset();
	BoundInstanceName = FName { };
	bContextsBuilt = false;

	Super::Deinitialize();
}

bool UModularOnlineSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is not a game specific subclass
	return ChildClasses.Num() == 0;
}

FName UModularOnlineSubsystem::GetBoundInstanceName() const
{
	EnsureContexts();

	return Contexts.IsEmpty() ? FName { } : BoundInstanceName;
}

EModularOnlineRole UModularOnlineSubsystem::ResolveRole(const EModularOnlineRole Role) const
{
	EnsureContexts();

	// Exactly the rule GetContext follows, said out loud so that callers who keep something per role can
	// key it the same way the lookups will ask for it.
	if (Contexts.Contains(Role))
	{
		return Role;
	}

	return Contexts.Contains(EModularOnlineRole::Default) ? EModularOnlineRole::Default : Role;
}

const FModularOnlineContext* UModularOnlineSubsystem::GetContext(const EModularOnlineRole Role) const
{
	EnsureContexts();

	if (const auto Found = Contexts.Find(Role))
	{
		return Found->Get();
	}

	// A role without a provider of its own is served by the default one, which is the shape of every
	// project that ships on a single backend.
	if (Role != EModularOnlineRole::Default)
	{
		if (const auto Fallback = Contexts.Find(EModularOnlineRole::Default))
		{
			return Fallback->Get();
		}
	}

	return nullptr;
}

bool UModularOnlineSubsystem::HasFeature(const FGameplayTag Feature, const EModularOnlineRole Role) const
{
	const auto Context = GetContext(Role);

	return Context && Context->HasFeature(Feature);
}

FGameplayTagContainer UModularOnlineSubsystem::GetFeatures(const EModularOnlineRole Role) const
{
	if (const auto Context = GetContext(Role))
	{
		return Context->GetFeatures();
	}

	return FGameplayTagContainer { };
}

FString UModularOnlineSubsystem::GetProviderName(const EModularOnlineRole Role) const
{
	if (const auto Context = GetContext(Role))
	{
		return Context->GetProviderName();
	}

	return FString { };
}

bool UModularOnlineSubsystem::HasDedicatedProvider(const EModularOnlineRole Role) const
{
	EnsureContexts();

	return Contexts.Contains(Role);
}

bool UModularOnlineSubsystem::HasAnyProvider() const
{
	EnsureContexts();

	return Contexts.Contains(EModularOnlineRole::Default);
}

void UModularOnlineSubsystem::RefreshCapabilities()
{
	EnsureContexts();

	for (const auto& Pair : Contexts)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->RefreshFeatures();
		}
	}

	UE_LOG(LogModularOnline, Log, TEXT("%s"), *DescribeCapabilities());
}

FString UModularOnlineSubsystem::DescribeCapabilities() const
{
	EnsureContexts();

	// Outside the editor the engine names no instance, and an empty one reads like something went missing.
	const auto InstanceLabel = BoundInstanceName.IsNone()
		? FString { TEXT("the default instance") }
		: FString::Printf(TEXT("instance '%s'"), *BoundInstanceName.ToString());

	TStringBuilder<1024> Builder;
	Builder.Appendf(TEXT("Modular Online contexts for %s:"), *InstanceLabel);

	for (const auto Role : PoFigGames::Online::Private::AllRoles)
	{
		const auto Context = GetContext(Role);
		const auto RoleName = LexToString(Role);

		if (!Context)
		{
			Builder.Appendf(TEXT("\n  %-8s : no provider"), *RoleName);
		}
		else
		{
			const auto bIsDedicated = Contexts.Contains(Role);
			Builder.Appendf(TEXT("\n  %-8s : %s%s, %d components"),
				*RoleName,
				*Context->GetProviderName(),
				bIsDedicated ? TEXT("") : TEXT(" (shared with Default)"),
				Context->GetFeatures().Num());

			for (const auto& Feature : Context->GetFeatures())
			{
				Builder.Appendf(TEXT("\n      %s"), *Feature.ToString());
			}
		}
	}

	return Builder.ToString();
}

void UModularOnlineSubsystem::EnsureContexts() const
{
	const auto GameInstance = GetGameInstance();
	const auto World = GameInstance ? GameInstance->GetWorld() : nullptr;

	// Asked on every getter and from every event handler, and answering it walks the engine's world
	// contexts. The same world cannot have a different instance name, so the question is only put when
	// the world is not the one already answered for.
	if (bContextsBuilt && World == BoundWorld)
	{
		return;
	}

	BoundWorld = World;

	if (const auto InstanceName = UE::Online::GetServicesInstanceName(World); !bContextsBuilt || InstanceName != BoundInstanceName)
	{
		BoundInstanceName = InstanceName;
		bContextsBuilt = true;

		CreateContexts();
	}
}

void UModularOnlineSubsystem::CreateContexts() const
{
	Contexts.Reset();

	const auto GameInstance = GetGameInstance();
	const auto World = GameInstance ? GameInstance->GetWorld() : nullptr;

	const auto DefaultServices = UE::Online::GetServices(World, UE::Online::EOnlineServices::Default);
	if (!DefaultServices.IsValid())
	{
		// Legitimate: a build with no online plugin, a commandlet, a platform with nothing configured.
		// Everything above this subsystem answers NotSupported and the game keeps running offline.
		UE_LOG(LogModularOnline, Log, TEXT("No online services are configured; the online layer stays offline."));

		return;
	}

	Contexts.Add(EModularOnlineRole::Default, MakeShared<FModularOnlineContext>(EModularOnlineRole::Default, DefaultServices));

	if (const auto PlatformServices = UE::Online::GetServices(World, UE::Online::EOnlineServices::Platform); PlatformServices.IsValid() && PlatformServices != DefaultServices)
	{
		Contexts.Add(EModularOnlineRole::Platform, MakeShared<FModularOnlineContext>(EModularOnlineRole::Platform, PlatformServices));
	}

	if (const auto ConfiguredService = GetConfiguredServiceProvider(); ConfiguredService != UE::Online::EOnlineServices::None)
	{
		if (const auto ServiceServices = UE::Online::GetServices(World, ConfiguredService); ServiceServices.IsValid() && ServiceServices != DefaultServices)
		{
			Contexts.Add(EModularOnlineRole::Service, MakeShared<FModularOnlineContext>(EModularOnlineRole::Service, ServiceServices));
		}
		else
		{
			// Asked for and not there. Falling back to the default provider quietly would leave a project
			// that configured two providers running on one and never being told which one it is on.
			UE_LOG(LogModularOnline, Error,
				TEXT("Service role is configured as '%s', which did not answer. Everything addressed to it answers the default provider instead; check that the provider's plugin is enabled."),
				LexToString(ConfiguredService));
		}
	}

	if (const auto Settings = GetDefault<UModularOnlineSettings>(); Settings && Settings->bLogCapabilitiesOnStartup)
	{
		UE_LOG(LogModularOnline, Log, TEXT("%s"), *DescribeCapabilities());
	}
}

UE::Online::EOnlineServices UModularOnlineSubsystem::GetConfiguredServiceProvider() const
{
	const auto Settings = GetDefault<UModularOnlineSettings>();
	if (!Settings || Settings->ServiceProvider.IsEmpty())
	{
		return UE::Online::EOnlineServices::None;
	}

	auto Provider = UE::Online::EOnlineServices::None;
	LexFromString(Provider, *Settings->ServiceProvider);

	// A name nobody recognises is a mistake in the configuration, not a decision to have no service role.
	// Left silent, the two are indistinguishable: the game simply behaves as though nothing was asked for.
	UE_CLOG(Provider == UE::Online::EOnlineServices::None, LogModularOnline, Error,
		TEXT("ServiceProvider is set to '%s', which names no provider. The service role will be missing; check [ModularOnline.Providers]."),
		*Settings->ServiceProvider);

	return Provider;
}
