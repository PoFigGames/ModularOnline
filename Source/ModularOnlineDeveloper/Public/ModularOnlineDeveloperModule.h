// Copyright PoFig Games Studio. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"


/**
 * @class FModularOnlineDeveloperModule
 *
 * @brief Developer tooling of the plugin: console commands and diagnostics.
 */
class FModularOnlineDeveloperModule : public IModuleInterface
{
public:

#pragma region IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#pragma endregion IModuleInterface

};
