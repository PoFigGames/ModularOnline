// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Core/ModularOnlineTypes.h"
#include "Online/Auth.h"
#include "Online/CoreOnline.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "User/ModularUserTypes.h"

#include "ModularServerSubsystem.generated.h"

class UModularOnlineSubsystem;


/** How a server login ends. */
DECLARE_DELEGATE_OneParam(FModularServerLoginDelegate, const FModularOnlineResult& /*Result*/);

/** The server signed in, or failed to. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularServerLoginEvent, const FModularOnlineResult& /*Result*/);


/**
 * @class UModularServerSubsystem
 *
 * @brief The side of the online layer that belongs to whoever is hosting.
 */
UCLASS(MinimalAPI)
class UModularServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when a server login finishes, successfully or not. */
	FModularServerLoginEvent OnServerLoginComplete { };

#pragma region UGameInstanceSubsystem

	MODULARONLINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	MODULARONLINE_API virtual void Deinitialize() override;
	MODULARONLINE_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

#pragma endregion UGameInstanceSubsystem

	/** Signs this machine in to the services as a game server. */
	MODULARONLINE_API virtual bool LoginServer(FModularServerLoginDelegate OnComplete = FModularServerLoginDelegate { });

	/** True once the services accepted this machine. */
	MODULARONLINE_API bool IsServerLoggedIn() const;

	/** The account the services gave this machine, invalid while it is not signed in. */
	MODULARONLINE_API UE::Online::FAccountId GetServerAccountId() const;

protected:
	/** The online layer, which owns the contexts. */
	MODULARONLINE_API UModularOnlineSubsystem* GetOnline() const;

	/** The role a server publishes its match on, which is also the one it signs in to. */
	MODULARONLINE_API EModularOnlineRole GetServerRole() const;

	/** The account of this machine, empty until it signs in. */
	UE::Online::FAccountId ServerAccount { };

	/** True on a machine with no local player at all. */
	bool bIsDedicatedServer { false };
};
