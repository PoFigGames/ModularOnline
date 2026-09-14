// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularCloudSavesSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/UserFile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularCloudSavesSubsystem)

FGameplayTag UModularCloudSavesSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_UserFile;
}

void UModularCloudSavesSubsystem::AnnounceEnumerated(const TArray<FString>& Filenames, const FModularOnlineResult& Result, const FModularCloudFilesDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Filenames, Result);
	OnFilesEnumerated.Broadcast(Filenames, Result);
	K2_OnFilesEnumerated.Broadcast(Filenames, Result);
}

void UModularCloudSavesSubsystem::AnnounceRead(const FString& Filename, const TArray<uint8>& Contents, const FModularOnlineResult& Result, const FModularCloudFileReadDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Contents, Result);
	OnFileRead.Broadcast(Filename, Contents, Result);
	K2_OnFileRead.Broadcast(Filename, Contents, Result);
}

void UModularCloudSavesSubsystem::AnnounceOperation(const FString& Filename, const FModularOnlineResult& Result, const FModularCloudFileOperationDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Result);
	OnFileOperationCompleted.Broadcast(Filename, Result);
	K2_OnFileOperationCompleted.Broadcast(Filename, Result);
}

bool UModularCloudSavesSubsystem::CanOperate(const TSharedPtr<UE::Online::IUserFile>& UserFile, const UE::Online::FAccountId& Account,
	const FString& Filename, const FModularCloudFileOperationDelegate& OnComplete)
{
	if (!UserFile.IsValid())
	{
		AnnounceOperation(Filename, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceOperation(Filename, NotSignedIn(), OnComplete);

		return false;
	}

	return true;
}

bool UModularCloudSavesSubsystem::EnumerateFiles(const int32 LocalPlayerIndex, FModularCloudFilesDelegate OnComplete)
{
	const auto UserFile = GetInterface<UE::Online::IUserFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!UserFile.IsValid())
	{
		AnnounceEnumerated(TArray<FString> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceEnumerated(TArray<FString> { }, NotSignedIn(), OnComplete);

		return false;
	}

	UE::Online::FUserFileEnumerateFiles::Params Params;
	Params.LocalAccountId = Account;

	UserFile->EnumerateFiles(MoveTemp(Params)).OnComplete(this, [this, LocalPlayerIndex, OnComplete](const UE::Online::TOnlineResult<UE::Online::FUserFileEnumerateFiles>& Result)
	{
		if (Result.IsError())
		{
			AnnounceEnumerated(TArray<FString> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), OnComplete);

			return;
		}

		// The operation itself answers with nothing; the list it built is read back separately.
		TArray<FString> Filenames;
		GetEnumeratedFiles(LocalPlayerIndex, Filenames);

		AnnounceEnumerated(Filenames, FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularCloudSavesSubsystem::GetEnumeratedFiles(const int32 LocalPlayerIndex, TArray<FString>& OutFilenames) const
{
	const auto UserFile = GetInterface<UE::Online::IUserFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!UserFile.IsValid() || !Account.IsValid())
	{
		return false;
	}

	UE::Online::FUserFileGetEnumeratedFiles::Params Params;
	Params.LocalAccountId = Account;

	const auto Enumerated = UserFile->GetEnumeratedFiles(MoveTemp(Params));
	if (Enumerated.IsError())
	{
		return false;
	}

	OutFilenames = Enumerated.GetOkValue().Filenames;

	return true;
}

bool UModularCloudSavesSubsystem::ReadFile(const int32 LocalPlayerIndex, const FString& Filename, FModularCloudFileReadDelegate OnComplete)
{
	const auto UserFile = GetInterface<UE::Online::IUserFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!UserFile.IsValid())
	{
		AnnounceRead(Filename, TArray<uint8> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceRead(Filename, TArray<uint8> { }, NotSignedIn(), OnComplete);

		return false;
	}

	UE::Online::FUserFileReadFile::Params Params;
	Params.LocalAccountId = Account;
	Params.Filename = Filename;

	UserFile->ReadFile(MoveTemp(Params)).OnComplete(this, [this, Filename, OnComplete](const UE::Online::TOnlineResult<UE::Online::FUserFileReadFile>& Result)
	{
		if (Result.IsError())
		{
			AnnounceRead(Filename, TArray<uint8> { }, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()), OnComplete);

			return;
		}

		AnnounceRead(Filename, *Result.GetOkValue().FileContents, FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularCloudSavesSubsystem::WriteFile(const int32 LocalPlayerIndex, const FString& Filename, const TArray<uint8>& Contents, FModularCloudFileOperationDelegate OnComplete)
{
	const auto UserFile = GetInterface<UE::Online::IUserFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!CanOperate(UserFile, Account, Filename, OnComplete))
	{
		return false;
	}

	UE::Online::FUserFileWriteFile::Params Params;
	Params.LocalAccountId = Account;
	Params.Filename = Filename;
	Params.FileContents = Contents;

	UserFile->WriteFile(MoveTemp(Params)).OnComplete(this, [this, Filename, OnComplete](const UE::Online::TOnlineResult<UE::Online::FUserFileWriteFile>& Result)
	{
		AnnounceOperation(Filename, Result.IsError() ? FModularOnlineResult::FromOnlineError(Result.GetErrorValue()) : FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularCloudSavesSubsystem::CopyFile(const int32 LocalPlayerIndex, const FString& SourceFilename, const FString& TargetFilename, FModularCloudFileOperationDelegate OnComplete)
{
	const auto UserFile = GetInterface<UE::Online::IUserFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!CanOperate(UserFile, Account, TargetFilename, OnComplete))
	{
		return false;
	}

	UE::Online::FUserFileCopyFile::Params Params;
	Params.LocalAccountId = Account;
	Params.SourceFilename = SourceFilename;
	Params.TargetFilename = TargetFilename;

	UserFile->CopyFile(MoveTemp(Params)).OnComplete(this, [this, TargetFilename, OnComplete](const UE::Online::TOnlineResult<UE::Online::FUserFileCopyFile>& Result)
	{
		AnnounceOperation(TargetFilename, Result.IsError() ? FModularOnlineResult::FromOnlineError(Result.GetErrorValue()) : FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}

bool UModularCloudSavesSubsystem::DeleteFile(const int32 LocalPlayerIndex, const FString& Filename, FModularCloudFileOperationDelegate OnComplete)
{
	const auto UserFile = GetInterface<UE::Online::IUserFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!CanOperate(UserFile, Account, Filename, OnComplete))
	{
		return false;
	}

	UE::Online::FUserFileDeleteFile::Params Params;
	Params.LocalAccountId = Account;
	Params.Filename = Filename;

	UserFile->DeleteFile(MoveTemp(Params)).OnComplete(this, [this, Filename, OnComplete](const UE::Online::TOnlineResult<UE::Online::FUserFileDeleteFile>& Result)
	{
		AnnounceOperation(Filename, Result.IsError() ? FModularOnlineResult::FromOnlineError(Result.GetErrorValue()) : FModularOnlineResult::Success(), OnComplete);
	});

	return true;
}
