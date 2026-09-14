// Copyright PoFig Games Studio. All Rights Reserved.

#include "User/ModularUserTags.h"

namespace ModularUserTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_SingleOnlineUser, "Platform.Trait.SingleOnlineUser", "The platform has one online user and every local player shares it.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_RequiresStartInput, "Platform.Trait.RequiresStartInput", "A player has to press a button before the game may pick a user for them, which is a certification requirement on consoles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_SupportsUserSwitch, "Platform.Trait.SupportsUserSwitch", "The platform can change the account behind a local player while the game runs.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_AllowsGuests, "Platform.Trait.AllowsGuests", "A local player may play without an account of their own.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_RequiresPrivilegeCheck, "Platform.Trait.RequiresPrivilegeCheck", "Privileges have to be asked for before any online action, whatever the game would otherwise do.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_CrossPlayOptional, "Platform.Trait.CrossPlayOptional", "The player can turn cross play off, so a match may have to be restricted to their own platform.");
}
