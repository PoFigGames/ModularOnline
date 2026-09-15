// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularAchievementsSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/Achievements.h"
#include "Online/OnlineResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularAchievementsSubsystem)

void UModularAchievementsSubsystem::AnnounceAchievements(const TArray<FModularAchievement>& Achievements, const FModularOnlineResult& Result, const FModularAchievementsDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Achievements, Result);
	OnAchievementsQueried.Broadcast(Achievements, Result);
	K2_OnAchievementsQueried.Broadcast(Achievements, Result);
}

void UModularAchievementsSubsystem::Deinitialize()
{
	UnlockedHandle = UE::Online::FOnlineEventDelegateHandle { };
	bListeningToUnlocks = false;

	Super::Deinitialize();
}

FGameplayTag UModularAchievementsSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_Achievements;
}

void UModularAchievementsSubsystem::EnsureListening()
{
	const auto Achievements = GetInterface<UE::Online::IAchievements>();

	if (!ShouldStartListening(bListeningToUnlocks) || !Achievements.IsValid())
	{
		return;
	}

	UnlockedHandle = Achievements->OnAchievementStateUpdated().Add(this, &ThisClass::HandleAchievementsUnlocked);
	bListeningToUnlocks = true;
}

void UModularAchievementsSubsystem::HandleAchievementsUnlocked(const UE::Online::FAchievementStateUpdated& EventParameters)
{
	UE_LOG(LogModularOnline, Log, TEXT("%d achievement(s) were earned."), EventParameters.AchievementIds.Num());

	OnAchievementsUnlocked.Broadcast(EventParameters.AchievementIds);
	K2_OnAchievementsUnlocked.Broadcast(EventParameters.AchievementIds);
}

bool UModularAchievementsSubsystem::QueryAchievements(const int32 LocalPlayerIndex, FModularAchievementsDelegate OnComplete)
{
	const auto Achievements = GetInterface<UE::Online::IAchievements>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Achievements.IsValid())
	{
		AnnounceAchievements(TArray<FModularAchievement> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceAchievements(TArray<FModularAchievement> { }, NotSignedIn(), OnComplete);

		return false;
	}

	EnsureListening();

	// What exists first, then what this player did about it: the states are meaningless without the
	// definitions, and both fill caches this facade reads back together.
	Achievements->QueryAchievementDefinitions({ Account }).OnComplete(this, [this, Account, LocalPlayerIndex, OnComplete](const UE::Online::TOnlineResult<UE::Online::FQueryAchievementDefinitions>& DefinitionsResult)
	{
		if (DefinitionsResult.IsError())
		{
			AnnounceAchievements(TArray<FModularAchievement> { }, FModularOnlineResult::FromOnlineError(DefinitionsResult.GetErrorValue()), OnComplete);

			return;
		}

		const auto Achievements = GetInterface<UE::Online::IAchievements>();
		if (!Achievements.IsValid())
		{
			AnnounceAchievements(TArray<FModularAchievement> { }, MissingFeature(), OnComplete);

			return;
		}

		Achievements->QueryAchievementStates({ Account }).OnComplete(this, [this, LocalPlayerIndex, OnComplete](const UE::Online::TOnlineResult<UE::Online::FQueryAchievementStates>& StatesResult)
		{
			if (StatesResult.IsError())
			{
				AnnounceAchievements(TArray<FModularAchievement> { }, FModularOnlineResult::FromOnlineError(StatesResult.GetErrorValue()), OnComplete);

				return;
			}

			TArray<FModularAchievement> Found;
			GetAchievements(LocalPlayerIndex, Found);

			AnnounceAchievements(Found, FModularOnlineResult::Success(), OnComplete);
		});
	});

	return true;
}

bool UModularAchievementsSubsystem::GetAchievements(const int32 LocalPlayerIndex, TArray<FModularAchievement>& OutAchievements) const
{
	const auto Achievements = GetInterface<UE::Online::IAchievements>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Achievements.IsValid() || !Account.IsValid())
	{
		return false;
	}

	const auto Ids = Achievements->GetAchievementIds({ Account });
	if (!Ids.IsOk())
	{
		return false;
	}

	OutAchievements.Reset(Ids.GetOkValue().AchievementIds.Num());

	for (const auto& Id : Ids.GetOkValue().AchievementIds)
	{
		const auto Definition = Achievements->GetAchievementDefinition({ Account, Id });

		// An achievement the provider will not describe is one this game has nothing to show for.
		if (Definition.IsOk())
		{
			const auto& Found = Definition.GetOkValue().AchievementDefinition;

			FModularAchievement Achievement;
			Achievement.Id = Found.AchievementId;
			Achievement.UnlockedName = Found.UnlockedDisplayName;
			Achievement.UnlockedDescription = Found.UnlockedDescription;
			Achievement.LockedName = Found.LockedDisplayName;
			Achievement.LockedDescription = Found.LockedDescription;
			Achievement.bIsHidden = Found.bIsHidden;

			// A state the services have no record of means the player has not earned it, which is the
			// ordinary case and not a failure worth reporting.
			if (const auto State = Achievements->GetAchievementState({ Account, Id }); State.IsOk())
			{
				const auto& Earned = State.GetOkValue().AchievementState;

				// Progress is what the interface calls the answer - "a value of 1.0 means the achievement
				// is unlocked" - and a provider that fills in no unlock time would otherwise read as locked.
				Achievement.Progress = Earned.Progress;
				Achievement.UnlockTime = Earned.UnlockTime;
				Achievement.bIsUnlocked = Earned.Progress >= 1.0f;
			}

			Achievement.IconUrl = Achievement.bIsUnlocked ? Found.UnlockedIconUrl : Found.LockedIconUrl;

			OutAchievements.Add(MoveTemp(Achievement));
		}
	}

	return true;
}

bool UModularAchievementsSubsystem::UnlockAchievements(const int32 LocalPlayerIndex, const TArray<FString>& AchievementIds)
{
	const auto Achievements = GetInterface<UE::Online::IAchievements>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Achievements.IsValid() || !Account.IsValid() || AchievementIds.IsEmpty())
	{
		return false;
	}

	UE::Online::FUnlockAchievements::Params Params;
	Params.LocalAccountId = Account;
	Params.AchievementIds = AchievementIds;

	// The answer is nobody's to wait for, but a refusal that nothing reports is a write that
	// silently did not happen.
	Achievements->UnlockAchievements(MoveTemp(Params)).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FUnlockAchievements>& Result)
	{
		UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to unlock an achievement: %s"), *Result.GetErrorValue().GetLogString());
	});

	return true;
}

bool UModularAchievementsSubsystem::ShowAchievementsUI(const int32 LocalPlayerIndex)
{
	const auto Achievements = GetInterface<UE::Online::IAchievements>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!Achievements.IsValid() || !Account.IsValid())
	{
		return false;
	}

	UE::Online::FDisplayAchievementUI::Params Params;
	Params.LocalAccountId = Account;

	const auto Shown = Achievements->DisplayAchievementUI(MoveTemp(Params));

	return Shown.IsOk();
}
