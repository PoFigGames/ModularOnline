// Copyright PoFig Games Studio. All Rights Reserved.

#include "User/ModularUserTypes.h"

#include "Core/ModularOnlineStringTable.h"
#include "Online/Privileges.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUserTypes)

UE::Online::EUserPrivileges FModularPrivilegeConversions::ToOnlineServices(const EModularOnlinePrivilege Privilege)
{
	using namespace UE::Online;

	switch (Privilege)
	{
	case EModularOnlinePrivilege::CanPlay:
		return EUserPrivileges::CanPlay;

	case EModularOnlinePrivilege::CanPlayOnline:
		return EUserPrivileges::CanPlayOnline;

	case EModularOnlinePrivilege::CanCommunicateViaText:
		return EUserPrivileges::CanCommunicateViaTextOnline;

	case EModularOnlinePrivilege::CanCommunicateViaVoice:
		return EUserPrivileges::CanCommunicateViaVoiceOnline;

	case EModularOnlinePrivilege::CanUseUserGeneratedContent:
		return EUserPrivileges::CanUseUserGeneratedContent;

	case EModularOnlinePrivilege::CanUseCrossPlay:
		return EUserPrivileges::CanCrossPlay;

	case EModularOnlinePrivilege::Count:
		break;
	}

	// Asking about a privilege that is not one is a programming error; the safest answer is the narrowest
	// question the services know.
	return EUserPrivileges::CanPlay;
}

EModularOnlinePrivilege FModularPrivilegeConversions::FromOnlineServices(const UE::Online::EUserPrivileges Privilege)
{
	using namespace UE::Online;

	switch (Privilege)
	{
	case EUserPrivileges::CanPlay:
		return EModularOnlinePrivilege::CanPlay;

	case EUserPrivileges::CanPlayOnline:
		return EModularOnlinePrivilege::CanPlayOnline;

	case EUserPrivileges::CanCommunicateViaTextOnline:
		return EModularOnlinePrivilege::CanCommunicateViaText;

	case EUserPrivileges::CanCommunicateViaVoiceOnline:
		return EModularOnlinePrivilege::CanCommunicateViaVoice;

	case EUserPrivileges::CanUseUserGeneratedContent:
		return EModularOnlinePrivilege::CanUseUserGeneratedContent;

	case EUserPrivileges::CanCrossPlay:
		return EModularOnlinePrivilege::CanUseCrossPlay;
	}

	return EModularOnlinePrivilege::Count;
}

EModularOnlinePrivilegeResult FModularPrivilegeConversions::FromOnlineServices(const UE::Online::EUserPrivileges Privilege, const UE::Online::EPrivilegeResults Results)
{
	using namespace UE::Online;

	// The services answer with a bitfield, because a platform can refuse for several reasons at once.
	// The player is told one thing, so the reasons are ordered by what they can do about them: sign in,
	// update, wait for the network, take it up with the platform.
	if (Results == EPrivilegeResults::NoFailures)
	{
		return EModularOnlinePrivilegeResult::Available;
	}

	if (EnumHasAnyFlags(Results, EPrivilegeResults::UserNotFound | EPrivilegeResults::UserNotLoggedIn))
	{
		return EModularOnlinePrivilegeResult::NotLoggedIn;
	}

	if (EnumHasAnyFlags(Results, EPrivilegeResults::RequiredPatchAvailable | EPrivilegeResults::RequiredSystemUpdate))
	{
		return EModularOnlinePrivilegeResult::VersionOutdated;
	}

	if (EnumHasAnyFlags(Results, EPrivilegeResults::NetworkConnectionUnavailable))
	{
		return EModularOnlinePrivilegeResult::NetworkUnavailable;
	}

	if (EnumHasAnyFlags(Results, EPrivilegeResults::AgeRestrictionFailure))
	{
		return EModularOnlinePrivilegeResult::AgeRestricted;
	}

	if (EnumHasAnyFlags(Results, EPrivilegeResults::AccountTypeFailure))
	{
		return EModularOnlinePrivilegeResult::AccountTypeRestricted;
	}

	constexpr auto AccountUseFailures = EPrivilegeResults::OnlinePlayRestricted | EPrivilegeResults::UGCRestriction | EPrivilegeResults::ChatRestriction;
	if (EnumHasAnyFlags(Results, AccountUseFailures))
	{
		return EModularOnlinePrivilegeResult::AccountUseRestricted;
	}

	// Being unable to play at all, with no reason given, is what an unowned game looks like on most
	// platforms.
	if (Privilege == EUserPrivileges::CanPlay)
	{
		return EModularOnlinePrivilegeResult::LicenseInvalid;
	}

	return EModularOnlinePrivilegeResult::PlatformFailure;
}

namespace PoFigGames::Online
{
	FText DescribePrivilegeRefusal(const EModularOnlinePrivilegeResult Result)
	{
		switch (Result)
		{
		case EModularOnlinePrivilegeResult::Available:
		case EModularOnlinePrivilegeResult::Unknown:
			return FText { };

		case EModularOnlinePrivilegeResult::NotLoggedIn:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.NotLoggedIn"));

		case EModularOnlinePrivilegeResult::LicenseInvalid:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.LicenseInvalid"));

		case EModularOnlinePrivilegeResult::VersionOutdated:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.VersionOutdated"));

		case EModularOnlinePrivilegeResult::NetworkUnavailable:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.NetworkUnavailable"));

		case EModularOnlinePrivilegeResult::AgeRestricted:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.AgeRestricted"));

		case EModularOnlinePrivilegeResult::AccountTypeRestricted:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.AccountTypeRestricted"));

		case EModularOnlinePrivilegeResult::AccountUseRestricted:
			return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.AccountUseRestricted"));

		case EModularOnlinePrivilegeResult::PlatformFailure:
			break;
		}

		return FText::FromStringTable(PoFigGames::Online::StringTableId, TEXT("Privilege.PlatformFailure"));
	}
}
