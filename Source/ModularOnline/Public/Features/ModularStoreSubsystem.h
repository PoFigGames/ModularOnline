// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Features/ModularFeatureSubsystem.h"
#include "Features/ModularFeatureTypes.h"
#include "Online/Commerce.h"
#include "Online/OnlineAsyncOpHandle.h"

#include "ModularStoreSubsystem.generated.h"


/** How a request about the catalogue ends. */
DECLARE_DELEGATE_TwoParams(FModularStoreOffersDelegate, const TArray<FModularStoreOffer>& /*Offers*/, const FModularOnlineResult& /*Result*/);

/** How a request about what the account owns ends. */
DECLARE_DELEGATE_TwoParams(FModularEntitlementsDelegate, const TArray<FModularEntitlement>& /*Entitlements*/, const FModularOnlineResult& /*Result*/);

/** How a checkout ends, with the transaction the store gave it when it gave one. */
DECLARE_DELEGATE_TwoParams(FModularCheckoutDelegate, const FString& /*TransactionId*/, const FModularOnlineResult& /*Result*/);

/** How redeeming an entitlement ends. */
DECLARE_DELEGATE_OneParam(FModularStoreOperationDelegate, const FModularOnlineResult& /*Result*/);

/** A purchase went through, whoever started it. */
/** The answer to a query this subsystem was asked for; what it fetched is in the cache. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularOffersQueriedEvent, const TArray<FModularStoreOffer>& /*Offers*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularOffersQueriedDynamic, const TArray<FModularStoreOffer>&, Offers, const FModularOnlineResult&, Result);

DECLARE_MULTICAST_DELEGATE_TwoParams(FModularEntitlementsQueriedEvent, const TArray<FModularEntitlement>& /*Entitlements*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularEntitlementsQueriedDynamic, const TArray<FModularEntitlement>&, Entitlements, const FModularOnlineResult&, Result);

/** The answer to a checkout this subsystem started, which a purchase made elsewhere is not. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularCheckoutAnsweredEvent, const FString& /*TransactionId*/, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularCheckoutAnsweredDynamic, const FString&, TransactionId, const FModularOnlineResult&, Result);

/** An entitlement was redeemed, or was not. */
DECLARE_MULTICAST_DELEGATE_OneParam(FModularRedeemAnsweredEvent, const FModularOnlineResult& /*Result*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularRedeemAnsweredDynamic, const FModularOnlineResult&, Result);

DECLARE_MULTICAST_DELEGATE_TwoParams(FModularPurchaseCompletedEvent, const FModularAccountHandle& /*AccountId*/, const FString& /*TransactionId*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularPurchaseCompletedDynamic, const FModularAccountHandle&, AccountId, const FString&, TransactionId);


/**
 * @class UModularStoreSubsystem
 *
 * @brief The storefront of the platform: what it sells, what the account already owns, and buying.
 */
UCLASS(MinimalAPI)
class UModularStoreSubsystem : public UModularFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Fired when a purchase completes, including one this game never started. */
	FModularPurchaseCompletedEvent OnPurchaseCompleted { };

/** Fired when a query for offers finishes. */
	FModularOffersQueriedEvent OnOffersQueried { };

	/** Fired when a query for entitlements finishes. */
	FModularEntitlementsQueriedEvent OnEntitlementsQueried { };

	/** Fired when a checkout this subsystem started finishes. */
	FModularCheckoutAnsweredEvent OnCheckoutAnswered { };

	/** Fired when a redemption finishes. */
	FModularRedeemAnsweredEvent OnRedeemAnswered { };

	#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual void Deinitialize() override;
	MODULARONLINE_API virtual FGameplayTag GetFeatureTag() const override;

#pragma endregion UModularFeatureSubsystem

	/** Asks the store for its catalogue; an empty list of ids asks for everything it sells. */
	MODULARONLINE_API virtual bool QueryOffers(int32 LocalPlayerIndex, const TArray<FString>& OfferIds, FModularStoreOffersDelegate OnComplete = FModularStoreOffersDelegate { });

	/** What the last query answered, without asking again. */
	MODULARONLINE_API bool GetOffers(int32 LocalPlayerIndex, TArray<FModularStoreOffer>& OutOffers) const;

	/** Starts the store's own checkout for one or more offers, which is where the player pays. */
	MODULARONLINE_API virtual bool Checkout(int32 LocalPlayerIndex, const TArray<FModularPurchaseLine>& Lines, FModularCheckoutDelegate OnComplete = FModularCheckoutDelegate { });

	/** Asks the store what the account owns. */
	MODULARONLINE_API virtual bool QueryEntitlements(int32 LocalPlayerIndex, bool bIncludeRedeemed, FModularEntitlementsDelegate OnComplete = FModularEntitlementsDelegate { });

	/** What the last such query answered, without asking again. */
	MODULARONLINE_API bool GetEntitlements(int32 LocalPlayerIndex, TArray<FModularEntitlement>& OutEntitlements) const;

	/** Consumes an entitlement, for stores that leave redeeming to the client. */
	MODULARONLINE_API virtual bool RedeemEntitlement(int32 LocalPlayerIndex, const FString& EntitlementId, int32 Quantity, FModularStoreOperationDelegate OnComplete = FModularStoreOperationDelegate { });

	/** Opens the store of the platform over the game. */
	MODULARONLINE_API virtual bool ShowStoreUI(int32 LocalPlayerIndex);

protected:
	/** The same events, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Store", meta = (DisplayName = "On Offers Queried"))
	FModularOffersQueriedDynamic K2_OnOffersQueried { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Store", meta = (DisplayName = "On Entitlements Queried"))
	FModularEntitlementsQueriedDynamic K2_OnEntitlementsQueried { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Store", meta = (DisplayName = "On Checkout Answered"))
	FModularCheckoutAnsweredDynamic K2_OnCheckoutAnswered { };

	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Store", meta = (DisplayName = "On Redeem Answered"))
	FModularRedeemAnsweredDynamic K2_OnRedeemAnswered { };

	/** Each answers on the caller's delegate and on both events. */
	MODULARONLINE_API void AnnounceOffers(const TArray<FModularStoreOffer>& Offers, const FModularOnlineResult& Result, const FModularStoreOffersDelegate& OnComplete);

	MODULARONLINE_API void AnnounceEntitlements(const TArray<FModularEntitlement>& Entitlements, const FModularOnlineResult& Result, const FModularEntitlementsDelegate& OnComplete);

	MODULARONLINE_API void AnnounceCheckout(const FString& TransactionId, const FModularOnlineResult& Result, const FModularCheckoutDelegate& OnComplete);

	MODULARONLINE_API void AnnounceRedeemed(const FModularOnlineResult& Result, const FModularStoreOperationDelegate& OnComplete);
	/** The same event, for Blueprint. */
	UPROPERTY(BlueprintAssignable, Category = "ModularOnline|Store", meta = (DisplayName = "On Purchase Completed"))
	FModularPurchaseCompletedDynamic K2_OnPurchaseCompleted { };

	/** Subscription to what the store says about purchases. */
	UE::Online::FOnlineEventDelegateHandle PurchaseCompletedHandle { };

	/** True once that subscription exists. */
	bool bListeningToPurchases { false };

#pragma region UModularFeatureSubsystem

	MODULARONLINE_API virtual EModularOnlineRole GetFeatureRole() const override;

#pragma endregion UModularFeatureSubsystem

	/** Starts listening, if it has not already. */
	MODULARONLINE_API void EnsureListening();

	/** The store reported a purchase. */
	MODULARONLINE_API void HandlePurchaseCompleted(const UE::Online::FCommerceOnPurchaseComplete& EventParameters);

	/** What Blueprint may ask of this subsystem. The answers arrive on the cached getters and the event. */
	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Store", meta = (DisplayName = "Query Offers", AutoCreateRefTerm = "OfferIds"))
	MODULARONLINE_API bool K2_QueryOffers(const TArray<FString>& OfferIds, int32 LocalPlayerIndex = 0) { return QueryOffers(LocalPlayerIndex, OfferIds); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Store", meta = (DisplayName = "Get Offers"))
	MODULARONLINE_API bool K2_GetOffers(TArray<FModularStoreOffer>& OutOffers, int32 LocalPlayerIndex = 0) const { return GetOffers(LocalPlayerIndex, OutOffers); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Store", meta = (DisplayName = "Checkout", AutoCreateRefTerm = "Lines"))
	MODULARONLINE_API bool K2_Checkout(const TArray<FModularPurchaseLine>& Lines, int32 LocalPlayerIndex = 0) { return Checkout(LocalPlayerIndex, Lines); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Store", meta = (DisplayName = "Query Entitlements"))
	MODULARONLINE_API bool K2_QueryEntitlements(bool bIncludeRedeemed = false, int32 LocalPlayerIndex = 0) { return QueryEntitlements(LocalPlayerIndex, bIncludeRedeemed); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Store", meta = (DisplayName = "Get Entitlements"))
	MODULARONLINE_API bool K2_GetEntitlements(TArray<FModularEntitlement>& OutEntitlements, int32 LocalPlayerIndex = 0) const { return GetEntitlements(LocalPlayerIndex, OutEntitlements); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Store", meta = (DisplayName = "Redeem Entitlement"))
	MODULARONLINE_API bool K2_RedeemEntitlement(const FString& EntitlementId, int32 Quantity = 1, int32 LocalPlayerIndex = 0) { return RedeemEntitlement(LocalPlayerIndex, EntitlementId, Quantity); }

	UFUNCTION(BlueprintCallable, Category = "ModularOnline|Store", meta = (DisplayName = "Show Store UI"))
	MODULARONLINE_API bool K2_ShowStoreUI(int32 LocalPlayerIndex = 0) { return ShowStoreUI(LocalPlayerIndex); }

	UFUNCTION(BlueprintPure, Category = "ModularOnline|Store", meta = (DisplayName = "Is Store Available"))
	MODULARONLINE_API bool K2_IsAvailable() const { return IsAvailable(); }
};
