// Copyright PoFig Games Studio. All Rights Reserved.

#include "User/ModularUserSubsystem.h"

#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineSettings.h"
#include "Core/ModularOnlineSubsystem.h"
#include "Core/ModularOnlineTags.h"
#include "User/ModularUserInfo.h"
#include "User/ModularUserTags.h"
#include "Online/Auth.h"
#include "Online/AuthErrors.h"
#include "Online/ExternalUI.h"
#include "Online/OnlineErrorDefinitions.h"
#include "Online/OnlineResult.h"
#include "Online/Privileges.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularUserSubsystem)

using PoFigGames::Online::LexToString;

namespace PoFigGames::Online::Private
{
	/** The step that follows another, which is the whole order of the login in one place. */
	static EModularLoginStep NextStep(const EModularLoginStep Step)
	{
		switch (Step)
		{
		case EModularLoginStep::PlatformLogin:
			return EModularLoginStep::TransferAuth;

		case EModularLoginStep::TransferAuth:
			return EModularLoginStep::ServiceLogin;

		case EModularLoginStep::ServiceLogin:
			return EModularLoginStep::PrivilegeCheck;

		case EModularLoginStep::PrivilegeCheck:
		case EModularLoginStep::Finished:
			return EModularLoginStep::Finished;
		}

		return EModularLoginStep::Finished;
	}

	/** A login refused because the account is already signed in has done what was asked of it. */
	static bool IsAlreadySignedIn(const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result)
	{
		return Result.IsError() && Result.GetErrorValue() == UE::Online::Errors::Auth::AlreadyLoggedIn();
	}
}

void UModularUserSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UModularOnlineSubsystem>();

	const auto GameInstance = GetGameInstance();
	bIsDedicatedServer = GameInstance && GameInstance->IsDedicatedServerInstance();

	if (!bIsDedicatedServer)
	{
		IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddUObject(this, &ThisClass::HandleInputDeviceConnectionChanged);
	}
}

void UModularUserSubsystem::Deinitialize()
{
	StopListeningForLoginKeys();

	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().RemoveAll(this);

	ExternalUIHandles.Reset();
	LoginStatusHandles.Reset();
	ActiveLogins.Reset();
	Users.Reset();

	Super::Deinitialize();
}

bool UModularUserSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is not a game specific subclass
	return ChildClasses.Num() == 0;
}

UModularOnlineSubsystem* UModularUserSubsystem::GetOnline() const
{
	const auto GameInstance = GetGameInstance();

	return GameInstance ? GameInstance->GetSubsystem<UModularOnlineSubsystem>() : nullptr;
}

bool UModularUserSubsystem::HasSeparateServiceProvider() const
{
	const auto Online = GetOnline();

	return Online && Online->GetContext(EModularOnlineRole::Platform) != Online->GetContext(EModularOnlineRole::Service);
}

EModularOnlineRole UModularUserSubsystem::GetServiceRole() const
{
	return HasSeparateServiceProvider() ? EModularOnlineRole::Service : EModularOnlineRole::Default;
}

void UModularUserSubsystem::SetTraitTags(const FGameplayTagContainer& InTraits)
{
	TraitTags = InTraits;
}

bool UModularUserSubsystem::ShouldWaitForStartInput() const
{
	// A platform that maps controllers to accounts has to know which controller pressed before it can
	// pick a user. One that has a single online user has nothing to pick.
	return HasTrait(ModularUserTags::Trait_RequiresStartInput);
}

void UModularUserSubsystem::SetMaxLocalPlayers(const int32 InMaxLocalPlayers)
{
	if (ensure(InMaxLocalPlayers >= 1))
	{
		MaxLocalPlayers = InMaxLocalPlayers;
	}
}

int32 UModularUserSubsystem::GetMaxLocalPlayers() const
{
	return MaxLocalPlayers;
}

const UModularUserInfo* UModularUserSubsystem::GetUserForLocalPlayerIndex(const int32 LocalPlayerIndex) const
{
	if (const auto Found = Users.Find(LocalPlayerIndex))
	{
		return *Found;
	}

	return nullptr;
}

const UModularUserInfo* UModularUserSubsystem::GetUserForPlatformUser(const FPlatformUserId PlatformUser) const
{
	if (!PlatformUser.IsValid())
	{
		return nullptr;
	}

	for (const auto& Pair : Users)
	{
		// Guests share the system user of the player they are a guest of, so they never answer for it.
		if (Pair.Value && Pair.Value->PlatformUser == PlatformUser && !Pair.Value->bIsGuest)
		{
			return Pair.Value;
		}
	}

	return nullptr;
}

const UModularUserInfo* UModularUserSubsystem::GetUserForInputDevice(const FInputDeviceId InputDevice) const
{
	for (const auto& Pair : Users)
	{
		if (Pair.Value && Pair.Value->PrimaryInputDevice == InputDevice)
		{
			return Pair.Value;
		}
	}

	return nullptr;
}

TArray<const UModularUserInfo*> UModularUserSubsystem::GetAllUsers() const
{
	TArray<const UModularUserInfo*> Result;
	Result.Reserve(Users.Num());

	for (const auto& Pair : Users)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Value);
		}
	}

	return Result;
}

UModularUserInfo* UModularUserSubsystem::FindOrCreateUser(const int32 LocalPlayerIndex)
{
	if (const auto Found = Users.Find(LocalPlayerIndex))
	{
		return *Found;
	}

	const auto User = NewObject<UModularUserInfo>(this);
	User->LocalPlayerIndex = LocalPlayerIndex;
	Users.Add(LocalPlayerIndex, User);

	return User;
}

void UModularUserSubsystem::ResetUsers()
{
	ActiveLogins.Reset();

	for (const auto& Pair : Users)
	{
		if (Pair.Value)
		{
			Pair.Value->MarkAsGarbage();
		}
	}

	Users.Reset();
}

void UModularUserSubsystem::SetUserState(UModularUserInfo* User, const EModularUserState NewState)
{
	if (!User || User->State == NewState)
	{
		return;
	}

	User->State = NewState;

	OnUserStateChanged.Broadcast(User);
	K2_OnUserStateChanged.Broadcast(User);
}

void UModularUserSubsystem::UpdatePrivilege(UModularUserInfo* User, const EModularOnlinePrivilege Privilege, const EModularOnlinePrivilegeResult Result, const EModularOnlineRole Role)
{
	if (!User)
	{
		return;
	}

	const auto Online = GetOnline();

	// Filed under the role that answers, like everything else kept per role: a named role that resolves
	// to Default must not leave an empty record behind that later lookups find instead of the real one.
	auto& Data = User->FindOrAddRoleData(Online ? Online->ResolveRole(Role) : Role);

	if (const auto Previous = Data.Privileges.Find(Privilege); Previous && *Previous == Result)
	{
		return;
	}

	Data.Privileges.Add(Privilege, Result);

	OnPrivilegeChanged.Broadcast(User, Privilege, Result);
	K2_OnPrivilegeChanged.Broadcast(User, Privilege, Result);
}

void UModularUserSubsystem::RefreshRoleData(UModularUserInfo* User, const EModularOnlineRole Role)
{
	const auto Online = GetOnline();
	const auto Auth = Online ? Online->GetInterface<UE::Online::IAuth>(Role) : nullptr;

	if (!User || !Auth.IsValid())
	{
		return;
	}

	const auto AccountResult = Auth->GetLocalOnlineUserByPlatformUserId({ User->PlatformUser });
	if (!AccountResult.IsOk())
	{
		return;
	}

	const auto AccountInfo = AccountResult.GetOkValue().AccountInfo;

	// Filed under the role that served the call, not the one asked for: every lookup resolves a named role
	// to Default, so data filed under a name nobody resolves to is data nobody reads.
	auto& Data = User->FindOrAddRoleData(Online->ResolveRole(Role));
	Data.AccountId = AccountInfo->AccountId;

	if (const auto DisplayName = AccountInfo->Attributes.Find(UE::Online::AccountAttributeData::DisplayName))
	{
		Data.Nickname = DisplayName->GetString();
	}

	// The display name is a standard attribute; the avatar is not, and every provider names it
	// differently, so the key comes from the settings and a new platform needs no rebuild.
	const auto Context = Online->GetContext(Role);
	const auto Settings = GetDefault<UModularAccountSettings>();

	if (const auto AvatarAttribute = Settings && Context ? Settings->GetAvatarAttribute(Context->GetProviderName()) : FName { }; !AvatarAttribute.IsNone())
	{
		if (const auto AvatarUrl = AccountInfo->Attributes.Find(AvatarAttribute))
		{
			Data.AvatarUrl = AvatarUrl->GetString();
		}
	}
}

void UModularUserSubsystem::BindServiceEvents()
{
	const auto Online = GetOnline();
	if (!Online)
	{
		return;
	}

	// A handle taken from the previous services listens to services that are gone. The instance name is
	// how a new world is recognised, and outside the editor GetServicesInstanceName answers nothing at all
	// (checked against 5.8.3 on 2026-09-15).
	if (const auto Instance = Online->GetBoundInstanceName(); Instance != BoundToInstance || !bInstanceKnown)
	{
		BoundToInstance = Instance;
		bInstanceKnown = true;

		LoginStatusHandles.Reset();
		ExternalUIHandles.Reset();
	}

	// Bound here rather than in Initialize: the contexts are built for a world, and there is none yet when
	// subsystems are. Only a role with a provider of its own, or the same event arrives twice.
	for (const auto Role : { EModularOnlineRole::Default, EModularOnlineRole::Platform, EModularOnlineRole::Service })
	{
		if (const auto bWorthBinding = !LoginStatusHandles.Contains(Role) && Online->HasDedicatedProvider(Role); bWorthBinding)
		{
			if (const auto Auth = Online->GetInterface<UE::Online::IAuth>(Role))
			{
				LoginStatusHandles.Add(Role, Auth->OnLoginStatusChanged().Add(this, &ThisClass::HandleLoginStatusChanged, Role));
			}
		}

		if (const auto bWorthBinding = !ExternalUIHandles.Contains(Role) && Online->HasDedicatedProvider(Role); bWorthBinding)
		{
			if (const auto ExternalUI = Online->GetInterface<UE::Online::IExternalUI>(Role))
			{
				ExternalUIHandles.Add(Role, ExternalUI->OnExternalUIStatusChanged().Add(this, &ThisClass::HandleExternalUIStatusChanged));
			}
		}
	}
}

void UModularUserSubsystem::HandleLoginStatusChanged(const UE::Online::FAuthLoginStatusChanged& EventParameters, const EModularOnlineRole Role)
{
	const auto PlatformUser = EventParameters.AccountInfo->PlatformUserId;
	const auto User = ModifyUser(GetUserForPlatformUser(PlatformUser));

	UE_LOG(LogModularOnline, Log, TEXT("Login status on role %s changed to %s for system user %d"),
		*LexToString(Role),
		LexToString(EventParameters.LoginStatus),
		PlatformUser.GetInternalId());

	if (!User || EventParameters.LoginStatus != UE::Online::ELoginStatus::NotLoggedIn)
	{
		return;
	}

	// A record with nothing on this role was not signed in on it - which is what a player who asked to
	// sign out is left holding, and not something to report back to them as a failure.
	const auto PreviousAccount = User->GetAccountId(Role);
	const auto bWasSignedIn = PreviousAccount.IsValid();

	// Somebody signed out behind our back: the account of that role is gone, and so is anything it
	// allowed. Whether the player can still play depends on which role it was.
	auto& Data = User->FindOrAddRoleData(Role);
	Data = UModularUserInfo::FRoleData { };

	if (!bWasSignedIn)
	{
		return;
	}

	if (Role == GetServiceRole() && HasSeparateServiceProvider())
	{
		SetUserState(User, EModularUserState::LoggedInLocally);
	}
	else
	{
		SetUserState(User, EModularUserState::LoginFailed);
	}
}

EModularStepPolicy UModularUserSubsystem::GetStepPolicy(const EModularLoginStep Step, const FModularLoginParams& Params) const
{
	const auto Online = GetOnline();

	// With no provider there is nobody to sign in to and the player plays locally: a supported state, not
	// an error - an offline build, or a platform with nothing configured yet.
	if (!Online || !Online->HasAnyProvider())
	{
		return EModularStepPolicy::Skip;
	}

	switch (Step)
	{
	case EModularLoginStep::PlatformLogin:
		// Signing in to the machine is the one step that always applies; everything else builds on it.
		return EModularStepPolicy::Required;

	case EModularLoginStep::TransferAuth:
		// Carrying the identity over only means something when there are two providers, and it is never
		// required: a backend that refuses the platform token can still be signed in to on its own.
		return HasSeparateServiceProvider() ? EModularStepPolicy::Optional : EModularStepPolicy::Skip;

	case EModularLoginStep::ServiceLogin:
		if (!HasSeparateServiceProvider())
		{
			return EModularStepPolicy::Skip;
		}

		// A player who only asked to play does not have to reach the backend; one who asked for anything
		// online does.
		return Params.RequestedPrivilege == EModularOnlinePrivilege::CanPlay ? EModularStepPolicy::Optional : EModularStepPolicy::Required;

	case EModularLoginStep::PrivilegeCheck:
		// Strict on purpose: whatever the services answer other than "available" fails the login.
		return EModularStepPolicy::Required;

	case EModularLoginStep::Finished:
		return EModularStepPolicy::Skip;
	}

	return EModularStepPolicy::Skip;
}

bool UModularUserSubsystem::CanBecomeGuest(const TSharedRef<FLoginRequest>& Request) const
{
	const auto User = Request->User.Get();

	// Only "this player has no account" becomes a guest. A refusal of the account they do have - age
	// restricted, licence invalid - is a refusal of the player, and a guest seat would walk around it.
	const auto bHasNoAccount = Request->Failure.IsSet()
		&& Request->Failure->Category == EModularOnlineErrorCategory::NotLoggedIn
		&& Request->FailedStep == EModularLoginStep::PlatformLogin;

	// Player zero is the one the platform signs in; a guest is somebody who joined them at the same
	// machine, so there is always a real account behind the session.
	return User && !User->bIsGuest && User->LocalPlayerIndex != 0
		&& bHasNoAccount
		&& Request->Params.bAllowGuest
		&& HasTrait(ModularUserTags::Trait_AllowsGuests.GetTag());
}

bool UModularUserSubsystem::LoginLocalUser(const FModularLoginParams& Params, FModularUserLoginCompleteDelegate OnComplete)
{
	if (bIsDedicatedServer)
	{
		UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: a dedicated server has no local players."));

		return false;
	}

	if (Params.LocalPlayerIndex < 0 || Params.LocalPlayerIndex >= MaxLocalPlayers)
	{
		UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: player index %d is outside the %d this game allows."), Params.LocalPlayerIndex, MaxLocalPlayers);

		return false;
	}

	if (Params.LocalPlayerIndex == 0 && Params.bAllowGuest)
	{
		UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: the primary player cannot be a guest."));

		return false;
	}

	// One online user and everybody local shares it: a second player here is a guest of the first or is
	// nobody, because there is no second account for them to be signed into.
	if (Params.LocalPlayerIndex != 0 && !Params.bAllowGuest && HasTrait(ModularUserTags::Trait_SingleOnlineUser.GetTag()))
	{
		UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: this platform has one online user, so player %d can only join as a guest."),
			Params.LocalPlayerIndex);

		return false;
	}

	auto User = FindOrCreateUser(Params.LocalPlayerIndex);
	if (User->IsLoggingIn())
	{
		UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: player %d is already signing in."), Params.LocalPlayerIndex);

		return false;
	}

	auto ResolvedParams = Params;

	// A caller can name the controller, the system user, or neither; the device mapper fills in the rest.
	auto& DeviceMapper = IPlatformInputDeviceMapper::Get();
	const auto bDeviceNamed = ResolvedParams.InputDevice.IsValid();

	if (ResolvedParams.InputDevice.IsValid() && !ResolvedParams.PlatformUser.IsValid())
	{
		ResolvedParams.PlatformUser = DeviceMapper.GetUserForInputDevice(ResolvedParams.InputDevice);
	}

	if (!ResolvedParams.PlatformUser.IsValid())
	{
		ResolvedParams.PlatformUser = DeviceMapper.GetPrimaryPlatformUser();

		// The primary system user belongs to player 0, so anybody else landing on it comes back with the
		// same account id. Where no trait gates that, the caller has to name the device or system user.
		UE_CLOG(ResolvedParams.LocalPlayerIndex != 0, LogModularOnline, Warning,
			TEXT("Player %d named neither a controller nor a system user and is signing in as the primary one; both players will carry the same account."),
			ResolvedParams.LocalPlayerIndex);
	}

	if (!ResolvedParams.InputDevice.IsValid())
	{
		ResolvedParams.InputDevice = DeviceMapper.GetPrimaryInputDeviceForUser(ResolvedParams.PlatformUser);
	}

	// Only a controller the caller named is one this player is claiming: the fallback above is the primary
	// device, which belongs to the first player by definition.
	if (const auto Holder = bDeviceNamed ? GetUserForInputDevice(ResolvedParams.InputDevice) : nullptr; Holder && Holder != User)
	{
		UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: controller %d already belongs to player %d."), ResolvedParams.InputDevice.GetId(), Holder->LocalPlayerIndex);

		return false;
	}

	if (User->PlatformUser.IsValid() && User->PlatformUser != ResolvedParams.PlatformUser)
	{
		// A different system user at the same local index. Platforms that allow swapping say so with a
		// trait; elsewhere a signed in player keeps their account until the caller signs them out.
		if (User->IsLoggedIn() && !HasTrait(ModularUserTags::Trait_SupportsUserSwitch.GetTag()))
		{
			UE_LOG(LogModularOnline, Warning, TEXT("LoginLocalUser refused: player %d is signed in as system user %d and this platform does not switch users."),
				Params.LocalPlayerIndex, User->PlatformUser.GetInternalId());

			return false;
		}

		// Nothing of the previous account may survive into the new one: identity, names and every answer
		// about privileges belonged to somebody else.
		Users.Remove(Params.LocalPlayerIndex);
		User->MarkAsGarbage();

		User = FindOrCreateUser(Params.LocalPlayerIndex);
	}

	User->PlatformUser = ResolvedParams.PlatformUser;
	User->PrimaryInputDevice = ResolvedParams.InputDevice;

	BindServiceEvents();
	SetUserState(User, EModularUserState::LoggingIn);

	const auto Request = MakeShared<FLoginRequest>();
	Request->User = User;
	Request->Params = ResolvedParams;
	Request->OnComplete = MoveTemp(OnComplete);

	ActiveLogins.Add(Request);

	UE_LOG(LogModularOnline, Log, TEXT("Signing in %s for privilege %s"),
		*User->ToDebugString(),
		*LexToString(ResolvedParams.RequestedPrivilege));

	AdvanceLogin(Request);

	return true;
}

bool UModularUserSubsystem::CancelLogin(const int32 LocalPlayerIndex)
{
	auto bCancelledAny = false;

	for (auto Index = ActiveLogins.Num() - 1; Index >= 0; --Index)
	{
		const auto& Request = ActiveLogins[Index];

		if (const auto User = Request->User.Get(); User && User->LocalPlayerIndex == LocalPlayerIndex)
		{
			// Marked as well as forgotten: a late answer finds the request gone from the active list only
			// after it has written an account into a player nobody is signing in, so it checks this flag.
			Request->bCancelled = true;

			// The steps that already finished stand; what was in flight is simply no longer waited for.
			const auto RemainingAccount = User->GetAccountId();
			SetUserState(User, RemainingAccount.IsValid() ? EModularUserState::LoggedInLocally : EModularUserState::Unknown);

			ActiveLogins.RemoveAt(Index);
			bCancelledAny = true;
		}
	}

	return bCancelledAny;
}

bool UModularUserSubsystem::LogoutLocalUser(const int32 LocalPlayerIndex)
{
	const auto User = ModifyUser(GetUserForLocalPlayerIndex(LocalPlayerIndex));
	if (!User)
	{
		return false;
	}

	CancelLogin(LocalPlayerIndex);

	if (User->bIsGuest)
	{
		// A guest has nothing on any service; forgetting them is the whole of signing them out. The state
		// is set before the record goes, or the event says a state changed and carries one that did not.
		SetUserState(User, EModularUserState::Unknown);
		Users.Remove(LocalPlayerIndex);

		return true;
	}

	const auto Online = GetOnline();

	// Asked of the roles as they resolve, not as they are named: on a single provider both names answer
	// to the same services, and signing the same account out twice is refused the second time.
	TSet<EModularOnlineRole> SignedOut;

	for (const auto Named : { EModularOnlineRole::Platform, EModularOnlineRole::Service })
	{
		const auto Role = Online ? Online->ResolveRole(Named) : Named;

		const auto AccountId = User->GetAccountId(Role);
		const auto Auth = Online ? Online->GetInterface<UE::Online::IAuth>(Role) : nullptr;

		if (!SignedOut.Contains(Role) && AccountId.IsValid() && Auth.IsValid())
		{
			SignedOut.Add(Role);

			UE::Online::FAuthLogout::Params LogoutParams;
			LogoutParams.LocalAccountId = AccountId;

			Auth->Logout(MoveTemp(LogoutParams)).OnComplete(this, [](const UE::Online::TOnlineResult<UE::Online::FAuthLogout>& Result)
			{
				UE_CLOG(Result.IsError(), LogModularOnline, Warning, TEXT("Signing out of the services failed: %s"), *Result.GetErrorValue().GetLogString());
			});
		}
	}

	if (LocalPlayerIndex == 0)
	{
		// The primary player keeps their record, because the game always has one; it is simply emptied.
		Users.Remove(0);
		User->MarkAsGarbage();

		const auto Replacement = FindOrCreateUser(0);
		Replacement->PlatformUser = User->PlatformUser;
		Replacement->PrimaryInputDevice = User->PrimaryInputDevice;

		OnUserStateChanged.Broadcast(Replacement);
		K2_OnUserStateChanged.Broadcast(Replacement);
	}
	else
	{
		SetUserState(User, EModularUserState::Unknown);
		Users.Remove(LocalPlayerIndex);
	}

	return true;
}

void UModularUserSubsystem::AdvanceLogin(const TSharedRef<FLoginRequest>& Request)
{
	// The login walks its steps in order and never goes back; a step that reached a service returns from
	// here and is carried on by the answer, so each remaining step runs at most once.
	while (Request->Step != EModularLoginStep::Finished)
	{
		if (!ActiveLogins.Contains(Request))
		{
			// Cancelled, or already finished from inside a step.
			return;
		}

		const auto User = Request->User.Get();
		if (!User || Request->bCancelled)
		{
			// The player went away while a service was answering; nobody is left to tell.
			ActiveLogins.Remove(Request);

			return;
		}

		if (Request->Failure.IsSet())
		{
			// A required step failed. Before giving up, see whether this player may sit as a guest of the
			// primary one - what a second controller does on a platform with no second account.
			if (CanBecomeGuest(Request))
			{
				UE_LOG(LogModularOnline, Log, TEXT("Player %d could not sign in (%s) and continues as a guest."),
					User->LocalPlayerIndex, *Request->Failure->ToLogString());

				User->bIsGuest = true;
				Request->Failure.Reset();

				// The privilege step is skipped from here and a guest has nothing to ask about anyway, but
				// their permission to play has to be written down or every reader sees no answer at all.
				UpdatePrivilege(User, EModularOnlinePrivilege::CanPlay, EModularOnlinePrivilegeResult::Available, EModularOnlineRole::Default);
			}

			Request->Step = EModularLoginStep::Finished;

			break;
		}

		const auto StepBefore = Request->Step;
		auto bWaitingOnService = false;

		switch (Request->Step)
		{
		case EModularLoginStep::PlatformLogin:
			bWaitingOnService = RunPlatformLogin(Request);
			break;

		case EModularLoginStep::TransferAuth:
			bWaitingOnService = RunTransferAuth(Request);
			break;

		case EModularLoginStep::ServiceLogin:
			bWaitingOnService = RunServiceLogin(Request);
			break;

		case EModularLoginStep::PrivilegeCheck:
			bWaitingOnService = RunPrivilegeCheck(Request);
			break;

		case EModularLoginStep::Finished:
			// Unreachable: it is the condition of the loop.
			break;
		}

		if (bWaitingOnService)
		{
			// A service is working on it and will carry the login on through CompleteStep.
			return;
		}

		// A step that did not reach a service has to have moved the login on by itself. One that does
		// neither is a defect, and ending the login here says so rather than spinning.
		if (!ensureMsgf(Request->Step != StepBefore, TEXT("Login step %s neither started a request nor moved on"), *LexToString(StepBefore)))
		{
			Request->Failure = FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidState());
			Request->Step = EModularLoginStep::Finished;
		}
	}

	FinishLogin(Request);
}

void UModularUserSubsystem::ApplyStepResult(const TSharedRef<FLoginRequest>& Request, const FModularOnlineResult& Result) const
{
	if (!Result.bWasSuccessful)
	{
		const auto Policy = GetStepPolicy(Request->Step, Request->Params);

		UE_LOG(LogModularOnline, Log, TEXT("Login step %s ended with %s (%s)"),
			*LexToString(Request->Step),
			*Result.ToLogString(),
			Policy == EModularStepPolicy::Required ? TEXT("required") : TEXT("optional"));

		if (Policy == EModularStepPolicy::Required)
		{
			Request->Failure = Result;
			Request->FailedStep = Request->Step;
		}
	}

	Request->bStepInFlight = false;
	Request->Step = PoFigGames::Online::Private::NextStep(Request->Step);
}

void UModularUserSubsystem::CompleteStep(const TSharedRef<FLoginRequest>& Request, const FModularOnlineResult& Result)
{
	ApplyStepResult(Request, Result);
	AdvanceLogin(Request);
}

void UModularUserSubsystem::FinishLogin(const TSharedRef<FLoginRequest>& Request)
{
	ActiveLogins.Remove(Request);

	const auto User = Request->User.Get();
	if (!User)
	{
		return;
	}

	const auto Result = Request->Failure.IsSet() ? Request->Failure.GetValue() : FModularOnlineResult::Success();

	if (Result.bWasSuccessful)
	{
		SetUserState(User, Request->bSignedInOnService ? EModularUserState::LoggedInOnline : EModularUserState::LoggedInLocally);
	}
	else
	{
		SetUserState(User, EModularUserState::LoginFailed);
	}

	UE_LOG(LogModularOnline, Log, TEXT("Login finished: %s, %s"), *User->ToDebugString(), *Result.ToLogString());

	Request->OnComplete.ExecuteIfBound(User, Result);
	OnLoginComplete.Broadcast(User, Result);
	K2_OnLoginComplete.Broadcast(User, Result);
}

bool UModularUserSubsystem::StartAutoLogin(const TSharedRef<FLoginRequest>& Request, const EModularOnlineRole Role)
{
	const auto User = Request->User.Get();
	const auto Online = GetOnline();
	const auto Auth = Online ? Online->GetInterface<UE::Online::IAuth>(Role) : nullptr;

	if (!User || !Auth.IsValid())
	{
		ApplyStepResult(Request, FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Auth));

		return false;
	}

	UE::Online::FAuthLogin::Params LoginParams;
	LoginParams.PlatformUserId = User->PlatformUser;
	LoginParams.CredentialsType = UE::Online::LoginCredentialsType::Auto;

	Request->bStepInFlight = true;

	Auth->Login(MoveTemp(LoginParams)).OnComplete(this, [this, Request, Role](const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result)
	{
		const auto LoggedInUser = Request->User.Get();
		if (!LoggedInUser || Request->bCancelled)
		{
			// Nobody is waiting for this any more. Writing the account into the player would sign in
			// somebody who asked to stop, which is the one thing cancelling was supposed to prevent.
			ActiveLogins.Remove(Request);

			return;
		}

		if (Result.IsOk() || PoFigGames::Online::Private::IsAlreadySignedIn(Result))
		{
			RefreshRoleData(LoggedInUser, Role);

			if (Role == GetServiceRole())
			{
				Request->bSignedInOnService = true;
			}

			CompleteStep(Request, FModularOnlineResult::Success());

			return;
		}

		// The services refused to sign the player in by themselves. If the caller allows it, the platform
		// may still ask the player directly.
		if (Request->Params.bAllowLoginUI && StartLoginUI(Request, Role))
		{
			return;
		}

		CompleteStep(Request, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
	});

	return true;
}

bool UModularUserSubsystem::StartLoginUI(const TSharedRef<FLoginRequest>& Request, const EModularOnlineRole Role)
{
	const auto User = Request->User.Get();
	const auto Online = GetOnline();
	const auto ExternalUI = Online ? Online->GetInterface<UE::Online::IExternalUI>(Role) : nullptr;

	if (!User || !ExternalUI.IsValid())
	{
		return false;
	}

	UE::Online::FExternalUIShowLoginUI::Params ShowParams;
	ShowParams.PlatformUserId = User->PlatformUser;

	ExternalUI->ShowLoginUI(MoveTemp(ShowParams)).OnComplete(this, [this, Request, Role](const UE::Online::TOnlineResult<UE::Online::FExternalUIShowLoginUI>& Result)
	{
		const auto ShownUser = Request->User.Get();
		if (!ShownUser || Request->bCancelled)
		{
			ActiveLogins.Remove(Request);

			return;
		}

		if (Result.IsOk())
		{
			RefreshRoleData(ShownUser, Role);

			if (Role == GetServiceRole())
			{
				Request->bSignedInOnService = true;
			}

			CompleteStep(Request, FModularOnlineResult::Success());

			return;
		}

		CompleteStep(Request, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));
	});

	return true;
}

bool UModularUserSubsystem::RunPlatformLogin(const TSharedRef<FLoginRequest>& Request)
{
	if (GetStepPolicy(EModularLoginStep::PlatformLogin, Request->Params) == EModularStepPolicy::Skip)
	{
		Request->Step = PoFigGames::Online::Private::NextStep(Request->Step);

		return false;
	}

	const auto User = Request->User.Get();
	const auto Online = GetOnline();
	const auto Auth = Online ? Online->GetInterface<UE::Online::IAuth>(EModularOnlineRole::Platform) : nullptr;

	if (!User || !Auth.IsValid())
	{
		ApplyStepResult(Request, FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Auth));

		return false;
	}

	// Somebody may already be signed in, which is the normal state of a PC store client and of a console
	// that signed its user in before the game started.
	if (const auto AccountResult = Auth->GetLocalOnlineUserByPlatformUserId({ User->PlatformUser });
		AccountResult.IsOk() && AccountResult.GetOkValue().AccountInfo->LoginStatus == UE::Online::ELoginStatus::LoggedIn)
	{
		RefreshRoleData(User, EModularOnlineRole::Platform);
		ApplyStepResult(Request, FModularOnlineResult::Success());

		return false;
	}

	return StartAutoLogin(Request, EModularOnlineRole::Platform);
}

bool UModularUserSubsystem::RunTransferAuth(const TSharedRef<FLoginRequest>& Request)
{
	if (GetStepPolicy(EModularLoginStep::TransferAuth, Request->Params) == EModularStepPolicy::Skip)
	{
		Request->Step = PoFigGames::Online::Private::NextStep(Request->Step);

		return false;
	}

	const auto User = Request->User.Get();
	const auto Online = GetOnline();
	const auto PlatformAuth = Online ? Online->GetInterface<UE::Online::IAuth>(EModularOnlineRole::Platform) : nullptr;
	const auto ServiceAuth = Online ? Online->GetInterface<UE::Online::IAuth>(EModularOnlineRole::Service) : nullptr;

	if (!User || !PlatformAuth.IsValid() || !ServiceAuth.IsValid())
	{
		ApplyStepResult(Request, FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Auth));

		return false;
	}

	const auto PlatformAccount = User->GetAccountId(EModularOnlineRole::Platform);
	if (!PlatformAccount.IsValid())
	{
		// Nothing to carry: the platform step did not sign anybody in.
		ApplyStepResult(Request, FModularOnlineResult::FromOnlineError(UE::Online::Errors::NotLoggedIn()));

		return false;
	}

	UE::Online::FAuthQueryExternalAuthToken::Params TokenParams;
	TokenParams.LocalAccountId = PlatformAccount;

	Request->bStepInFlight = true;

	PlatformAuth->QueryExternalAuthToken(MoveTemp(TokenParams)).OnComplete(this, [this, Request, ServiceAuth](const UE::Online::TOnlineResult<UE::Online::FAuthQueryExternalAuthToken>& TokenResult)
	{
		const auto TokenUser = Request->User.Get();
		if (!TokenUser || Request->bCancelled)
		{
			ActiveLogins.Remove(Request);

			return;
		}

		if (TokenResult.IsError())
		{
			CompleteStep(Request, FModularOnlineResult::FromOnlineError(TokenResult.GetErrorValue()));

			return;
		}

		UE::Online::FAuthLogin::Params LoginParams;
		LoginParams.PlatformUserId = TokenUser->PlatformUser;
		LoginParams.CredentialsType = UE::Online::LoginCredentialsType::ExternalAuth;
		LoginParams.CredentialsToken.Emplace<UE::Online::FExternalAuthToken>(TokenResult.GetOkValue().ExternalAuthToken);

		ServiceAuth->Login(MoveTemp(LoginParams)).OnComplete(this, [this, Request](const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& LoginResult)
		{
			const auto SignedInUser = Request->User.Get();
			if (!SignedInUser || Request->bCancelled)
			{
				ActiveLogins.Remove(Request);

				return;
			}

			if (LoginResult.IsOk() || PoFigGames::Online::Private::IsAlreadySignedIn(LoginResult))
			{
				RefreshRoleData(SignedInUser, EModularOnlineRole::Service);

				Request->bPlatformAuthCarried = true;
				Request->bSignedInOnService = true;

				CompleteStep(Request, FModularOnlineResult::Success());

				return;
			}

			CompleteStep(Request, FModularOnlineResult::FromOnlineError(LoginResult.GetErrorValue()));
		});
	});

	return true;
}

bool UModularUserSubsystem::RunServiceLogin(const TSharedRef<FLoginRequest>& Request)
{
	if (GetStepPolicy(EModularLoginStep::ServiceLogin, Request->Params) == EModularStepPolicy::Skip || Request->bPlatformAuthCarried)
	{
		// Carrying the platform identity over is a backend login; doing it twice would only ask the
		// player to sign in again for no reason.
		Request->Step = PoFigGames::Online::Private::NextStep(Request->Step);

		return false;
	}

	const auto User = Request->User.Get();
	const auto Online = GetOnline();
	const auto ServiceAuth = Online ? Online->GetInterface<UE::Online::IAuth>(EModularOnlineRole::Service) : nullptr;

	if (!User || !ServiceAuth.IsValid())
	{
		ApplyStepResult(Request, FModularOnlineResult::NotSupported(ModularOnlineTags::Feature_Auth));

		return false;
	}

	if (const auto AccountResult = ServiceAuth->GetLocalOnlineUserByPlatformUserId({ User->PlatformUser });
		AccountResult.IsOk() && AccountResult.GetOkValue().AccountInfo->LoginStatus == UE::Online::ELoginStatus::LoggedIn)
	{
		RefreshRoleData(User, EModularOnlineRole::Service);
		Request->bSignedInOnService = true;
		ApplyStepResult(Request, FModularOnlineResult::Success());

		return false;
	}

	return StartAutoLogin(Request, EModularOnlineRole::Service);
}

EModularOnlinePrivilegeResult UModularUserSubsystem::AnswerUnaskedPrivilege(const EModularOnlinePrivilege Privilege) const
{
	// A platform that says it gates players and cannot be asked is a configuration that cannot be honoured.
	// Elsewhere a provider that gates nobody grants; Steam had no privileges component on 2026-09-15.
	if (HasTrait(ModularUserTags::Trait_RequiresPrivilegeCheck.GetTag()))
	{
		UE_LOG(LogModularOnline, Error, TEXT("This platform requires a privilege check and its provider has no privileges component; %s cannot be answered."),
			*LexToString(Privilege));

		return EModularOnlinePrivilegeResult::PlatformFailure;
	}

	UE_LOG(LogModularOnline, Verbose, TEXT("No privileges component on this provider; %s is taken as granted."), *LexToString(Privilege));

	return EModularOnlinePrivilegeResult::Available;
}


bool UModularUserSubsystem::RunPrivilegeCheck(const TSharedRef<FLoginRequest>& Request)
{
	if (GetStepPolicy(EModularLoginStep::PrivilegeCheck, Request->Params) == EModularStepPolicy::Skip)
	{
		Request->Step = PoFigGames::Online::Private::NextStep(Request->Step);

		return false;
	}

	const auto User = Request->User.Get();
	const auto Online = GetOnline();

	if (!User)
	{
		ApplyStepResult(Request, FModularOnlineResult::FromOnlineError(UE::Online::Errors::InvalidUser()));

		return false;
	}

	// A guest has no account to ask about and is allowed exactly one thing: to play.
	if (User->bIsGuest)
	{
		UpdatePrivilege(User, EModularOnlinePrivilege::CanPlay, EModularOnlinePrivilegeResult::Available, EModularOnlineRole::Default);
		ApplyStepResult(Request, FModularOnlineResult::Success());

		return false;
	}

	// Asked of the platform, because that is what gates a player: a parental control, an age rating and a
	// multiplayer entitlement belong to the console or store they signed into, not to the match backend.
	const auto Role = EModularOnlineRole::Platform;
	const auto Privileges = Online ? Online->GetInterface<UE::Online::IPrivileges>(Role) : nullptr;
	const auto Privilege = Request->Params.RequestedPrivilege;

	if (!Privileges.IsValid())
	{
		const auto Unasked = AnswerUnaskedPrivilege(Privilege);
		UpdatePrivilege(User, Privilege, Unasked, Role);

		if (Unasked == EModularOnlinePrivilegeResult::Available)
		{
			ApplyStepResult(Request, FModularOnlineResult::Success());
		}
		else
		{
			ApplyStepResult(Request, FModularOnlineResult::FromOnlineError(UE::Online::Errors::NotImplemented()));
		}

		return false;
	}

	const auto AccountId = User->GetAccountId(Role);
	if (!AccountId.IsValid())
	{
		ApplyStepResult(Request, FModularOnlineResult::FromOnlineError(UE::Online::Errors::NotLoggedIn()));

		return false;
	}

	// A project publishing on both roles decides what to publish by this answer, and nothing else asks it.
	// Left unasked it is a cached nothing, and every search narrows itself on a no nobody ever gave.
	if (const auto CrossPlay = GetDefault<UModularCrossPlaySettings>();
		CrossPlay && CrossPlay->CrossPlayPolicy == EModularCrossPlayPolicy::BothRoles && Privilege != EModularOnlinePrivilege::CanUseCrossPlay)
	{
		QueryPrivilege(User, EModularOnlinePrivilege::CanUseCrossPlay, Role);
	}

	UE::Online::FQueryUserPrivilege::Params QueryParams;
	QueryParams.LocalAccountId = AccountId;
	QueryParams.Privilege = FModularPrivilegeConversions::ToOnlineServices(Privilege);

	Request->bStepInFlight = true;

	Privileges->QueryUserPrivilege(MoveTemp(QueryParams)).OnComplete(this, [this, Request, Privilege, Role](const UE::Online::TOnlineResult<UE::Online::FQueryUserPrivilege>& Result)
	{
		const auto CheckedUser = Request->User.Get();
		if (!CheckedUser || Request->bCancelled)
		{
			ActiveLogins.Remove(Request);

			return;
		}

		if (Result.IsError())
		{
			UpdatePrivilege(CheckedUser, Privilege, EModularOnlinePrivilegeResult::PlatformFailure, Role);
			CompleteStep(Request, FModularOnlineResult::FromOnlineError(Result.GetErrorValue()));

			return;
		}

		const auto Answer = FModularPrivilegeConversions::FromOnlineServices(FModularPrivilegeConversions::ToOnlineServices(Privilege), Result.GetOkValue().PrivilegeResult);
		UpdatePrivilege(CheckedUser, Privilege, Answer, Role);

		if (Answer == EModularOnlinePrivilegeResult::Available)
		{
			CompleteStep(Request, FModularOnlineResult::Success());

			return;
		}

		// Strict: whatever the services answered other than "available" ends the login, and the reason
		// travels with it so that a screen can say which one it was.
		auto Failure = FModularOnlineResult::FromOnlineError(UE::Online::Errors::AccessDenied());
		Failure.ErrorId = LexToString(Answer);

		CompleteStep(Request, Failure);
	});

	return true;
}

void UModularUserSubsystem::QueryPrivilege(const UModularUserInfo* User, const EModularOnlinePrivilege Privilege, const EModularOnlineRole Role)
{
	const auto MutableUser = ModifyUser(User);
	const auto Online = GetOnline();
	const auto Privileges = Online ? Online->GetInterface<UE::Online::IPrivileges>(Role) : nullptr;

	if (!MutableUser)
	{
		return;
	}

	if (!Privileges.IsValid())
	{
		UpdatePrivilege(MutableUser, Privilege, AnswerUnaskedPrivilege(Privilege), Role);

		return;
	}

	const auto AccountId = MutableUser->GetAccountId(Role);
	if (!AccountId.IsValid())
	{
		UpdatePrivilege(MutableUser, Privilege, EModularOnlinePrivilegeResult::NotLoggedIn, Role);

		return;
	}

	UE::Online::FQueryUserPrivilege::Params QueryParams;
	QueryParams.LocalAccountId = AccountId;
	QueryParams.Privilege = FModularPrivilegeConversions::ToOnlineServices(Privilege);

	const TWeakObjectPtr<UModularUserInfo> WeakUser { MutableUser };

	Privileges->QueryUserPrivilege(MoveTemp(QueryParams)).OnComplete(this, [this, WeakUser, Privilege, Role](const UE::Online::TOnlineResult<UE::Online::FQueryUserPrivilege>& Result)
	{
		const auto CheckedUser = WeakUser.Get();
		if (!CheckedUser)
		{
			return;
		}

		const auto Answer = Result.IsOk()
			? FModularPrivilegeConversions::FromOnlineServices(FModularPrivilegeConversions::ToOnlineServices(Privilege), Result.GetOkValue().PrivilegeResult)
			: EModularOnlinePrivilegeResult::PlatformFailure;

		UpdatePrivilege(CheckedUser, Privilege, Answer, Role);
	});
}

void UModularUserSubsystem::HandleExternalUIStatusChanged(const UE::Online::FExternalUIStatusChanged& EventParameters)
{
	UE_LOG(LogModularOnline, Log, TEXT("The system overlay is %s."), EventParameters.bIsOpening ? TEXT("opening") : TEXT("closing"));

	OnExternalUIStatusChanged.Broadcast(EventParameters.bIsOpening);
	K2_OnExternalUIStatusChanged.Broadcast(EventParameters.bIsOpening);
}

void UModularUserSubsystem::HandleInputDeviceConnectionChanged(const EInputDeviceConnectionState NewConnectionState, const FPlatformUserId PlatformUser, const FInputDeviceId InputDevice)
{
	const auto bIsConnected = NewConnectionState == EInputDeviceConnectionState::Connected;
	const auto User = ModifyUser(GetUserForInputDevice(InputDevice));

	UE_LOG(LogModularOnline, Log, TEXT("Controller %d of system user %d was %s."),
		InputDevice.GetId(),
		PlatformUser.GetInternalId(),
		bIsConnected ? TEXT("connected") : TEXT("disconnected"));

	if (!User)
	{
		return;
	}

	// What to do about it belongs to the game: a console has to put a "reconnect your controller" screen
	// up, a PC usually does not care. The player layer only says whose controller it was.
	OnUserInputDeviceChanged.Broadcast(User, bIsConnected);
	K2_OnUserInputDeviceChanged.Broadcast(User, bIsConnected);
}

int32 UModularUserSubsystem::FindFreeLocalPlayerIndex() const
{
	for (auto Index = 0; Index < MaxLocalPlayers; ++Index)
	{
		const auto User = GetUserForLocalPlayerIndex(Index);

		if (!User || User->State == EModularUserState::Unknown)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UModularUserSubsystem::ListenForLoginKeys(const TArray<FKey>& AnyUserKeys, const TArray<FKey>& NewUserKeys, const FModularLoginParams& Params)
{
	const auto GameInstance = GetGameInstance();
	const auto ViewportClient = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;

	if (!ViewportClient)
	{
		UE_LOG(LogModularOnline, Warning, TEXT("Nothing to listen on: this game instance has no viewport."));

		return;
	}

	if (AnyUserKeys.IsEmpty() && NewUserKeys.IsEmpty())
	{
		StopListeningForLoginKeys();

		return;
	}

	LoginKeysForAnyUser = AnyUserKeys;
	LoginKeysForNewUser = NewUserKeys;
	LoginKeyParams = Params;

	if (!bInputHandlerInstalled)
	{
		// The test is whether this handler is installed, not whether something was wrapped: a game with no
		// handler of its own would otherwise wrap this one and call itself until the stack ran out.
		bInputHandlerInstalled = true;

		WrappedInputKeyHandler = ViewportClient->OnOverrideInputKey();
		ViewportClient->OnOverrideInputKey().BindUObject(this, &ThisClass::HandleLoginKeyInput);
	}
}

void UModularUserSubsystem::StopListeningForLoginKeys()
{
	LoginKeysForAnyUser.Reset();
	LoginKeysForNewUser.Reset();
	LoginKeyParams = FModularLoginParams { };

	const auto GameInstance = GetGameInstance();

	if (const auto ViewportClient = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		ViewportClient->OnOverrideInputKey() = WrappedInputKeyHandler;
	}

	WrappedInputKeyHandler.Unbind();
	bInputHandlerInstalled = false;
}

bool UModularUserSubsystem::HandleLoginKeyInput(FInputKeyEventArgs& EventArgs)
{
	const auto PassOn = [this, &EventArgs]
	{
		return WrappedInputKeyHandler.IsBound() ? WrappedInputKeyHandler.Execute(EventArgs) : false;
	};

	if (EventArgs.Event != IE_Pressed)
	{
		return PassOn();
	}

	const auto bIsAnyUserKey = LoginKeysForAnyUser.Contains(EventArgs.Key);
	const auto bIsNewUserKey = LoginKeysForNewUser.Contains(EventArgs.Key);

	if (!bIsAnyUserKey && !bIsNewUserKey)
	{
		return PassOn();
	}

	const auto Holder = GetUserForInputDevice(EventArgs.InputDevice);

	if (Holder && Holder->IsLoggingIn())
	{
		// Swallow the key: the player is already being signed in, and a second press would only be
		// refused. Consoles also require that a press start screen stops reacting once it was pressed.
		return true;
	}

	auto Params = LoginKeyParams;
	Params.InputDevice = EventArgs.InputDevice;
	Params.PlatformUser = FPlatformUserId { };

	// Whatever the game asked for the players it invites, the primary one cannot be a guest, and asking
	// for one on their behalf is refused outright rather than falling back to a real sign in.
	Params.bAllowGuest = LoginKeyParams.bAllowGuest && (!Holder || Holder->LocalPlayerIndex != 0);

	if (Holder && !Holder->IsLoggedIn())
	{
		// A player who is there but not signed in: the press start screen of the primary player.
		if (!bIsAnyUserKey)
		{
			return PassOn();
		}

		Params.LocalPlayerIndex = Holder->LocalPlayerIndex;

		return LoginLocalUser(Params);
	}

	if (!Holder)
	{
		// An unclaimed controller. On a platform that maps controllers to accounts this is a second
		// player joining; elsewhere it is usually the same person picking up a gamepad.
		if (!bIsNewUserKey)
		{
			return PassOn();
		}

		const auto FreeIndex = FindFreeLocalPlayerIndex();
		if (FreeIndex == INDEX_NONE)
		{
			UE_LOG(LogModularOnline, Log, TEXT("A controller asked to join, but all %d local players are taken."), MaxLocalPlayers);

			return true;
		}

		Params.LocalPlayerIndex = FreeIndex;
		Params.bAllowGuest = LoginKeyParams.bAllowGuest;

		return LoginLocalUser(Params);
	}

	return PassOn();
}

EModularUserState UModularUserSubsystem::GetUserState(const int32 LocalPlayerIndex) const
{
	const auto User = GetUserForLocalPlayerIndex(LocalPlayerIndex);

	return User ? User->State : EModularUserState::Unknown;
}

FString UModularUserSubsystem::GetUserNickname(const int32 LocalPlayerIndex, const EModularOnlineRole Role) const
{
	const auto User = GetUserForLocalPlayerIndex(LocalPlayerIndex);

	return User ? User->GetNickname(Role) : FString { };
}

FString UModularUserSubsystem::GetUserAvatarUrl(const int32 LocalPlayerIndex, const EModularOnlineRole Role) const
{
	const auto User = GetUserForLocalPlayerIndex(LocalPlayerIndex);

	return User ? User->GetAvatarUrl(Role) : FString { };
}

bool UModularUserSubsystem::IsUserGuest(const int32 LocalPlayerIndex) const
{
	const auto User = GetUserForLocalPlayerIndex(LocalPlayerIndex);

	return User && User->bIsGuest;
}

bool UModularUserSubsystem::IsUserSignedInOnline(const int32 LocalPlayerIndex) const
{
	const auto User = GetUserForLocalPlayerIndex(LocalPlayerIndex);

	return User && User->IsSignedInOnline();
}

EModularOnlinePrivilegeResult UModularUserSubsystem::GetUserPrivilege(const EModularOnlinePrivilege Privilege, const int32 LocalPlayerIndex, const EModularOnlineRole Role) const
{
	const auto User = GetUserForLocalPlayerIndex(LocalPlayerIndex);

	return User ? User->GetPrivilege(Privilege, Role) : EModularOnlinePrivilegeResult::Unknown;
}

void UModularUserSubsystem::K2_QueryPrivilege(const EModularOnlinePrivilege Privilege, const int32 LocalPlayerIndex, const EModularOnlineRole Role)
{
	QueryPrivilege(GetUserForLocalPlayerIndex(LocalPlayerIndex), Privilege, Role);
}
