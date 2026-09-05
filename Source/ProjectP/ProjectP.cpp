// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectP.h"
#include "Misc/NetworkVersion.h"
#include "Modules/ModuleManager.h"

namespace
{
	uint32 GetProjectPNetworkVersion()
	{
		return 2026051801;
	}
}

class FProjectPModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		FNetworkVersion::GetLocalNetworkVersionOverride.BindStatic(&GetProjectPNetworkVersion);
	}

	virtual void ShutdownModule() override
	{
		FNetworkVersion::GetLocalNetworkVersionOverride.Unbind();
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FProjectPModule, ProjectP, "ProjectP");
