// Copyright PoFig Games Studio. All Rights Reserved.

#include "Core/ModularOnlineLogChannels.h"
#include "Core/ModularOnlineNetSerializers.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/Serialization/NetSerializerDelegates.h"

namespace PoFigGames::Online::Private
{
	static const FName UniqueNetIdReplStructName { TEXT("UniqueNetIdRepl") };
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(UniqueNetIdReplStructName, FModularUniqueNetIdNetSerializer);


	/**
	 * @class FUniqueNetIdReplRegistryDelegates
	 *
	 * @brief Puts FModularUniqueNetIdNetSerializer in place of the engine's serializer for FUniqueNetIdRepl.
	 *
	 * The registry is emptied when Iris starts and takes registrations only through these delegates. Two infos for one
	 * struct would be picked in an undefined order, so the engine's is taken out rather than shadowed.
	 */
	class FUniqueNetIdReplRegistryDelegates final : public UE::Net::FNetSerializerRegistryDelegates
	{
	public:
		virtual ~FUniqueNetIdReplRegistryDelegates() override
		{
			// Does nothing in a monolithic build, where the registry may already be destroyed at this point.
			UE_NET_UNREGISTER_NETSERIALIZER_INFO(UniqueNetIdReplStructName);
		}

	protected:
		// Post freeze, because by then every loaded module has registered and the engine's info is there to replace
		virtual void OnPostFreezeNetSerializerRegistry() override
		{
			const auto EngineInfo = UE::Net::FPropertyNetSerializerInfoRegistry::FindStructSerializerInfo(UniqueNetIdReplStructName);

			if (EngineInfo)
			{
				UE::Net::FPropertyNetSerializerInfoRegistry::Unregister(EngineInfo);
			}

			UE_NET_REGISTER_NETSERIALIZER_INFO(UniqueNetIdReplStructName);

			UE_CLOG(EngineInfo, LogModularOnline, Log, TEXT("FUniqueNetIdRepl replicates through FModularUniqueNetIdNetSerializer under Iris."));
			UE_CLOG(!EngineInfo, LogModularOnline, Warning,
				TEXT("The engine registered no Iris serializer for FUniqueNetIdRepl to replace; check whether this workaround is still needed."));
		}
	};

	static FUniqueNetIdReplRegistryDelegates UniqueNetIdReplRegistryDelegates { };
}
