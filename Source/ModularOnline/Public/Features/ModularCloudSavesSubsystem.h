// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"

#include "ModularCloudSavesSubsystem.generated.h"

namespace UE::Online
{
	class IUserFile;
}


/** How a request for the list of an account's files ends. */
DECLARE_DELEGATE_TwoParams(FModularCloudFilesDelegate, const TArray<FString>& /*Filenames*/, const FModularOnlineResult& /*Result*/);

/** How a read of one of them ends. */
DECLARE_DELEGATE_TwoParams(FModularCloudFileReadDelegate, const TArray<uint8>& /*Contents*/, const FModularOnlineResult& /*Result*/);

/** How a write, a copy or a delete ends. */
DECLARE_DELEGATE_OneParam(FModularCloudFileOperationDelegate, const FModularOnlineResult& /*Result*/);

/** The list of files the account has was answered. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularCloudFilesEnumeratedEvent, const TArray<FString>& /*Filenames*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularCloudFilesEnumeratedDynamic, const TArray<FString>&, Filenames, const FModularOnlineResult&, Result);

/** A file was read, whether or not it arrived. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FModularCloudFileReadEvent, const FString& /*Filename*/, const TArray<uint8>& /*Contents*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FModularCloudFileReadDynamic, const FString&, Filename, const TArray<uint8>&, Contents, const FModularOnlineResult&, Result);

/** A file was written, copied or deleted. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularCloudFileOperationEvent, const FString& /*Filename*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularCloudFileOperationDynamic, const FString&, Filename, const FModularOnlineResult&, Result);


/**
 * @class UModularCloudSavesSubsystem
 *
 * @brief The files that belong to one account, which is where a save that follows the player lives.
 */
UCLASS(MinimalAPI)
class UModularCloudSavesSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when the list of the account's files is answered. */
	FModularCloudFilesEnumeratedEvent OnFilesEnumerated { };

	/** Fired when a read finishes, with the bytes when it succeeded and none when it did not. */
	FModularCloudFileReadEvent OnFileRead { };

	/** Fired when a write, a copy or a delete finishes. */
	FModularCloudFileOperationEvent OnFileOperationCompleted { };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Asks the services which files this account has. */
	MODULARONLINE_API virtual bool EnumerateFiles(int32 LocalPlayerIndex, FModularCloudFilesDelegate OnComplete = FModularCloudFilesDelegate { });

	/** What the last enumeration answered, without asking again. */
	MODULARONLINE_API bool GetEnumeratedFiles(int32 LocalPlayerIndex, TArray<FString>& OutFilenames) const;

	/** Reads one file whole. */
	MODULARONLINE_API virtual bool ReadFile(int32 LocalPlayerIndex, const FString& Filename, FModularCloudFileReadDelegate OnComplete = FModularCloudFileReadDelegate { });

	/** Writes one file whole, replacing whatever was under that name. */
	MODULARONLINE_API virtual bool WriteFile(int32 LocalPlayerIndex, const FString& Filename, const TArray<uint8>& Contents, FModularCloudFileOperationDelegate OnComplete = FModularCloudFileOperationDelegate { });

	/** Copies a file to another name, which is how a backup before an overwrite is made. */
	MODULARONLINE_API virtual bool CopyFile(int32 LocalPlayerIndex, const FString& SourceFilename, const FString& TargetFilename, FModularCloudFileOperationDelegate OnComplete = FModularCloudFileOperationDelegate { });

	/** Deletes a file. */
	MODULARONLINE_API virtual bool DeleteFile(int32 LocalPlayerIndex, const FString& Filename, FModularCloudFileOperationDelegate OnComplete = FModularCloudFileOperationDelegate { });

protected:
	/** The same events, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "On Files Enumerated"))
	FModularCloudFilesEnumeratedDynamic K2_OnFilesEnumerated { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "On File Read"))
	FModularCloudFileReadDynamic K2_OnFileRead { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "On File Operation Completed"))
	FModularCloudFileOperationDynamic K2_OnFileOperationCompleted { };

	/** Tells everyone listening how a listing ended, and answers the caller who asked for it. */
	MODULARONLINE_API void AnnounceEnumerated(const TArray<FString>& Filenames, const FModularOnlineResult& Result, const FModularCloudFilesDelegate& OnComplete);

	/** The same for a read. */
	MODULARONLINE_API void AnnounceRead(const FString& Filename, const TArray<uint8>& Contents, const FModularOnlineResult& Result, const FModularCloudFileReadDelegate& OnComplete);

	/** And for a write, a copy or a delete. */
	MODULARONLINE_API void AnnounceOperation(const FString& Filename, const FModularOnlineResult& Result, const FModularCloudFileOperationDelegate& OnComplete);

	/** Whether a write, a copy or a delete can happen, answering the caller itself when it cannot. */
	MODULARONLINE_API bool CanOperate(const TSharedPtr<UE::Online::IUserFile>& UserFile, const UE::Online::FAccountId& Account,
		const FString& Filename, const FModularCloudFileOperationDelegate& OnComplete);

	/** What Blueprint may ask of this subsystem. The answers arrive on the events above. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Enumerate Cloud Files"))
	MODULARONLINE_API bool K2_EnumerateFiles(int32 LocalPlayerIndex = 0) { return EnumerateFiles(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Get Enumerated Cloud Files"))
	MODULARONLINE_API bool K2_GetEnumeratedFiles(TArray<FString>& OutFilenames, int32 LocalPlayerIndex = 0) const { return GetEnumeratedFiles(LocalPlayerIndex, OutFilenames); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Read Cloud File"))
	MODULARONLINE_API bool K2_ReadFile(const FString& Filename, int32 LocalPlayerIndex = 0) { return ReadFile(LocalPlayerIndex, Filename); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Write Cloud File", AutoCreateRefTerm = "Contents"))
	MODULARONLINE_API bool K2_WriteFile(const FString& Filename, const TArray<uint8>& Contents, int32 LocalPlayerIndex = 0) { return WriteFile(LocalPlayerIndex, Filename, Contents); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Copy Cloud File"))
	MODULARONLINE_API bool K2_CopyFile(const FString& SourceFilename, const FString& TargetFilename, int32 LocalPlayerIndex = 0) { return CopyFile(LocalPlayerIndex, SourceFilename, TargetFilename); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Delete Cloud File"))
	MODULARONLINE_API bool K2_DeleteFile(const FString& Filename, int32 LocalPlayerIndex = 0) { return DeleteFile(LocalPlayerIndex, Filename); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|CloudSaves", meta = (DisplayName = "Are Cloud Saves Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
