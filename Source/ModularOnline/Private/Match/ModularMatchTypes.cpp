// Copyright PoFig Games Studio. All Rights Reserved.

#include "Match/ModularMatchTypes.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularMatchTypes)

#define LOCTEXT_NAMESPACE "ModularOnline"

bool FModularMatchHandle::IsValid() const
{
	return LobbyId.IsValid() || SessionId.IsValid();
}

FString FModularMatchSettings::GetMapName() const
{
	if (FAssetData MapAssetData; UAssetManager::Get().GetPrimaryAssetData(MapId, MapAssetData))
	{
		return MapAssetData.PackageName.ToString();
	}

	// An id can also carry the package path as its name, which is what the editor's asset picker wrote for
	// a world the asset manager has not registered under that id when it was checked on 2026-09-15. The
	// path travels on its own.
	if (const auto MapAssetName = MapId.PrimaryAssetName.ToString(); FPackageName::IsValidLongPackageName(MapAssetName))
	{
		return MapAssetName;
	}

	return FString { };
}

FString FModularMatchSettings::ConstructTravelURL() const
{
	const auto MapName = GetMapName();

	// A URL with options and no map is a relative one, and the engine fills the map in from wherever the
	// game already is - which would travel it to itself instead of saying it cannot go.
	if (MapName.IsEmpty())
	{
		return FString { };
	}

	TStringBuilder<256> Builder;
	Builder.Append(MapName);

	if (OnlineMode == EModularMatchOnlineMode::LAN)
	{
		Builder.Append(TEXT("?bIsLanMatch"));
	}

	if (OnlineMode != EModularMatchOnlineMode::Offline)
	{
		Builder.Append(TEXT("?listen"));
	}

	for (const auto& Argument : ExtraArgs)
	{
		if (Argument.Key.IsEmpty())
		{
			// An argument with no name is not one, and a bare "?" in a travel URL is a parse error.
		}
		else if (Argument.Value.IsEmpty())
		{
			Builder.Appendf(TEXT("?%s"), *Argument.Key);
		}
		else
		{
			Builder.Appendf(TEXT("?%s=%s"), *Argument.Key, *Argument.Value);
		}
	}

	return Builder.ToString();
}

bool FModularMatchSettings::Validate(FText& OutError) const
{
	if (GetMapName().IsEmpty())
	{
		OutError = FText::Format(LOCTEXT("MatchHasNoMap", "There is no map registered as {0}, so this match cannot be hosted."), FText::FromString(MapId.ToString()));

		return false;
	}

	if (MaxPlayers < 1)
	{
		OutError = LOCTEXT("MatchHasNoRoom", "A match has to have room for at least one player.");

		return false;
	}

#if !WITH_SERVER_CODE
	// A client build carries no server code at all, so there is nothing in it to host with. Games that
	// need a client to host something - a tutorial, a practice range - ship those as server capable.
	OutError = LOCTEXT("ClientCannotHost", "This build cannot host a match.");

	return false;
#else
	return true;
#endif
}

#undef LOCTEXT_NAMESPACE
