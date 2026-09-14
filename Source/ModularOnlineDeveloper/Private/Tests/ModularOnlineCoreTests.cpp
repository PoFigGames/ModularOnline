// Copyright PoFig Games Studio. All Rights Reserved.

#include "Containers/Ticker.h"
#include "Misc/AutomationTest.h"

#include "Match/ModularLobbyBackend.h"
#include "Match/ModularMatchSubsystem.h"
#include "Match/ModularMatchTypes.h"
#include "Match/ModularSessionBackend.h"
#include "Core/ModularOnlineContext.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineTags.h"
#include "Core/ModularOnlineTypes.h"
#include "User/ModularUserInfo.h"
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
	TestEqual(TEXT("NotImplemented is NotSupported"), FModularOnlineResult::FromOnlineError(Errors::NotImplemented()).Category, EModularOnlineErrorCategory::NotSupported);
	TestEqual(TEXT("MissingInterface is NotSupported"), FModularOnlineResult::FromOnlineError(Errors::MissingInterface()).Category, EModularOnlineErrorCategory::NotSupported);
	TestEqual(TEXT("NotLoggedIn is NotLoggedIn"), FModularOnlineResult::FromOnlineError(Errors::NotLoggedIn()).Category, EModularOnlineErrorCategory::NotLoggedIn);
	TestEqual(TEXT("NoConnection is NoConnection"), FModularOnlineResult::FromOnlineError(Errors::NoConnection()).Category, EModularOnlineErrorCategory::NoConnection);
	TestEqual(TEXT("AccessDenied is AccessDenied"), FModularOnlineResult::FromOnlineError(Errors::AccessDenied()).Category, EModularOnlineErrorCategory::AccessDenied);
	TestEqual(TEXT("Cancelled is Cancelled"), FModularOnlineResult::FromOnlineError(Errors::Cancelled()).Category, EModularOnlineErrorCategory::Cancelled);
	TestEqual(TEXT("Timeout is TimedOut"), FModularOnlineResult::FromOnlineError(Errors::Timeout()).Category, EModularOnlineErrorCategory::TimedOut);
	TestEqual(TEXT("An unmapped error stays Unknown"), FModularOnlineResult::FromOnlineError(Errors::RequestFailure()).Category, EModularOnlineErrorCategory::Unknown);

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

	// A machine without the provider's SDK logs this at error level, which the automation framework counts
	// as a failure. A negative count means "ignore if present"; zero would make them required.
	AddExpectedMessagePlain(TEXT("InitEx failed"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, -1);
	AddExpectedMessagePlain(TEXT("Failed to initialize a "), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, -1);

	// Taken without a world on purpose: this runs outside a game instance, and the shared instance is the
	// only one there is here. What is under test is the discovery, not which world it was asked for.
	const auto Services = UE::Online::GetServices(UE::Online::EOnlineServices::Default);

	const FModularOnlineContext Context { EModularOnlineRole::Default, Services };

	if (!Services.IsValid())
	{
		// A build with no online services is a state this plugin has to survive, not a failed test.
		TestFalse(TEXT("A context without services is not valid"), Context.IsValid());
		TestEqual(TEXT("A context without services implements nothing"), Context.GetFeatures().Num(), 0);
		TestEqual(TEXT("A context without services has no provider"), Context.GetProvider(), UE::Online::EOnlineServices::None);

		return true;
	}

	TestTrue(TEXT("A context over real services is valid"), Context.IsValid());
	TestNotEqual(TEXT("A context over real services names its provider"), Context.GetProvider(), UE::Online::EOnlineServices::None);

	// Every provider worth configuring signs users in; if even that is missing, discovery is broken
	// rather than the provider being modest.
	TestTrue(TEXT("A configured provider implements authentication"), Context.HasFeature(ModularOnlineTags::Feature_Auth));

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
	TestEqual(TEXT("A named provider gets its own attribute"), Accounts->GetAvatarAttribute(TEXT("Steam")), FString(TEXT("avatar_url_full")));
	TestEqual(TEXT("An unnamed provider falls back to the default"), Accounts->GetAvatarAttribute(TEXT("Epic")), FString(TEXT("AvatarUrl")));

	// The store is addressed separately from matches on purpose: the backend that runs a project's
	// sessions is rarely the storefront that took the player's money. A project where they are the same
	// says so in the ini rather than discovering an empty catalogue at runtime.
	TestEqual(TEXT("Matches default to the service role"), Roles->MatchRole, EModularOnlineRole::Service);
	TestEqual(TEXT("The store defaults to the platform role"), Roles->StoreRole, EModularOnlineRole::Platform);

	// Emptying the default is how a project says that avatars are not read at all.
	Accounts->DefaultAvatarAttribute.Reset();
	TestTrue(TEXT("An empty default means no avatar is read"), Accounts->GetAvatarAttribute(TEXT("Epic")).IsEmpty());
	TestEqual(TEXT("An empty default does not affect a named provider"), Accounts->GetAvatarAttribute(TEXT("Steam")), FString(TEXT("avatar_url_full")));

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

	// The mode a match is advertised under falls back to the name of the project rather than to nothing.

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

	FModularLobbyBackend { }.FindMatches(Nothing, FModularMatchSearchParams { }, OnSearch);

	TestTrue(TEXT("A search without services answers"), bSearchAnswered);
	TestFalse(TEXT("With a refusal"), SearchAnswer.bWasSuccessful);
	TestEqual(TEXT("And an empty list, not an absent one"), FoundCount, 0);

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

	// Both names have to reach the schema the project declared, so neither may be empty.
	TestFalse(TEXT("Cross play is published under a name"), Settings->MatchCrossPlayAttribute.IsNone());
	TestFalse(TEXT("The two publications are linked by a name"), Settings->MatchLinkAttribute.IsNone());

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
	InMatch.Properties.Emplace(TEXT("game_mode"), TEXT("{mode}"));
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

#endif // WITH_AUTOMATION_TESTS
