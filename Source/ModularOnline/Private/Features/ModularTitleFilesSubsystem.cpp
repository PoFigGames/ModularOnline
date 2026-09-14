// Copyright PoFig Games Studio. All Rights Reserved.

#include "Features/ModularTitleFilesSubsystem.h"

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineTags.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/TitleFile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularTitleFilesSubsystem)

FGameplayTag UModularTitleFilesSubsystem::GetFeatureTag() const
{
	return ModularOnlineTags::Feature_TitleFile;
}

void UModularTitleFilesSubsystem::AnnounceEnumerated(const TArray<FString>& Filenames, const FModularOnlineResult& Result, const FModularTitleFilesDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Filenames, Result);
	OnFilesEnumerated.Broadcast(Filenames, Result);
	K2_OnFilesEnumerated.Broadcast(Filenames, Result);
}

void UModularTitleFilesSubsystem::AnnounceRead(const FString& Filename, const TArray<uint8>& Contents, const FModularOnlineResult& Result, const FModularTitleFileReadDelegate& OnComplete)
{
	OnComplete.ExecuteIfBound(Contents, Result);
	OnFileRead.Broadcast(Filename, Contents, Result);
	K2_OnFileRead.Broadcast(Filename, Contents, Result);
}

bool UModularTitleFilesSubsystem::EnumerateFiles(const int32 LocalPlayerIndex, FModularTitleFilesDelegate OnComplete)
{
	const auto TitleFile = GetInterface<UE::Online::ITitleFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!TitleFile.IsValid())
	{
		AnnounceEnumerated(TArray<FString> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceEnumerated(TArray<FString> { }, NotSignedIn(), OnComplete);

		return false;
	}

	UE::Online::FTitleFileEnumerateFiles::Params Params;
	Params.LocalAccountId = Account;

	TitleFile->EnumerateFiles(MoveTemp(Params)).OnComplete(this, [this, LocalPlayerIndex, OnComplete](const UE::Online::TOnlineResult<UE::Online::FTitleFileEnumerateFiles>& Result)
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

bool UModularTitleFilesSubsystem::GetEnumeratedFiles(const int32 LocalPlayerIndex, TArray<FString>& OutFilenames) const
{
	const auto TitleFile = GetInterface<UE::Online::ITitleFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!TitleFile.IsValid() || !Account.IsValid())
	{
		return false;
	}

	UE::Online::FTitleFileGetEnumeratedFiles::Params Params;
	Params.LocalAccountId = Account;

	const auto Enumerated = TitleFile->GetEnumeratedFiles(MoveTemp(Params));
	if (Enumerated.IsError())
	{
		return false;
	}

	OutFilenames = Enumerated.GetOkValue().Filenames;

	return true;
}

bool UModularTitleFilesSubsystem::ReadFile(const int32 LocalPlayerIndex, const FString& Filename, FModularTitleFileReadDelegate OnComplete)
{
	const auto TitleFile = GetInterface<UE::Online::ITitleFile>();
	const auto Account = GetLocalAccount(LocalPlayerIndex);

	if (!TitleFile.IsValid())
	{
		AnnounceRead(Filename, TArray<uint8> { }, MissingFeature(), OnComplete);

		return false;
	}

	if (!Account.IsValid())
	{
		AnnounceRead(Filename, TArray<uint8> { }, NotSignedIn(), OnComplete);

		return false;
	}

	UE::Online::FTitleFileReadFile::Params Params;
	Params.LocalAccountId = Account;
	Params.Filename = Filename;

	TitleFile->ReadFile(MoveTemp(Params)).OnComplete(this, [this, Filename, OnComplete](const UE::Online::TOnlineResult<UE::Online::FTitleFileReadFile>& Result)
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
