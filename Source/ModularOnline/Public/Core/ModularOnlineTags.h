// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"


/** One tag per Online Services component, published for whatever the current provider implements. */
namespace ModularOnlineTags
{
	// ============================================================================
	// Components of the online services
	// ============================================================================

	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Auth);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Privileges);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Connectivity);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_UserInfo);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_ExternalUI);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Lobbies);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Sessions);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Presence);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Social);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Achievements);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Stats);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Leaderboards);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_TitleFile);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_UserFile);
	MODULARONLINE_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Feature_Commerce);
}
