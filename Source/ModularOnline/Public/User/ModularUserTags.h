// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"


/** Traits of the platform the game is running on, as the online layer needs to know them. */
namespace ModularUserTags
{
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_SingleOnlineUser);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_RequiresStartInput);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_SupportsUserSwitch);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_AllowsGuests);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_RequiresPrivilegeCheck);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_CrossPlayOptional);
}
