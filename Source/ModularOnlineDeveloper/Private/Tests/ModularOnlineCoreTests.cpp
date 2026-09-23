// Copyright PoFig Games Studio. All Rights Reserved.

#include "Containers/Ticker.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/AutomationTest.h"

#include "Match/ModularLobbyBackend.h"
#include "Match/ModularMatchBackend.h"
#include "Match/ModularMatchSubsystem.h"
#include "Match/ModularMatchTypes.h"
#include "Match/ModularSessionBackend.h"
#include "Engine/GameInstance.h"
#include "Features/ModularPresenceSubsystem.h"
#include "Tests/ModularOnlineTestProbes.h"
#include "Core/ModularOnlineContext.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineTags.h"
#include "Core/ModularOnlineTypes.h"
#include "User/ModularUserInfo.h"
#include "User/ModularUserSubsystem.h"
#include "User/ModularUserTags.h"
#include "User/ModularUserTypes.h"
#include "Online/Auth.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineServices.h"
#include "Online/Privileges.h"

#if WITH_AUTOMATION_TESTS

namespace PoFigGames::Online::Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineResultTest, "ModularOnline.Core.Result", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineResultTest::RunTest(const FString& /*Parameters*/)
{
	using namespace UE::Online;

	{
		const auto Result = FModularOnlineResult::Success();
		TestTrue(TEXT("Success is successful"), Result.bWasSuccessful);
		TestEqual(TEXT("Success carries no category"), Result.Category, EModularOnlineErrorCategory::None);
	}

	{
		const auto Result = FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Sessions);
		TestFalse(TEXT("A missing component is a failure"), Result.bWasSuccessful);
		TestEqual(TEXT("A missing component is categorised as such"), Result.Category, EModularOnlineErrorCategory::NotSupported);
		TestEqual(TEXT("The missing component is named"), Result.MissingFeature, ModularOnlineTags::Feature_Sessions.GetTag());
		TestFalse(TEXT("A missing component says something to the player"), Result.ErrorText.IsEmpty());
	}

	{
		const auto Result = FModularOnlineResult::FromOnlineError(Errors::Success());
		TestTrue(TEXT("The success error is not a failure"), Result.bWasSuccessful);
	}

	// The categories a screen branches on, and the errors that have to land in them.
	const TArray<TPair<UE::Online::FOnlineError, EModularOnlineErrorCategory>> Mappings
	{
		{ Errors::NotImplemented(), EModularOnlineErrorCategory::NotSupported },
		{ Errors::MissingInterface(), EModularOnlineErrorCategory::NotSupported },
		{ Errors::NotLoggedIn(), EModularOnlineErrorCategory::NotLoggedIn },
		{ Errors::NoConnection(), EModularOnlineErrorCategory::NoConnection },
		{ Errors::AccessDenied(), EModularOnlineErrorCategory::AccessDenied },
		{ Errors::Cancelled(), EModularOnlineErrorCategory::Cancelled },
		{ Errors::Timeout(), EModularOnlineErrorCategory::TimedOut },
		{ Errors::RequestFailure(), EModularOnlineErrorCategory::Unknown },
	};

	for (const auto& Mapping : Mappings)
	{
		const auto Translated = FModularOnlineResult::FromOnlineError(Mapping.Key);

		TestEqual(*FString::Printf(TEXT("%s lands in the right category"), *Mapping.Key.GetErrorId()), Translated.Category, Mapping.Value);
	}

	{
		const auto Result = FModularOnlineResult::FromOnlineError(Errors::NoConnection());
		TestFalse(TEXT("A failure carries the error id of the services"), Result.ErrorId.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineContextTest, "ModularOnline.Core.Context", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineContextTest::RunTest(const FString& /*Parameters*/)
{
	using namespace PoFigGames::Online;

	// What the type itself guarantees, asserted on every machine: an offline build and a commandlet both
	// reach this state, and it has to be survivable rather than merely untested here.
	const FModularOnlineContext Nothing { EModularOnlineRole::Default, nullptr };

	TestFalse(TEXT("A context without services is not valid"), Nothing.IsValid());
	TestEqual(TEXT("A context without services implements nothing"), Nothing.GetFeatures().Num(), 0);
	TestEqual(TEXT("A context without services has no provider"), Nothing.GetProvider(), UE::Online::EOnlineServices::None);
	TestFalse(TEXT("And it implements no component by name either"), Nothing.HasFeature(ModularOnlineTags::Feature_Auth));

	// The rest depends on what this machine has configured, so it reports rather than asserts. Asking for
	// the services initialises the SDK, and failing to do so logs an error the framework counts as a test
	// failure; a negative count means "ignore if present".
	AddExpectedMessagePlain(TEXT("InitEx failed"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, -1);
	AddExpectedMessagePlain(TEXT("Failed to initialize a "), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, -1);

	// Taken without a world on purpose: this runs outside a game instance, and the shared instance is the
	// only one there is here.
	const auto Services = UE::Online::GetServices(UE::Online::EOnlineServices::Default);

	if (!Services.IsValid())
	{
		AddInfo(TEXT("No online services are configured on this machine; discovery was not exercised."));

		return true;
	}

	const FModularOnlineContext Context { EModularOnlineRole::Default, Services };

	TestTrue(TEXT("A context over real services is valid"), Context.IsValid());
	TestNotEqual(TEXT("A context over real services names its provider"), Context.GetProvider(), UE::Online::EOnlineServices::None);

	AddInfo(FString::Printf(TEXT("Provider '%s' implements %d components: %s"),
		*Context.GetProviderName(),
		Context.GetFeatures().Num(),
		*Context.GetFeatures().ToStringSimple()));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlinePrivilegeTest, "ModularOnline.Core.Privileges", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlinePrivilegeTest::RunTest(const FString& /*Parameters*/)
{
	using namespace UE::Online;

	// Every privilege this plugin names has to survive the trip to the services and back, or an answer
	// would end up filed against the wrong question.
	for (auto Index = 0; Index < static_cast<int32>(EModularOnlinePrivilege::Count); ++Index)
	{
		const auto Privilege = static_cast<EModularOnlinePrivilege>(Index);
		const auto RoundTripped = FModularPrivilegeConversions::FromOnlineServices(FModularPrivilegeConversions::ToOnlineServices(Privilege));

		TestEqual(*FString::Printf(TEXT("Privilege %d survives the round trip"), Index), RoundTripped, Privilege);
	}

	// No failures means granted, whatever was asked about.
	TestEqual(TEXT("No failures is available"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanPlayOnline, EPrivilegeResults::NoFailures),
		EModularOnlinePrivilegeResult::Available);

	// The services answer with a bitfield; the player is told the one thing they can act on.
	TestEqual(TEXT("Not signed in is reported as such"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanPlayOnline, EPrivilegeResults::UserNotLoggedIn),
		EModularOnlinePrivilegeResult::NotLoggedIn);

	TestEqual(TEXT("A required patch is a version problem"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanPlayOnline, EPrivilegeResults::RequiredPatchAvailable),
		EModularOnlinePrivilegeResult::VersionOutdated);

	TestEqual(TEXT("Parental controls are reported as an age restriction"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanCommunicateViaTextOnline, EPrivilegeResults::AgeRestrictionFailure),
		EModularOnlinePrivilegeResult::AgeRestricted);

	TestEqual(TEXT("A chat restriction is an account restriction"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanCommunicateViaTextOnline, EPrivilegeResults::ChatRestriction),
		EModularOnlinePrivilegeResult::AccountUseRestricted);

	// Signing out is the one the player can act on even when the platform also complains about the patch.
	TestEqual(TEXT("Several failures at once report the one the player can act on first"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanPlayOnline, EPrivilegeResults::UserNotLoggedIn | EPrivilegeResults::RequiredPatchAvailable),
		EModularOnlinePrivilegeResult::NotLoggedIn);

	// Being unable to play at all, with nothing else said, is what an unowned game looks like.
	TestEqual(TEXT("An unexplained refusal to play is a licence problem"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanPlay, EPrivilegeResults::GenericFailure),
		EModularOnlinePrivilegeResult::LicenseInvalid);

	TestEqual(TEXT("An unexplained refusal of anything else is a platform failure"),
		FModularPrivilegeConversions::FromOnlineServices(EUserPrivileges::CanPlayOnline, EPrivilegeResults::GenericFailure),
		EModularOnlinePrivilegeResult::PlatformFailure);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineSettingsTest, "ModularOnline.Core.Settings", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineSettingsTest::RunTest(const FString& /*Parameters*/)
{
	const auto Accounts = NewObject<UModularAccountSettings>();
	const auto Roles = NewObject<UModularOnlineSettings>();

	Accounts->DefaultAvatarAttribute = TEXT("AvatarUrl");
	Accounts->AvatarAttributeByProvider.Add(TEXT("Steam"), TEXT("avatar_url_full"));

	// A provider that named its own key gets it; everyone else gets the default. This is what lets a new
	// platform be supported by an ini entry rather than by a rebuild.
	TestEqual(TEXT("A named provider gets its own attribute"), Accounts->GetAvatarAttribute(TEXT("Steam")), FName(TEXT("avatar_url_full")));
	TestEqual(TEXT("An unnamed provider falls back to the default"), Accounts->GetAvatarAttribute(TEXT("Epic")), FName(TEXT("AvatarUrl")));

	// The store is addressed separately from matches on purpose: the backend that runs a project's
	// sessions is rarely the storefront that took the player's money. A project where they are the same
	// says so in the ini rather than discovering an empty catalogue at runtime.
	TestEqual(TEXT("Matches default to the service role"), Roles->MatchRole, EModularOnlineRole::Service);
	TestEqual(TEXT("The store defaults to the platform role"), Roles->StoreRole, EModularOnlineRole::Platform);

	// Emptying the default is how a project says that avatars are not read at all.
	Accounts->DefaultAvatarAttribute = FName { };
	TestTrue(TEXT("An unset default means no avatar is read"), Accounts->GetAvatarAttribute(TEXT("Epic")).IsNone());
	TestEqual(TEXT("An unset default does not affect a named provider"), Accounts->GetAvatarAttribute(TEXT("Steam")), FName(TEXT("avatar_url_full")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineMatchSettingsTest, "ModularOnline.Match.Settings", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineMatchSettingsTest::RunTest(const FString& /*Parameters*/)
{
	FModularMatchSettings Settings;
	Settings.MapId = FPrimaryAssetId(TEXT("Map"), TEXT("/Game/Maps/TestMap"));

	// A map the asset manager does not know by that id is still travelled to when the id carries a
	// package path, which is what an asset picker writes for an unregistered world.
	TestEqual(TEXT("A package path is taken as the map"), Settings.GetMapName(), FString(TEXT("/Game/Maps/TestMap")));

	Settings.OnlineMode = EModularMatchOnlineMode::Offline;
	TestEqual(TEXT("An offline match does not listen"), Settings.ConstructTravelURL(), FString(TEXT("/Game/Maps/TestMap")));

	Settings.OnlineMode = EModularMatchOnlineMode::Online;
	TestEqual(TEXT("An online match listens"), Settings.ConstructTravelURL(), FString(TEXT("/Game/Maps/TestMap?listen")));

	Settings.OnlineMode = EModularMatchOnlineMode::LAN;
	TestEqual(TEXT("A LAN match says so and listens"), Settings.ConstructTravelURL(), FString(TEXT("/Game/Maps/TestMap?bIsLanMatch?listen")));

	Settings.OnlineMode = EModularMatchOnlineMode::Online;
	Settings.ExtraArgs.Add(TEXT("Difficulty"), TEXT("Hard"));
	Settings.ExtraArgs.Add(TEXT("Tutorial"), FString { });
	const auto WithArguments = Settings.ConstructTravelURL();
	TestTrue(TEXT("An argument with a value is written as one"), WithArguments.Contains(TEXT("?Difficulty=Hard")));
	TestTrue(TEXT("An argument without a value is written bare"), WithArguments.Contains(TEXT("?Tutorial")));

	// A URL of options with no map in front of it is a relative one, and the engine would fill the map in
	// from wherever the game already is.
	FModularMatchSettings Unknown;
	Unknown.OnlineMode = EModularMatchOnlineMode::Online;

	TestTrue(TEXT("A match with no map has nowhere to travel"), Unknown.ConstructTravelURL().IsEmpty());

	Unknown.OnlineMode = EModularMatchOnlineMode::LAN;
	Unknown.ExtraArgs.Add(TEXT("Difficulty"), TEXT("Hard"));

	TestTrue(TEXT("Neither do its options give it one"), Unknown.ConstructTravelURL().IsEmpty());


#if WITH_SERVER_CODE
	FText Error;
	TestTrue(TEXT("Settings with a map and room are hostable"), Settings.Validate(Error));

	Settings.MaxPlayers = 0;
	TestFalse(TEXT("A match with no room is refused"), Settings.Validate(Error));
	TestFalse(TEXT("The refusal says why"), Error.IsEmpty());

	Settings.MaxPlayers = 4;
	Settings.MapId = FPrimaryAssetId { };
	TestFalse(TEXT("A match with no map is refused"), Settings.Validate(Error));
#endif

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineMatchHandleTest, "ModularOnline.Match.Handle", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineMatchHandleTest::RunTest(const FString& /*Parameters*/)
{
	const FModularMatchHandle Empty;
	TestFalse(TEXT("A handle that names nothing is invalid"), Empty.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineBackendRefusalTest, "ModularOnline.Match.BackendRefusal", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineBackendRefusalTest::RunTest(const FString& /*Parameters*/)
{
	using namespace PoFigGames::Online;

	// Each backend answers for the component it is built on, and says so by name. This is what keeps a
	// project that configured one of them from quietly getting the other: there is no other to get.
	const FModularLobbyBackend Lobbies;
	const FModularSessionBackend Sessions;

	TestEqual(TEXT("The lobby backend stands on lobbies"), Lobbies.GetRequiredFeature(), ModularOnlineTags::Feature_Lobbies.GetTag());
	TestEqual(TEXT("The session backend stands on sessions"), Sessions.GetRequiredFeature(), ModularOnlineTags::Feature_Sessions.GetTag());

	// An empty context is what a call gets on a platform whose provider has neither component, or before
	// anybody signed in. The answer has to name the missing component rather than fail vaguely.
	const FModularMatchContext Nothing;
	TestFalse(TEXT("A context without services is not valid"), Nothing.IsValid());

	FModularMatchSettings Settings;
	Settings.MapId = FPrimaryAssetId(TEXT("Map"), TEXT("/Game/Maps/TestMap"));

	for (const auto& Backend : TArray<const IModularMatchBackend*> { &Lobbies, &Sessions })
	{
		auto bAnswered = false;
		FModularOnlineResult Answer;

		const auto OnComplete = FModularMatchOperationDelegate::CreateLambda([&bAnswered, &Answer](const FModularOnlineResult& Result)
		{
			bAnswered = true;
			Answer = Result;
		});

		const_cast<IModularMatchBackend*>(Backend)->CreateMatch(Nothing, Settings, OnComplete);

		TestTrue(TEXT("A backend without services still answers"), bAnswered);
		TestFalse(TEXT("And the answer is a refusal"), Answer.bWasSuccessful);
		TestEqual(TEXT("Named as unsupported rather than as a failure"), Answer.Category, EModularOnlineErrorCategory::NotSupported);
		TestEqual(TEXT("And it names the component that was missing"), Answer.MissingFeature, Backend->GetRequiredFeature());
	}

	// A search answers the same way, with an empty list rather than no answer at all.
	auto bSearchAnswered = false;
	auto FoundCount = -1;
	FModularOnlineResult SearchAnswer;

	const auto OnSearch = FModularMatchSearchDelegate::CreateLambda([&](const FModularOnlineResult& Result, const TArray<FModularMatchInfo>& Matches)
	{
		bSearchAnswered = true;
		SearchAnswer = Result;
		FoundCount = Matches.Num();
	});

	FModularLobbyBackend Searching;
	Searching.FindMatches(Nothing, FModularMatchSearchParams { }, OnSearch);

	TestTrue(TEXT("A search without services answers"), bSearchAnswered);
	TestFalse(TEXT("With a refusal"), SearchAnswer.bWasSuccessful);
	TestEqual(TEXT("And an empty list, not an absent one"), FoundCount, 0);

	// A lobby is a service record, so there is no local-network one to open. Answering anything but a
	// refusal would publish an internet match to somebody who asked for a LAN game.
	auto LanSettings = Settings;
	LanSettings.OnlineMode = EModularMatchOnlineMode::LAN;

	auto bLanAnswered = false;
	FModularOnlineResult LanAnswer;

	FModularLobbyBackend Hosting;
	Hosting.CreateMatch(Nothing, LanSettings, FModularMatchOperationDelegate::CreateLambda([&bLanAnswered, &LanAnswer](const FModularOnlineResult& Result)
	{
		bLanAnswered = true;
		LanAnswer = Result;
	}));

	TestTrue(TEXT("A LAN match on lobbies is answered"), bLanAnswered);
	TestFalse(TEXT("And refused"), LanAnswer.bWasSuccessful);
	TestEqual(TEXT("Because lobbies carry no LAN match"), LanAnswer.Category, EModularOnlineErrorCategory::NotSupported);

	return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularCrossPlayPolicyTest, "ModularOnline.Match.CrossPlayPolicy", PoFigGames::Online::Tests::TestFlags)

bool FModularCrossPlayPolicyTest::RunTest(const FString& /*Parameters*/)
{
	// Publishing on both roles needs three yeses, and any one no is the same answer: this match stays on
	// the platform it was opened from. Nothing here reads state, which is why the rule is a static.
	TestTrue(TEXT("Three yeses publish on both roles"), UModularMatchSubsystem::ShouldMirrorMatch(true, true, true));
	TestFalse(TEXT("A project on one role never mirrors"), UModularMatchSubsystem::ShouldMirrorMatch(false, true, true));
	TestFalse(TEXT("A host refusing cross play is not mirrored"), UModularMatchSubsystem::ShouldMirrorMatch(true, false, true));
	TestFalse(TEXT("An account that may not cross play is not mirrored"), UModularMatchSubsystem::ShouldMirrorMatch(true, true, false));

	// A search is narrowed for want of the privilege, and only then. Asking for one platform is always
	// honoured, and a project with one publication is left with the scope it asked for because there is
	// nothing for the scope to choose between.
	TestEqual(TEXT("An allowed account searches everywhere"),
		UModularMatchSubsystem::ResolveSearchScope(EModularCrossPlayScope::Everywhere, true, true), EModularCrossPlayScope::Everywhere);

	TestEqual(TEXT("An account that may not cross play is narrowed"),
		UModularMatchSubsystem::ResolveSearchScope(EModularCrossPlayScope::Everywhere, true, false), EModularCrossPlayScope::OwnPlatformOnly);

	TestEqual(TEXT("Asking for one platform is honoured"),
		UModularMatchSubsystem::ResolveSearchScope(EModularCrossPlayScope::OwnPlatformOnly, true, true), EModularCrossPlayScope::OwnPlatformOnly);

	TestEqual(TEXT("One publication leaves the scope alone"),
		UModularMatchSubsystem::ResolveSearchScope(EModularCrossPlayScope::Everywhere, false, false), EModularCrossPlayScope::Everywhere);

	// A handle names the publication it came from, so that joining goes back to the same one. Default
	// means the role that carries matches, which is what a project on one role has.
	const FModularMatchHandle Handle;
	TestEqual(TEXT("A handle starts on no particular role"), Handle.Role, EModularOnlineRole::Default);

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularPublishedTextTest, "ModularOnline.Match.PublishedText", PoFigGames::Online::Tests::TestFlags)

bool FModularPublishedTextTest::RunTest(const FString& /*Parameters*/)
{
	using PoFigGames::Online::IModularMatchBackend;

	TestEqual(TEXT("An ordinary name is shown as it was published"),
		IModularMatchBackend::DescribePublishedText(TEXT("Anna's run")), FString(TEXT("Anna's run")));

	FString WithControls { TEXT("Room") };
	WithControls.AppendChar(TEXT('\n'));
	WithControls.AppendChar(TEXT('\t'));
	WithControls.Append(TEXT("One"));

	TestEqual(TEXT("Control characters do not reach a widget"),
		IModularMatchBackend::DescribePublishedText(WithControls), FString(TEXT("RoomOne")));

	const FString TooLong { FString::ChrN(600, TEXT('A')) };
	TestEqual(TEXT("A name is bounded before it reaches a list"), IModularMatchBackend::DescribePublishedText(TooLong).Len(), 256);

	// What the provider keeps beside a match is recognised by the configured prefix and never removed by
	// an update; with no prefix configured nothing is reserved and everything the match no longer names
	// comes off.
	const auto Backends = GetMutableDefault<UModularMatchBackendSettings>();
	const auto Configured = Backends->ReservedAttributePrefix;

	Backends->ReservedAttributePrefix = TEXT("__");
	TestTrue(TEXT("A prefixed attribute belongs to the provider"), IModularMatchBackend::IsProviderAttribute(TEXT("__memberCount")));
	TestFalse(TEXT("An ordinary attribute does not"), IModularMatchBackend::IsProviderAttribute(TEXT("Map")));

	Backends->ReservedAttributePrefix.Reset();
	TestFalse(TEXT("With no prefix configured nothing is reserved"), IModularMatchBackend::IsProviderAttribute(TEXT("__memberCount")));

	Backends->ReservedAttributePrefix = Configured;

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularCrossPlaySettingsTest, "ModularOnline.Match.CrossPlaySettings", PoFigGames::Online::Tests::TestFlags)

bool FModularCrossPlaySettingsTest::RunTest(const FString& /*Parameters*/)
{
	const auto Settings = NewObject<UModularCrossPlaySettings>();
	const auto Roles = NewObject<UModularOnlineSettings>();

	// Cross play is off until a project asks for it: a second publication costs a second backend and
	// changes where invitations come from, which is not something to inherit by default.
	TestEqual(TEXT("Cross play is off by default"), Settings->CrossPlayPolicy, EModularCrossPlayPolicy::SingleRole);

	// And when it is on, the second publication belongs to the machine the game runs on, carried in a
	// lobby because that is the shape an overlay and an invitation understand.
	TestEqual(TEXT("The companion role is the platform"), Roles->CompanionMatchRole, EModularOnlineRole::Platform);
	TestEqual(TEXT("The companion publication is a lobby"), Settings->CompanionMatchBackend, EModularMatchBackendKind::Lobbies);
	TestNotEqual(TEXT("The companion role is not the match role"), Roles->CompanionMatchRole, Roles->MatchRole);

	// Both names have to reach the schema the project declared, and the plugin knows no schema: a name it
	// brought along would publish an attribute the project never declared and have the whole match refused.
	TestTrue(TEXT("Cross play carries no name of the plugin's own"), Settings->MatchCrossPlayAttribute.IsNone());
	TestTrue(TEXT("The link between publications carries no name of the plugin's own"), Settings->MatchLinkAttribute.IsNone());

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularLoginFlowTest, "ModularOnline.User.LoginFlow", PoFigGames::Online::Tests::TestFlags)

bool FModularLoginFlowTest::RunTest(const FString& /*Parameters*/)
{
	const auto Instance = NewObject<UGameInstance>(GetTransientPackage());
	const auto Probe = NewObject<UModularLoginFlowProbe>(Instance);

	// No provider behind it: an offline build and a commandlet both reach this, and the login has to walk
	// none of its steps rather than fail at the first one.
	const FModularLoginParams Params;

	for (const auto Step : { EModularLoginStep::PlatformLogin, EModularLoginStep::TransferAuth, EModularLoginStep::ServiceLogin, EModularLoginStep::PrivilegeCheck })
	{
		TestEqual(*FString::Printf(TEXT("Step %d is skipped when nobody signs anybody in"), static_cast<int32>(Step)),
			Probe->PolicyOf(Step, Params), EModularStepPolicy::Skip);
	}

	// A guest seat is for a player who has no account, and only where the platform says guests exist.
	FGameplayTagContainer Traits;
	Traits.AddTag(ModularUserTags::Trait_AllowsGuests.GetTag());
	Probe->SetTraitTags(Traits);

	TestTrue(TEXT("A second player with no account may sit as a guest"),
		Probe->WouldBecomeGuest(1, true, EModularOnlineErrorCategory::NotLoggedIn, EModularLoginStep::PlatformLogin));

	// The refusals that are about the account itself, which a guest seat would walk around.
	TestFalse(TEXT("A refused account does not become a guest"),
		Probe->WouldBecomeGuest(1, true, EModularOnlineErrorCategory::AccessDenied, EModularLoginStep::PlatformLogin));
	TestFalse(TEXT("Neither does a failure past the platform step"),
		Probe->WouldBecomeGuest(1, true, EModularOnlineErrorCategory::NotLoggedIn, EModularLoginStep::ServiceLogin));
	TestFalse(TEXT("Nor the primary player, who is the account a guest sits beside"),
		Probe->WouldBecomeGuest(0, true, EModularOnlineErrorCategory::NotLoggedIn, EModularLoginStep::PlatformLogin));
	TestFalse(TEXT("Nor a caller who did not ask for one"),
		Probe->WouldBecomeGuest(1, false, EModularOnlineErrorCategory::NotLoggedIn, EModularLoginStep::PlatformLogin));

	// And not at all where the platform has no such thing.
	Probe->SetTraitTags(FGameplayTagContainer { });

	TestFalse(TEXT("A platform without guests has none"),
		Probe->WouldBecomeGuest(1, true, EModularOnlineErrorCategory::NotLoggedIn, EModularLoginStep::PlatformLogin));

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularUserRoleDataTest, "ModularOnline.User.RoleData", PoFigGames::Online::Tests::TestFlags)

bool FModularUserRoleDataTest::RunTest(const FString& /*Parameters*/)
{
	const auto User = NewObject<UModularUserInfo>();

	// What a project with a single provider looks like: one context, under the default role, and the
	// account data filed under it. Asking about the platform or the service is the same question, and
	// both have to answer - that is what the lookup's fallback is for.
	User->FindOrAddRoleData(EModularOnlineRole::Default).Nickname = TEXT("DefaultName");

	TestEqual(TEXT("The default role answers"), User->GetNickname(EModularOnlineRole::Default), FString(TEXT("DefaultName")));
	TestEqual(TEXT("The platform role falls back to it"), User->GetNickname(EModularOnlineRole::Platform), FString(TEXT("DefaultName")));
	TestEqual(TEXT("The service role falls back to it"), User->GetNickname(EModularOnlineRole::Service), FString(TEXT("DefaultName")));

	// And the direction that does not exist, which is why account data is filed under the role that
	// served the call rather than the one that was asked for. A screen asking "who am I" names no role,
	// so it asks for Default, and data filed only under Platform would be invisible to it.
	const auto Other = NewObject<UModularUserInfo>();
	Other->FindOrAddRoleData(EModularOnlineRole::Platform).Nickname = TEXT("PlatformName");

	TestEqual(TEXT("The platform role answers"), Other->GetNickname(EModularOnlineRole::Platform), FString(TEXT("PlatformName")));
	TestTrue(TEXT("The default role does not fall back to a named one"), Other->GetNickname(EModularOnlineRole::Default).IsEmpty());

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularUserSignedInOnlineTest, "ModularOnline.User.SignedInOnline", PoFigGames::Online::Tests::TestFlags)

bool FModularUserSignedInOnlineTest::RunTest(const FString& /*Parameters*/)
{
	const auto User = NewObject<UModularUserInfo>();

	// Nobody has tried yet.
	TestFalse(TEXT("A player who never signed in has no online account"), User->IsSignedInOnline());

	// A player signed in as far as the platform and no further. The state says LoggedInLocally, which is
	// where a project on a single provider stops, and it is exactly the case this flag exists to tell
	// apart from having no account at all.
	User->State = EModularUserState::LoggedInLocally;
	TestFalse(TEXT("Being logged in is not the same as having an account"), User->IsSignedInOnline());

	User->FindOrAddRoleData(EModularOnlineRole::Default).AccountId = UE::Online::FAccountId { UE::Online::EOnlineServices::Null, 1 };
	TestTrue(TEXT("A real account on any role counts"), User->IsSignedInOnline());

	// A guest plays on somebody else's account and has none of their own, whatever the roles hold.
	User->bIsGuest = true;
	TestFalse(TEXT("A guest has no account of their own"), User->IsSignedInOnline());

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularMergeMatchesTest, "ModularOnline.Match.MergeMatches", PoFigGames::Online::Tests::TestFlags)

bool FModularMergeMatchesTest::RunTest(const FString& /*Parameters*/)
{
	// A game instance subsystem is declared within a game instance, so one has to exist to hold it even
	// when nothing about it is initialised.
	const auto Instance = NewObject<UGameInstance>(GetTransientPackage());
	const auto Probe = NewObject<UModularMergeMatchesProbe>(Instance);
	const auto CrossPlay = GetMutableDefault<UModularCrossPlaySettings>();
	const auto Configured = CrossPlay->MatchLinkAttribute;

	CrossPlay->MatchLinkAttribute = TEXT("LinkedMatchId");

	FModularMatchInfo OnTheMatchRole;
	OnTheMatchRole.Handle.Id = TEXT("match-1");

	FModularMatchInfo TheSameGameOnThePlatform;
	TheSameGameOnThePlatform.Handle.Id = TEXT("platform-1");
	TheSameGameOnThePlatform.Attributes.Emplace(TEXT("LinkedMatchId"), TEXT("match-1"));

	FModularMatchInfo SomebodyElse;
	SomebodyElse.Handle.Id = TEXT("platform-2");

	TArray<FModularMatchInfo> Found { OnTheMatchRole };
	Probe->Merge(Found, { TheSameGameOnThePlatform, SomebodyElse }, 20);

	// One game published twice is one line in a browser; the link attribute is the only thing that says so.
	TestEqual(TEXT("The second publication of the same game is not listed again"), Found.Num(), 2);
	TestEqual(TEXT("And the one that is added is the other host's"), Found[1].Handle.Id, FString(TEXT("platform-2")));

	// Without the link nothing connects the two answers, and both are listed.
	CrossPlay->MatchLinkAttribute = FName { };

	TArray<FModularMatchInfo> Unlinked { OnTheMatchRole };
	Probe->Merge(Unlinked, { TheSameGameOnThePlatform }, 20);

	TestEqual(TEXT("With no link configured a game published twice is listed twice"), Unlinked.Num(), 2);

	// What a search was asked for is what it answers with, however many roles it looked on.
	TArray<FModularMatchInfo> Capped { OnTheMatchRole };
	Probe->Merge(Capped, { TheSameGameOnThePlatform, SomebodyElse }, 1);

	TestEqual(TEXT("A merge does not exceed the number of results asked for"), Capped.Num(), 1);

	CrossPlay->MatchLinkAttribute = Configured;

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularPresenceWithoutServicesTest, "ModularOnline.Features.PresenceWithoutServices", PoFigGames::Online::Tests::TestFlags)

bool FModularPresenceWithoutServicesTest::RunTest(const FString& /*Parameters*/)
{
	// A subsystem with no game instance behind it has no services either, which is the state a commandlet
	// and an offline build are in. Publishing has to answer false rather than reach through a null.
	const auto Instance = NewObject<UGameInstance>(GetTransientPackage());
	const auto Presence = NewObject<UModularPresenceSubsystem>(Instance);
	const auto State = FGameplayTag::RequestGameplayTag(TEXT("Online.Feature.Presence"));

	TestFalse(TEXT("A described state cannot be published without services"), Presence->PublishState(0, State, TMap<FString, FString> { }));
	TestFalse(TEXT("Neither can an undescribed one"), Presence->PublishState(0, FGameplayTag { }, TMap<FString, FString> { }));

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularPresenceStatesTest, "ModularOnline.Features.PresenceStates", PoFigGames::Online::Tests::TestFlags)

bool FModularPresenceStatesTest::RunTest(const FString& /*Parameters*/)
{
	const auto Settings = NewObject<UModularPresenceSettings>();

	// Emptied on purpose: a new object copies whatever this project configured, and what is under test is
	// the mechanism rather than the states this particular game happens to describe.
	Settings->States.Reset();

	// A state nobody described cannot be published, and saying so is the point: the alternative is a
	// friends list quietly showing the wrong thing because of a typo in an ini.
	TestNull(TEXT("An undescribed state is not found"), Settings->FindState(FGameplayTag::RequestGameplayTag(TEXT("Online.Feature.Presence"))));

	FModularPresenceState InMatch;
	InMatch.State = FGameplayTag::RequestGameplayTag(TEXT("Online.Feature.Presence"));
	InMatch.Status = TEXT("Status_InGame");
	InMatch.Properties.Emplace(TEXT("map_name"), TEXT("{map}"));
	InMatch.Properties.Emplace(TEXT("zone"), TEXT("{zone}"));
	InMatch.Properties.Emplace(TEXT("kind"), TEXT("coop"));

	Settings->States.Add(InMatch);

	const auto Found = Settings->FindState(FGameplayTag::RequestGameplayTag(TEXT("Online.Feature.Presence")));
	TestNotNull(TEXT("A described state is found by its tag"), Found);

	if (Found)
	{
		TestEqual(TEXT("It carries the status the provider renders"), Found->Status, FString(TEXT("Status_InGame")));

		// Two kinds of value live side by side: one written plainly, and two naming what the publisher
		// supplies. Which is which is decided by the braces, in configuration, without any C++.
		TestEqual(TEXT("A plain value is kept as written"), Found->Properties.FindRef(TEXT("kind")), FString(TEXT("coop")));
		TestEqual(TEXT("A placeholder names a supplied value"), Found->Properties.FindRef(TEXT("map_name")), FString(TEXT("{map}")));
	}

	// Another state added beside it is exactly as much work, which is the whole point of the array.
	FModularPresenceState Menu;
	Menu.State = FGameplayTag::RequestGameplayTag(TEXT("Online.Feature.Social"));
	Menu.Status = TEXT("Status_MainMenu");

	Settings->States.Add(Menu);

	TestNotNull(TEXT("A second state is found too"), Settings->FindState(FGameplayTag::RequestGameplayTag(TEXT("Online.Feature.Social"))));
	TestNull(TEXT("And one still undescribed is not"), Settings->FindState(ModularOnlineTags::Feature_Auth.GetTag()));

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModularOnlineStringTableTest, "ModularOnline.Core.StringTable", PoFigGames::Online::Tests::TestFlags)

bool FModularOnlineStringTableTest::RunTest(const FString& /*Parameters*/)
{
	// Asked the way the plugin asks, before anything has loaded the asset: by its id, which loads it on first use.
	const auto Refusal = FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Lobbies);
	TestEqual(TEXT("A plugin sentence is read from the asset"), Refusal.ErrorText.ToString(),
		FString::Printf(TEXT("%s is not available on this platform."), *ModularOnlineTags::Feature_Lobbies.GetTag().ToString()));

	const auto Asset = LoadObject<UStringTable>(nullptr, TEXT("/ModularOnline/StringTables/ModularOnline.ModularOnline"));
	const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("ModularOnline"));

	TestNotNull(TEXT("The plugin's string table asset loads"), Asset);
	TestTrue(TEXT("The plugin is found"), Plugin.IsValid());

	if (Asset && Plugin.IsValid())
	{
		const auto Table = Asset->GetStringTable();

		FTextLocalizationResource Russian;
		const auto RussianPath = Plugin->GetContentDir() / TEXT("Localization/ModularOnline/ru/ModularOnline.locres");

		TestTrue(TEXT("The Russian translation loads"), Russian.LoadFromFile(RussianPath, 0));

		const FTextKey Namespace { Table->GetNamespace() };
		auto EntryCount = 0;

		// The hash ties a translation to the English it was made from: after the English changes, the engine shows
		// English again, and this is where that shows up instead of in a build.
		Table->EnumerateKeysAndSourceStrings([this, &Russian, &Namespace, &EntryCount](const FTextKey& Key, const FString& Source)
		{
			const auto Name = Key.ToString();
			const auto Translated = Russian.Entries.Find(FTextId(Namespace, Key));
			const auto bTranslated = Translated && Translated->LocalizedString.IsValid() && !Translated->LocalizedString->IsEmpty();

			TestFalse(*FString::Printf(TEXT("'%s' has English text"), *Name), Source.IsEmpty());
			TestTrue(*FString::Printf(TEXT("'%s' has a Russian translation"), *Name), bTranslated);
			TestTrue(*FString::Printf(TEXT("'%s' is translated from its current English text"), *Name),
				Translated && Translated->SourceStringHash == FTextLocalizationResource::HashString(Source));

			++EntryCount;

			return true;
		});

		TestTrue(TEXT("The table is not empty"), EntryCount > 0);

		// The file alone proves nothing about the plugin's localisation target, which is what puts it in front of the text.
		const auto Expected = Russian.Entries.Find(FTextId(Namespace, FTextKey(TEXT("MatchHasNoRoom"))));

		FInternationalization::FCultureStateSnapshot Previous;
		FInternationalization::Get().BackupCultureState(Previous);
		FInternationalization::Get().SetCurrentCulture(TEXT("ru"));

		const auto Shown = FText::FromStringTable(Asset->GetStringTableId(), TEXT("MatchHasNoRoom"));
		TestTrue(TEXT("A sentence of the table shows in Russian under the Russian culture"),
			Expected && Expected->LocalizedString.IsValid() && Shown.ToString() == *Expected->LocalizedString);

		FInternationalization::Get().RestoreCultureState(Previous);
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
