// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularStatsSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineResult.h"
#include "Online/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularStatsSubsystem)

void UModularStatsSubsystem::AnnounceStats(const FModularStatMap& Stats, const FModularOnlineResult& Result, const FModularStatsDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Stats, Result);
	OnStatsQueried.Broadcast(Stats, Result);
	K2_OnStatsQueried.Broadcast(Result);
}

namespace PoFigGames::Online::Private
{
	/** A value of this plugin as the services carry it. */
	static UE::Online::FStatValue ToStatValue(const FModularStatValue& Value)
	{
		switch (Value.Kind)
		{
		case EModularStatKind::Float:
			return UE::Online::FStatValue { Value.FloatValue };

		case EModularStatKind::Boolean:
			return UE::Online::FStatValue { Value.bBoolValue };

		case EModularStatKind::Text:
			return UE::Online::FStatValue { Value.TextValue };

		case EModularStatKind::Integer:
			break;
		}

		return UE::Online::FStatValue { Value.IntValue };
	}

	/** And back again, keeping the kind so that a caller knows which field it may read. */
	static FModularStatValue FromStatValue(const UE::Online::FStatValue& Value)
	{
		FModularStatValue Converted;

		switch (Value.VariantType)
		{
		case UE::Online::ESchemaAttributeType::Double:
			Converted.Kind = EModularStatKind::Float;
			Converted.FloatValue = Value.GetDouble();
			break;

		case UE::Online::ESchemaAttributeType::Bool:
			Converted.Kind = EModularStatKind::Boolean;
			Converted.bBoolValue = Value.GetBoolean();
			break;

		case UE::Online::ESchemaAttributeType::String:
			Converted.Kind = EModularStatKind::Text;
			Converted.TextValue = Value.GetString();
			break;

		case UE::Online::ESchemaAttributeType::Int64:
			Converted.Kind = EModularStatKind::Integer;
			Converted.IntValue = Value.GetInt64();
			break;

		case UE::Online::ESchemaAttributeType::None:
			break;
		}

		return Converted;
	}
}

void UModularStatsSubsystem::Deinitialize()
{
	StatsUpdatedHandle = UE::Online::FOnlineEventDelegateHandle { };
	bListeningToStats = false;

	Super::Deinitialize();
}

FGameplayTag UModularStatsSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_Stats;
}

void UModularStatsSubsystem::EnsureListening()
{
	const auto Stats = GetInterface<UE::Online::IStats>();

	if (!ShouldStartListening(bListeningToStats) || !Stats.IsValid())
	{
		return;
	}

	StatsUpdatedHandle = Stats->OnStatsUpdated().Add(this, &ThisClass::HandleStatsUpdated);
	bListeningToStats = true;
}

void UModularStatsSubsystem::HandleStatsUpdated(const UE::Online::FStatsUpdated& EventParameters)
{
	const auto AccountId = MakeModularAccount(EventParameters.LocalAccountId);

	OnStatsUpdated.Broadcast(AccountId);
	K2_OnStatsUpdated.Broadcast(AccountId);
}

bool UModularStatsSubsystem::UpdateStats(const int32 LocalPlayerIndex, const TMap<FString, FModularStatValue>& Stats)
{
	const auto StatsInterface = GetInterface<UE::Online::IStats>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!StatsInterface.IsValid() || !Account.IsValid() || Stats.IsEmpty())
	{
		UE_LOG(LogModularOnline, Verbose, TEXT("No statistics were sent for player %d."), LocalPlayerIndex);

		return false;
	}

	UE::Online::FUserStats UserStats;
	UserStats.AccountId = Account;

	for (const auto& Stat : Stats)
	{
		UserStats.Stats.Emplace(Stat.Key, PoFigGames::Online::Private::ToStatValue(Stat.Value));
	}

	UE::Online::FUpdateStats::Params Params;
	Params.LocalAccountId = Account;
	Params.UpdateUsersStats = { MoveTemp(UserStats) };

	// The answer is nobody's to wait for, but a refusal that nothing reports is a write that
	// silently did not happen.
	StatsInterface->UpdateStats(MoveTemp(Params)).OnComplete(this, [this](const UE::Online::TOnlineResult<UE::Online::FUpdateStats>& Result)
	{
		UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("The services refused to write a stat: %s"), *Result.GetErrorValue().GetLogString());
	});

	return true;
}

bool UModularStatsSubsystem::QueryStats(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, const TArray<FString>& StatNames, FModularStatsDelegate OnComplete)
{
	const auto StatsInterface = GetInterface<UE::Online::IStats>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = TargetAccountId.IsValid() ? TargetAccountId.AccountId : Account;

	if (!StatsInterface.IsValid())
	{
		AnnounceStats(TMap<FString, FModularStatValue> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid() || !Target.IsValid())
	{
		AnnounceStats(TMap<FString, FModularStatValue> { }, NotSignedIn(), OnComplete);

		return false;
	}

	EnsureListening();

	UE::Online::FQueryStats::Params Params;
	Params.LocalAccountId = Account;
	Params.TargetAccountId = Target;
	Params.StatNames = StatNames;

	StatsInterface->QueryStats(MoveTemp(Params)).OnComplete(this, [this, OnComplete](const UE::Online::TOnlineResult<UE::Online::FQueryStats>& Result)
	{
		if (Result.IsError())
		{
			AnnounceStats(TMap<FString, FModularStatValue> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), OnComplete);

			return;
		}

		TMap<FString, FModularStatValue> Stats;
		Stats.Reserve(Result.GetOkValue().Stats.Num());

		for (const auto& Stat : Result.GetOkValue().Stats)
		{
			Stats.Emplace(Stat.Key, PoFigGames::Online::Private::FromStatValue(Stat.Value));
		}

		AnnounceStats(Stats, FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularStatsSubsystem::GetCachedStats(const int32 LocalPlayerIndex, const FModularAccountHandle& TargetAccountId, TMap<FString, FModularStatValue>& OutStats) const
{
	const auto StatsInterface = GetInterface<UE::Online::IStats>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);
	const auto Target = TargetAccountId.IsValid() ? TargetAccountId.AccountId : Account;

	if (!StatsInterface.IsValid() || !Target.IsValid())
	{
		return false;
	}

	// The whole cache, for every player: FGetCachedStats::Params carries no fields to narrow it with
	// (Engine 5.8.3, Online/Stats.h:124, checked on 2026-09-13).
	const auto Cached = StatsInterface->GetCachedStats({ });
	if (!Cached.IsOk())
	{
		return false;
	}

	// Only one entry can be the player asked about, so it is found rather than walked past.
	const auto Found = Cached.GetOkValue().UsersStats.FindByPredicate(
		[&Target](const UE::Online::FUserStats& UserStats) { return UserStats.AccountId == Target; });

	if (Found)
	{
		OutStats.Reset();

		for (const auto& Stat : Found->Stats)
		{
			OutStats.Emplace(Stat.Key, PoFigGames::Online::Private::FromStatValue(Stat.Value));
		}

		return true;
	}

	return false;
}
