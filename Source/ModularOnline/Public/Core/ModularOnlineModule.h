// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"


/**
 * @class FModularOnlineModule
 *
 * @brief Runtime module of the plugin.
 */
class FModularOnlineModule : public IModuleInterface
{
public:

#pragma region IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#pragma endregion IModuleInterface

};
