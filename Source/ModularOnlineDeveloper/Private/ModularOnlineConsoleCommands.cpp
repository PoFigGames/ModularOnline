// Copyright PoFig Games Studio. All Rights Reserved.

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSubsystem.h"

namespace PoFigGames::Online::Private
{
	/** The subsystem of the world the command was typed in. */
	static UModularOnlineSubsystem* FindSubsystem(const UWorld* World)
	{
		const auto GameInstance = World ? World->GetGameInstance() : nullptr;
		const auto Subsystem = GameInstance ? GameInstance->GetSubsystem<UModularOnlineSubsystem>() : nullptr;

		if (!Subsystem)
		{
			UE_LOG(LogModularOnline, Warning, TEXT("No Modular Online subsystem in this world."));
		}

		return Subsystem;
	}

	static FAutoConsoleCommandWithWorldAndArgs GStatusCommand(
		TEXT("ModularOnline.Status"),
		TEXT("Prints the provider and the components of every online role of this game instance."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (const auto Subsystem = FindSubsystem(World))
			{
				UE_LOG(LogModularOnline, Display, TEXT("%s"), *Subsystem->DescribeCapabilities());
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GRefreshCommand(
		TEXT("ModularOnline.Refresh"),
		TEXT("Asks every online role again which components it implements, then prints the result."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& /*Args*/, UWorld* World)
		{
			if (const auto Subsystem = FindSubsystem(World))
			{
				Subsystem->RefreshCapabilities();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs GHasFeatureCommand(
		TEXT("ModularOnline.HasFeature"),
		TEXT("ModularOnline.HasFeature <Online.Feature.X> [Default|Platform|Service] - answers what a screen would ask."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogModularOnline, Warning, TEXT("Usage: ModularOnline.HasFeature <Online.Feature.X> [Default|Platform|Service]"));

				return;
			}

			const auto Subsystem = FindSubsystem(World);
			if (!Subsystem)
			{
				return;
			}

			const auto Feature = FGameplayTag::RequestGameplayTag(FName(*Args[0]), false);
			if (!Feature.IsValid())
			{
				UE_LOG(LogModularOnline, Warning, TEXT("'%s' is not a known gameplay tag."), *Args[0]);

				return;
			}

			auto Role = EModularOnlineRole::Default;
			if (Args.Num() > 1)
			{
				if (Args[1].Equals(TEXT("Platform"), ESearchCase::IgnoreCase))
				{
					Role = EModularOnlineRole::Platform;
				}
				else if (Args[1].Equals(TEXT("Service"), ESearchCase::IgnoreCase))
				{
					Role = EModularOnlineRole::Service;
				}
				else if (!Args[1].Equals(TEXT("Default"), ESearchCase::IgnoreCase))
				{
					UE_LOG(LogModularOnline, Warning, TEXT("'%s' is not a role; ask about Default, Platform or Service."), *Args[1]);

					return;
				}
			}

			UE_LOG(LogModularOnline, Display, TEXT("%s on %s: %s"),
				*Feature.ToString(),
				*Subsystem->GetProviderName(Role),
				Subsystem->HasFeature(Feature, Role) ? TEXT("yes") : TEXT("no"));
		}));
}
