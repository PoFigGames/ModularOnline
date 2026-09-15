// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Engine/GameViewportClient.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "User/ModularUserTypes.h"

#include "ModularUserSubsystem.generated.h"

class UModularOnlineSubsystem;
class UModularUserInfo;

namespace UE::Online
{
	struct FAuthLogin;
	struct FAuthLoginStatusChanged;
	struct FAuthQueryExternalAuthToken;
	struct FExternalUIStatusChanged;
	struct FExternalUIShowLoginUI;
	struct FQueryUserPrivilege;

	template <typename OpType>
	class TOnlineResult;
}


/** Answer to one caller who asked for a login. Single cast: it belongs to that caller alone. */
DECLARE_DELEGATE_TwoParams(FModularUserLoginCompleteDelegate, const UModularUserInfo* /*User*/, const FModularOnlineResult& /*Result*/);

/** Events of this layer come in pairs: a native one, and a dynamic twin for Blueprint. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularUserLoginCompleteEvent, const UModularUserInfo* /*User*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularUserLoginCompleteDynamic, const UModularUserInfo*, User, const FModularOnlineResult&, Result);

/** A player changed state: started signing in, signed in, signed out, or was signed out by the platform. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularUserStateChangedEvent, const UModularUserInfo* /*User*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularUserStateChangedDynamic, const UModularUserInfo*, User);

/** An answer about a privilege changed, which is how a screen learns that online play just stopped. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FModularUserPrivilegeChangedEvent, const UModularUserInfo* /*User*/, EModularOnlinePrivilege /*Privilege*/, EModularOnlinePrivilegeResult /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FModularUserPrivilegeChangedDynamic, const UModularUserInfo*, User, EModularOnlinePrivilege, Privilege, EModularOnlinePrivilegeResult, Result);

/** The controller of a local player was unplugged or plugged back in. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularUserInputDeviceChangedEvent, const UModularUserInfo* /*User*/, bool /*bIsConnected*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularUserInputDeviceChangedDynamic, const UModularUserInfo*, User, bool, bIsConnected);

/** The system overlay of the platform opened or closed, which a game is expected to pause for. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularExternalUIStatusEvent, bool /*bIsOpen*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularExternalUIStatusDynamic, bool, bIsOpen);


/**
 * @class UModularUserSubsystem
 *
 * @brief Who is playing locally, and how they signed in.
 */
UCLASS(MinimalAPI, BlueprintType)
class UModularUserSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Called when any login finishes, successfully or not. */
	FModularUserLoginCompleteEvent OnLoginComplete { };

	/** Called whenever a player's state changes, including when the platform signs them out. */
	FModularUserStateChangedEvent OnUserStateChanged { };

	/** Called when an answer about a privilege changes for a player. */
	FModularUserPrivilegeChangedEvent OnPrivilegeChanged { };

	/** Called when a local player's controller is unplugged or comes back. */
	FModularUserInputDeviceChangedEvent OnUserInputDeviceChanged { };

	/** Called when the system overlay opens or closes. */
	FModularExternalUIStatusEvent OnExternalUIStatusChanged { };

	UModularUserSubsystem() { }

#pragma region UGameInstanceSubsystem

	MODULARONLINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	MODULARONLINE_API virtual void Deinitialize() override;

	MODULARONLINE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

#pragma endregion UGameInstanceSubsystem

	/** Signs a local player in, running as many of the steps as this platform needs. */
	MODULARONLINE_API virtual bool LoginLocalUser(const FModularLoginParams& Params, FModularUserLoginCompleteDelegate OnComplete = FModularUserLoginCompleteDelegate { });

	/** Signs a local player out, of the services as well as of the game. */
	MODULARONLINE_API virtual bool LogoutLocalUser(int32 LocalPlayerIndex);

	/** Abandons a login in progress. The player is left in whatever state the finished steps reached. */
	MODULARONLINE_API virtual bool CancelLogin(int32 LocalPlayerIndex);

	/** Asks the services again whether a player may do something, and updates the cached answer. */
	MODULARONLINE_API virtual void QueryPrivilege(const UModularUserInfo* User, EModularOnlinePrivilege Privilege, EModularOnlineRole Role = EModularOnlineRole::Default);

	/** The player at a local index, or null when nobody has signed in there. */
	MODULARONLINE_API const UModularUserInfo* GetUserForLocalPlayerIndex(int32 LocalPlayerIndex) const;

	/** The player behind a system user, ignoring guests, or null. */
	MODULARONLINE_API const UModularUserInfo* GetUserForPlatformUser(FPlatformUserId PlatformUser) const;

	/** The player holding a controller, or null. */
	MODULARONLINE_API const UModularUserInfo* GetUserForInputDevice(FInputDeviceId InputDevice) const;

	/** Every local player known to this subsystem, in no particular order. */
	MODULARONLINE_API TArray<const UModularUserInfo*> GetAllUsers() const;

	/** How many local players this game allows. Four by default, as the engine assumes. */
	MODULARONLINE_API void SetMaxLocalPlayers(int32 InMaxLocalPlayers);

	MODULARONLINE_API int32 GetMaxLocalPlayers() const;

	/** Forgets every player and abandons every login, for a return to the title screen after an error. */
	MODULARONLINE_API virtual void ResetUsers();

	/** Tells the plugin what is true of the platform this build runs on. */
	MODULARONLINE_API void SetTraitTags(const FGameplayTagContainer& InTraits);

	const FGameplayTagContainer& GetTraitTags() const { return TraitTags; }

	bool HasTrait(const FGameplayTag Trait) const { return TraitTags.HasTag(Trait); }

	/** Whether a player has to press something before the game may pick a user for them. */
	MODULARONLINE_API virtual bool ShouldWaitForStartInput() const;

	/**
	 * Watches the viewport for the keys that start a login, which is the press start screen and the way
	 * a second player joins at the same machine.
	 */
	MODULARONLINE_API virtual void ListenForLoginKeys(const TArray<FKey>& AnyUserKeys, const TArray<FKey>& NewUserKeys, const FModularLoginParams& Params);

	/** Stops watching the viewport and restores whatever handler was there before. */
	MODULARONLINE_API virtual void StopListeningForLoginKeys();

	/** True when the backend is a different provider from the platform, which is what makes steps two and three real. */
	MODULARONLINE_API bool HasSeparateServiceProvider() const;

	/** What a step would do for these parameters. Public so that a game can explain the flow to itself. */
	MODULARONLINE_API EModularStepPolicy GetStepPolicy(EModularLoginStep Step, const FModularLoginParams& Params) const;

protected:

	/**
	 * @struct FLoginRequest
	 *
	 * @brief One login in flight, and how far it has got.
	 */
	struct FLoginRequest : TSharedFromThis<FLoginRequest>
	{
		/** The player being signed in. Weak: a login outliving its player is abandoned, not crashed. */
		TWeakObjectPtr<UModularUserInfo> User { };

		/** What the caller asked for. */
		FModularLoginParams Params { };

		/** Who to tell when it is over. */
		FModularUserLoginCompleteDelegate OnComplete { };

		/** The step about to run, or Finished. */
		EModularLoginStep Step { EModularLoginStep::PlatformLogin };

		/** True while a service is working on the current step. */
		bool bStepInFlight { false };

		/** True once the platform identity was carried to the backend, which changes what step three does. */
		bool bPlatformAuthCarried { false };

		/** True when the backend was signed in to, which is what separates online from local play. */
		bool bSignedInOnService { false };

		/** Set when a required step failed; the login ends with this. */
		TOptional<FModularOnlineResult> Failure { };

		/** Which step the failure above came from, which the current step no longer says once it moves on. */
		EModularLoginStep FailedStep { EModularLoginStep::Finished };

		/** Set when nobody is waiting for this login any more, so that a late answer changes nothing. */
		bool bCancelled { false };
	};

	/** Logins in flight. */
	TArray<TSharedRef<FLoginRequest>> ActiveLogins { };

	/** What the game said is true of this platform. */
	FGameplayTagContainer TraitTags { };

	/** Upper bound on local players. */
	int32 MaxLocalPlayers { 4 };

	/** True on a dedicated server, which has local players of no kind. */
	bool bIsDedicatedServer { false };

	/** Subscriptions to the login status of each role. */
	TMap<EModularOnlineRole, UE::Online::FOnlineEventDelegateHandle> LoginStatusHandles { };

	/** Subscriptions to the system overlay of each role. */
	TMap<EModularOnlineRole, UE::Online::FOnlineEventDelegateHandle> ExternalUIHandles { };

	/** The services instance the subscriptions above were taken from. */
	FName BoundToInstance { };

	/** Whether the instance name above has been read at all, as opposed to being empty because it is. */
	bool bInstanceKnown { false };

	/** Handler the viewport had before this subsystem took it, restored when it gives it back. */
	FOverrideInputKeyHandler WrappedInputKeyHandler { };

	/** Whether this handler is on the viewport, which is not the same as having wrapped one of the game's. */
	bool bInputHandlerInstalled { false };

	/** Keys that sign in whoever pressed them. */
	TArray<FKey> LoginKeysForAnyUser { };

	/** Keys that sign in a new local player. */
	TArray<FKey> LoginKeysForNewUser { };

	/** What a key driven login asks for. */
	FModularLoginParams LoginKeyParams { };

	/** The same events, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|User", meta = (DisplayName = "On Login Complete"))
	FModularUserLoginCompleteDynamic K2_OnLoginComplete { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|User", meta = (DisplayName = "On User State Changed"))
	FModularUserStateChangedDynamic K2_OnUserStateChanged { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|User", meta = (DisplayName = "On Privilege Changed"))
	FModularUserPrivilegeChangedDynamic K2_OnPrivilegeChanged { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|User", meta = (DisplayName = "On Input Device Changed"))
	FModularUserInputDeviceChangedDynamic K2_OnUserInputDeviceChanged { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|User", meta = (DisplayName = "On External UI Status Changed"))
	FModularExternalUIStatusDynamic K2_OnExternalUIStatusChanged { };

	/** Local players, by their index in the game instance. */
	UPROPERTY()
	TMap<int32, TObjectPtr<UModularUserInfo>> Users { };

	/** Runs steps until one of them goes asynchronous or the login ends. */
	MODULARONLINE_API void AdvanceLogin(const TSharedRef<FLoginRequest>& Request);

	/** The steps themselves. Each returns true when it started something and will call back. */
	MODULARONLINE_API bool RunPlatformLogin(const TSharedRef<FLoginRequest>& Request);

	MODULARONLINE_API bool RunTransferAuth(const TSharedRef<FLoginRequest>& Request);

	MODULARONLINE_API bool RunServiceLogin(const TSharedRef<FLoginRequest>& Request);

	MODULARONLINE_API bool RunPrivilegeCheck(const TSharedRef<FLoginRequest>& Request);

	/** Ends a login: updates the player, answers the caller, drops the request. */
	MODULARONLINE_API void FinishLogin(const TSharedRef<FLoginRequest>& Request);

	/** Records how a step ended and moves to the next one, failing the login when a required step failed. */
	MODULARONLINE_API void ApplyStepResult(const TSharedRef<FLoginRequest>& Request, const FModularOnlineResult& Result) const;

	/** The same, for a step that answered asynchronously: records the result and carries the login on. */
	MODULARONLINE_API void CompleteStep(const TSharedRef<FLoginRequest>& Request, const FModularOnlineResult& Result);

	/** Puts the login screen of the platform in front of the player. Returns true when one appeared. */
	MODULARONLINE_API bool StartLoginUI(const TSharedRef<FLoginRequest>& Request, EModularOnlineRole Role);

	/** The role a backend login and the privilege check are addressed to. */
	MODULARONLINE_API EModularOnlineRole GetServiceRole() const;

	/** Whether this player may fall back to being a guest after the platform refused them. */
	MODULARONLINE_API bool CanBecomeGuest(const TSharedRef<FLoginRequest>& Request) const;

	/** Signs in to one role with whatever credentials the services accept by default. */
	MODULARONLINE_API bool StartAutoLogin(const TSharedRef<FLoginRequest>& Request, EModularOnlineRole Role);

	/** Reads account, nickname and avatar of a player from a role and caches them. */
	MODULARONLINE_API void RefreshRoleData(UModularUserInfo* User, EModularOnlineRole Role);

	/** The answer when the provider has no privileges component: a refusal on a platform that gates, a grant elsewhere. */
	MODULARONLINE_API EModularOnlinePrivilegeResult AnswerUnaskedPrivilege(EModularOnlinePrivilege Privilege) const;

	/** Writes a privilege answer into the player and tells anyone watching when it changed. */
	MODULARONLINE_API void UpdatePrivilege(UModularUserInfo* User, EModularOnlinePrivilege Privilege, EModularOnlinePrivilegeResult Result, EModularOnlineRole Role);

	/** Moves a player to a new state and broadcasts it. */
	MODULARONLINE_API void SetUserState(UModularUserInfo* User, EModularUserState NewState);

	/** Creates the record for a local index, or returns the existing one. */
	MODULARONLINE_API UModularUserInfo* FindOrCreateUser(int32 LocalPlayerIndex);

	/** Deconst helper for the const getters above. */
	UModularUserInfo* ModifyUser(const UModularUserInfo* User) const { return const_cast<UModularUserInfo*>(User); }

	/** The online subsystem of this game instance, which owns the contexts. */
	MODULARONLINE_API UModularOnlineSubsystem* GetOnline() const;

	/** Subscribes to the login status of every distinct role, so that a sign out reaches the game. */
	MODULARONLINE_API void BindServiceEvents();

	/** The platform signed somebody in or out behind our back. */
	MODULARONLINE_API void HandleLoginStatusChanged(const UE::Online::FAuthLoginStatusChanged& EventParameters, EModularOnlineRole Role);

	/** The system overlay opened or closed. */
	MODULARONLINE_API void HandleExternalUIStatusChanged(const UE::Online::FExternalUIStatusChanged& EventParameters);

	/** A controller was unplugged or plugged in. */
	MODULARONLINE_API void HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUser, FInputDeviceId InputDevice);

	/** The viewport handler installed by ListenForLoginKeys. */
	MODULARONLINE_API bool HandleLoginKeyInput(FInputKeyEventArgs& EventArgs);

	/** The next local index nobody has signed in at, or INDEX_NONE when the game is full. */
	MODULARONLINE_API int32 FindFreeLocalPlayerIndex() const;

	/** What Blueprint may ask of this subsystem. */
	/** How far a local player got through signing in. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|User")
	MODULARONLINE_API EModularUserState GetUserState(int32 LocalPlayerIndex = 0) const;

	/** Display name of a local player on a role, empty while nobody is signed in there. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|User")
	MODULARONLINE_API FString GetUserNickname(int32 LocalPlayerIndex = 0, EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Avatar of a local player on a role, empty where the services publish none. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|User")
	MODULARONLINE_API FString GetUserAvatarUrl(int32 LocalPlayerIndex = 0, EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** True when this player has no account of their own and plays as a guest of the primary one. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|User")
	MODULARONLINE_API bool IsUserGuest(int32 LocalPlayerIndex = 0) const;

	/** True when this player has an account with the online services, which a guest does not. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|User")
	MODULARONLINE_API bool IsUserSignedInOnline(int32 LocalPlayerIndex = 0) const;

	/** The last answer about a privilege, Unknown while it was never asked for. */
	UFUNCTION(BlueprintPure, Category = "ModularOnline|User")
	MODULARONLINE_API EModularOnlinePrivilegeResult GetUserPrivilege(EModularOnlinePrivilege Privilege, int32 LocalPlayerIndex = 0, EModularOnlineRole Role = EModularOnlineRole::Default) const;

	/** Asks the services again whether a player may do something; the answer arrives as an event. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|User", meta = (DisplayName = "Query User Privilege"))
	MODULARONLINE_API void K2_QueryPrivilege(EModularOnlinePrivilege Privilege, int32 LocalPlayerIndex = 0, EModularOnlineRole Role = EModularOnlineRole::Default);

	/** Signs a local player out, for a graph that cannot pass a completion delegate. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|User", meta = (DisplayName = "Log Out Local User"))
	MODULARONLINE_API bool K2_LogoutLocalUser(int32 LocalPlayerIndex) { return LogoutLocalUser(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|User", meta = (DisplayName = "Should Wait For Start Input"))
	bool K2_ShouldWaitForStartInput() const { return ShouldWaitForStartInput(); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|User", meta = (DisplayName = "Set Trait Tags"))
	void K2_SetTraitTags(const FGameplayTagContainer& InTraits) { SetTraitTags(InTraits); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|User", meta = (DisplayName = "Stop Listening For Login Keys"))
	void K2_StopListeningForLoginKeys() { StopListeningForLoginKeys(); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|User", meta = (DisplayName = "Listen For Login Keys", AutoCreateRefTerm = "AnyUserKeys,NewUserKeys"))
	void K2_ListenForLoginKeys(const TArray<FKey>& AnyUserKeys, const TArray<FKey>& NewUserKeys, const FModularLoginParams& Params) { ListenForLoginKeys(AnyUserKeys, NewUserKeys, Params); }
};
