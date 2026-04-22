#pragma once

#include "Modules/ModuleManager.h"

class FGameplayTagIndexerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
