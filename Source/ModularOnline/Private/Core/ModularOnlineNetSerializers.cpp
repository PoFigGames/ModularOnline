// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineNetSerializers.h"

#include "Core/ModularOnlineLogChannels.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetErrorContext.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Online/CoreOnline.h"

namespace PoFigGames::Online::Private
{
	/**
	 * @struct FUniqueNetIdQuantizedType
	 *
	 * @brief An account id as Iris keeps it: its provider and the bytes that name it; no bytes is no id.
	 */
	struct FUniqueNetIdQuantizedType
	{
		// Sixteen inline bytes hold a Steam id and most others without an allocation.
		typedef UE::Net::FNetSerializerArrayStorage<uint8, UE::Net::AllocationPolicies::TInlinedElementAllocationPolicy<16>> FDataStorage;

		FDataStorage Data { };
		uint8 OnlineServices { 0 };
	};
}

template <> struct TIsPODType<PoFigGames::Online::Private::FUniqueNetIdQuantizedType> { enum { Value = true }; };

namespace PoFigGames::Online::Private
{
	constexpr uint32 OnlineServicesBits { 8 };
	constexpr uint32 LengthBits { 8 };
	constexpr int32 MaxDataBytes { (1 << LengthBits) - 1 };


	/**
	 * @struct FModularUniqueNetIdNetSerializer
	 *
	 * @brief Replicates FUniqueNetIdRepl under Iris for Online Services account ids.
	 *
	 * The engine's serializer for the struct reads every id through GetV1() and dereferences null on an account id
	 * (Engine/Private/GameFramework/UniqueNetIdReplNetSerializer.cpp:255 and :431; the same in ue5-main and ue6-main on
	 * 2026-09-24). An account id travels as its provider and that provider's replication data, and is accepted only if
	 * a provider here reads it and writes it back byte for byte. Online Subsystem ids are not replicated. To be dropped
	 * the day the engine serializer learns account ids.
	 */
	struct FModularUniqueNetIdNetSerializer
	{
		static constexpr uint32 Version { 0 };
		static constexpr bool bHasDynamicState { true };

		typedef FUniqueNetIdRepl SourceType;
		typedef FUniqueNetIdQuantizedType QuantizedType;
		typedef FNetSerializerConfig ConfigType;

		inline static const ConfigType DefaultConfig { };

		static void Serialize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetSerializeArgs& Args);
		static void Deserialize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetDeserializeArgs& Args);
		static void Quantize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetQuantizeArgs& Args);
		static void Dequantize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetDequantizeArgs& Args);
		static bool IsEqual(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetIsEqualArgs& Args);
		static bool Validate(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetValidateArgs& Args);
		static void CloneDynamicState(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetCloneDynamicStateArgs& Args);
		static void FreeDynamicState(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetFreeDynamicStateArgs& Args);

	private:
		/** The provider of an id and the bytes that name it; false for an id the wire does not carry. */
		static bool EncodeId(const SourceType& Id, uint8& OutOnlineServices, TArray<uint8>& OutData);

		static bool IsSameQuantized(const QuantizedType& Value0, const QuantizedType& Value1);

		/** Whether bytes from a peer name a real id: one a provider here can read, written exactly as it would write it. */
		static bool IsGenuine(uint8 OnlineServices, const TArray<uint8>& Data);
	};

	UE_NET_IMPLEMENT_SERIALIZER(FModularUniqueNetIdNetSerializer);

	bool FModularUniqueNetIdNetSerializer::EncodeId(const SourceType& Id, uint8& OutOnlineServices, TArray<uint8>& OutData)
	{
		OutOnlineServices = 0;
		OutData.Reset();

		auto bCarried = !Id.IsValid();

		if (Id.IsValid() && Id.IsV2())
		{
			const auto AccountId = Id.GetV2();

			OutOnlineServices = static_cast<uint8>(AccountId.GetOnlineServicesType());
			OutData = UE::Online::FOnlineIdRegistryRegistry::Get().ToReplicationData(AccountId);
			bCarried = !OutData.IsEmpty() && OutData.Num() <= MaxDataBytes;
		}

		return bCarried;
	}

	bool FModularUniqueNetIdNetSerializer::IsSameQuantized(const QuantizedType& Value0, const QuantizedType& Value1)
	{
		return Value0.OnlineServices == Value1.OnlineServices
			&& Value0.Data.Num() == Value1.Data.Num()
			&& FMemory::Memcmp(Value0.Data.GetData(), Value1.Data.GetData(), Value0.Data.Num()) == 0;
	}

	bool FModularUniqueNetIdNetSerializer::IsGenuine(const uint8 OnlineServices, const TArray<uint8>& Data)
	{
		// Only a provider this machine runs can tell whether the bytes name somebody; left to the engine, a provider it
		// does not know keeps them unread as a foreign id.
		const auto& Registries = UE::Online::FOnlineIdRegistryRegistry::Get();
		const auto Registry = Registries.GetAccountIdRegistry(static_cast<UE::Online::EOnlineServices>(OnlineServices));
		const auto AccountId = Registry ? Registry->FromReplicationData(Data) : UE::Online::FAccountId { };

		// Written back, a genuine id is the same bytes; whatever the provider read past or around is not.
		return AccountId.IsValid() && Registries.ToReplicationData(AccountId) == Data;
	}

	void FModularUniqueNetIdNetSerializer::Serialize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetSerializeArgs& Args)
	{
		const auto& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const auto Writer = Context.GetBitStreamWriter();

		if (Writer->WriteBool(Value.Data.Num() > 0))
		{
			Writer->WriteBits(Value.OnlineServices, OnlineServicesBits);
			Writer->WriteBits(Value.Data.Num(), LengthBits);

			for (const auto Byte : MakeArrayView(Value.Data.GetData(), Value.Data.Num()))
			{
				Writer->WriteBits(Byte, 8);
			}
		}
	}

	void FModularUniqueNetIdNetSerializer::Deserialize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetDeserializeArgs& Args)
	{
		auto& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const auto Reader = Context.GetBitStreamReader();

		const auto bHasId = Reader->ReadBool();
		const auto OnlineServices = bHasId ? Reader->ReadBits(OnlineServicesBits) : 0U;
		const auto Length = bHasId ? Reader->ReadBits(LengthBits) : 0U;

		TArray<uint8> Data;
		Data.SetNumUninitialized(static_cast<int32>(Length));

		for (auto& Byte : Data)
		{
			Byte = static_cast<uint8>(Reader->ReadBits(8));
		}

		// Taken for an id only once shown to be one: a peer that sends anything else is sending a broken stream.
		if (!Context.HasErrorOrOverflow() && (!bHasId || IsGenuine(static_cast<uint8>(OnlineServices), Data)))
		{
			Target.OnlineServices = static_cast<uint8>(OnlineServices);
			Target.Data.AdjustSize(Context, Data.Num());
			FMemory::Memcpy(Target.Data.GetData(), Data.GetData(), Data.Num());
		}
		else
		{
			Context.SetError(UE::Net::GNetError_InvalidValue);
		}
	}

	void FModularUniqueNetIdNetSerializer::Quantize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetQuantizeArgs& Args)
	{
		const auto& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		auto& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

		uint8 OnlineServices;
		TArray<uint8> Data;

		if (EncodeId(Source, OnlineServices, Data))
		{
			Target.OnlineServices = OnlineServices;
			Target.Data.AdjustSize(Context, Data.Num());
			FMemory::Memcpy(Target.Data.GetData(), Data.GetData(), Data.Num());
		}
		else
		{
			UE_LOG(LogModularOnline, Error,
				TEXT("An online id was not sent: Iris carries only Online Services account ids whose replication data is 1 to %d bytes long."),
				MaxDataBytes);

			Context.SetError(UE::Net::GNetError_InvalidValue);
		}
	}

	void FModularUniqueNetIdNetSerializer::Dequantize(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetDequantizeArgs& Args)
	{
		const auto& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		auto& Target = *reinterpret_cast<SourceType*>(Args.Target);

		const TArray Data { Source.Data.GetData(), static_cast<int32>(Source.Data.Num()) };
		const auto Services = static_cast<UE::Online::EOnlineServices>(Source.OnlineServices);
		const auto AccountId = Data.IsEmpty() ? UE::Online::FAccountId { } : UE::Online::FOnlineIdRegistryRegistry::Get().ToAccountId(Services, Data);

		Target = AccountId.IsValid() ? FUniqueNetIdRepl { AccountId } : FUniqueNetIdRepl { };
	}

	bool FModularUniqueNetIdNetSerializer::IsEqual(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetIsEqualArgs& Args)
	{
		return Args.bStateIsQuantized
			? IsSameQuantized(*reinterpret_cast<const QuantizedType*>(Args.Source0), *reinterpret_cast<const QuantizedType*>(Args.Source1))
			: *reinterpret_cast<const SourceType*>(Args.Source0) == *reinterpret_cast<const SourceType*>(Args.Source1);
	}

	bool FModularUniqueNetIdNetSerializer::Validate(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetValidateArgs& Args)
	{
		uint8 OnlineServices;
		TArray<uint8> Data;

		return EncodeId(*reinterpret_cast<const SourceType*>(Args.Source), OnlineServices, Data);
	}

	void FModularUniqueNetIdNetSerializer::CloneDynamicState(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetCloneDynamicStateArgs& Args)
	{
		const auto& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		auto& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

		Target.Data.Clone(Context, Source.Data);
	}

	void FModularUniqueNetIdNetSerializer::FreeDynamicState(UE::Net::FNetSerializationContext& Context, const UE::Net::FNetFreeDynamicStateArgs& Args)
	{
		auto& Value = *reinterpret_cast<QuantizedType*>(Args.Source);

		Value.Data.Free(Context);
		Value.OnlineServices = 0;
	}
}
