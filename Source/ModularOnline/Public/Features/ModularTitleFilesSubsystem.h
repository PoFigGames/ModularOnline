// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"

#include "ModularTitleFilesSubsystem.generated.h"


/** How a request for the list of the title's files ends. */
DECLARE_DELEGATE_TwoParams(FModularTitleFilesDelegate, const TArray<FString>& /*Filenames*/, const FModularOnlineResult& /*Result*/);

/** How a read of one of them ends. */
DECLARE_DELEGATE_TwoParams(FModularTitleFileReadDelegate, const TArray<uint8>& /*Contents*/, const FModularOnlineResult& /*Result*/);

/** The list of files the title publishes was answered. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularTitleFilesEnumeratedEvent, const TArray<FString>& /*Filenames*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularTitleFilesEnumeratedDynamic, const TArray<FString>&, Filenames, const FModularOnlineResult&, Result);

/** One of them was read, whether or not it arrived. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FModularTitleFileReadEvent, const FString& /*Filename*/, const TArray<uint8>& /*Contents*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FModularTitleFileReadDynamic, const FString&, Filename, const TArray<uint8>&, Contents, const FModularOnlineResult&, Result);


/**
 * @class UModularTitleFilesSubsystem
 *
 * @brief The files the title publishes to every client, which is how a hotfix reaches a shipped build.
 */
UCLASS(MinimalAPI)
class UModularTitleFilesSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when the list of published files is answered. */
	FModularTitleFilesEnumeratedEvent OnFilesEnumerated { };

	/** Fired when a read finishes, with the bytes when it succeeded and none when it did not. */
	FModularTitleFileReadEvent OnFileRead { };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Asks the services which files the title publishes. */
	MODULARONLINE_API virtual bool EnumerateFiles(int32 LocalPlayerIndex, FModularTitleFilesDelegate OnComplete = FModularTitleFilesDelegate { });

	/** What the last enumeration answered, without asking again. */
	MODULARONLINE_API bool GetEnumeratedFiles(int32 LocalPlayerIndex, TArray<FString>& OutFilenames) const;

	/** Reads one of them whole. */
	MODULARONLINE_API virtual bool ReadFile(int32 LocalPlayerIndex, const FString& Filename, FModularTitleFileReadDelegate OnComplete = FModularTitleFileReadDelegate { });

protected:
	/** The same events, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|TitleFiles", meta = (DisplayName = "On Title Files Enumerated"))
	FModularTitleFilesEnumeratedDynamic K2_OnFilesEnumerated { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|TitleFiles", meta = (DisplayName = "On Title File Read"))
	FModularTitleFileReadDynamic K2_OnFileRead { };

	/** Tells everyone listening how a listing ended, and answers the caller who asked for it. */
	MODULARONLINE_API void AnnounceEnumerated(const TArray<FString>& Filenames, const FModularOnlineResult& Result, const FModularTitleFilesDelegate& OnComplete);

	/** The same for a read. */
	MODULARONLINE_API void AnnounceRead(const FString& Filename, const TArray<uint8>& Contents, const FModularOnlineResult& Result, const FModularTitleFileReadDelegate& OnComplete);

	/** What Blueprint may ask of this subsystem. The answers arrive on the events above. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|TitleFiles", meta = (DisplayName = "Enumerate Title Files"))
	MODULARONLINE_API bool K2_EnumerateFiles(int32 LocalPlayerIndex = 0) { return EnumerateFiles(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|TitleFiles", meta = (DisplayName = "Get Enumerated Title Files"))
	MODULARONLINE_API bool K2_GetEnumeratedFiles(TArray<FString>& OutFilenames, int32 LocalPlayerIndex = 0) const { return GetEnumeratedFiles(LocalPlayerIndex, OutFilenames); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|TitleFiles", meta = (DisplayName = "Read Title File"))
	MODULARONLINE_API bool K2_ReadFile(const FString& Filename, int32 LocalPlayerIndex = 0) { return ReadFile(LocalPlayerIndex, Filename); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|TitleFiles", meta = (DisplayName = "Are Title Files Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
