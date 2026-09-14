// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineTags.h"

namespace ModularOnlineTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Auth, "Online.Feature.Auth", "Signing a local user in and out, and the account they are signed in with.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Privileges, "Online.Feature.Privileges", "What the account is allowed to do: play, play online, chat, use user generated content, cross play.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Connectivity, "Online.Feature.Connectivity", "Whether the backend is reachable right now. A provider without it is treated as always connected.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_UserInfo, "Online.Feature.UserInfo", "Names and avatars of accounts other than the local one.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_ExternalUI, "Online.Feature.ExternalUI", "The system overlay of the platform: its login screen, its friends list.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Lobbies, "Online.Feature.Lobbies", "Player hosted groups the match can live in, which is how Steam and Epic Game Services carry a match.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Sessions, "Online.Feature.Sessions", "Registered sessions, which is what a dedicated server publishes and what console services search.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Presence, "Online.Feature.Presence", "What the player is shown to be doing, to their friends and on their profile.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Social, "Online.Feature.Social", "Friends, block lists and friend requests.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Achievements, "Online.Feature.Achievements", "Achievement definitions, their state and unlocking them.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Stats, "Online.Feature.Stats", "Per account statistics kept by the backend.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Leaderboards, "Online.Feature.Leaderboards", "Ranked tables, around a player or across their friends.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_TitleFile, "Online.Feature.TitleFile", "Files the title publishes to every client, which is how hotfix data arrives.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_UserFile, "Online.Feature.UserFile", "Files belonging to one account, which is where cloud saves live.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Commerce, "Online.Feature.Commerce", "The store of the platform: entitlements, offers and purchases.");
}
