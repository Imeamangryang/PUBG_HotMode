#include "BG_GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/BG_LogHelper.h"

void UBG_GameInstance::Init()
{
	Super::Init();

	BG_SHIN_LOG_EVENT_BLOCK(this, "GameInstance Init",
		TEXT("GameInstance initialized"));

	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UBG_GameInstance::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UBG_GameInstance::HandlePostLoadMap);
}

void UBG_GameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	HideLoadingScreen();

	Super::Shutdown();
}

void UBG_GameInstance::HandlePreLoadMap(const FString& MapName)
{
	BG_SHIN_LOG_INFO(TEXT("PreLoadMap: %s"), *MapName);
	ShowLoadingScreen();
}

void UBG_GameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	BG_SHIN_LOG_INFO(TEXT("PostLoadMapWithWorld: %s"),
		LoadedWorld ? *LoadedWorld->GetMapName() : TEXT("null"));

	HideLoadingScreen();
}

void UBG_GameInstance::StartLoadingScreen()
{
	ShowLoadingScreen();
}

void UBG_GameInstance::StopLoadingScreen()
{
	HideLoadingScreen();
}

void UBG_GameInstance::ShowLoadingScreen()
{
	if (LoadingWidgetInstance)
	{
		BG_SHIN_LOG_WARN(TEXT("ShowLoadingScreen skipped because LoadingWidgetInstance already exists"));
		return;
	}

	if (!LoadingWidgetClass)
	{
		BG_SHIN_LOG_WARN(TEXT("LoadingWidgetClass is not set"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("ShowLoadingScreen failed because World was null"));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		BG_SHIN_LOG_WARN(TEXT("ShowLoadingScreen failed because PlayerController was null"));
		return;
	}

	LoadingWidgetInstance = CreateWidget<UUserWidget>(PC, LoadingWidgetClass);
	if (!LoadingWidgetInstance)
	{
		BG_SHIN_LOG_ERROR(TEXT("CreateWidget failed for LoadingWidgetClass=%s"), *BGLogHelper::SafeClass(LoadingWidgetClass));
		return;
	}

	LoadingWidgetInstance->AddToViewport(10000);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	BG_SHIN_LOG_INFO(TEXT("Loading screen shown"));
}

void UBG_GameInstance::HideLoadingScreen()
{
	if (!LoadingWidgetInstance)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}

	LoadingWidgetInstance->RemoveFromParent();
	LoadingWidgetInstance = nullptr;

	BG_SHIN_LOG_INFO(TEXT("Loading screen hidden"));
}

void UBG_GameInstance::ConnectToDedicatedServer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("ConnectToDedicatedServer failed because World was null"));
		return;
	}

	BG_SHIN_LOG_EVENT_BLOCK(this, "ConnectToDedicatedServer",
		TEXT("Attempting to connect to dedicated server"));

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		BG_SHIN_LOG_WARN(TEXT("Skipped because current net mode is already DedicatedServer"));
		return;
	}

	// const FString ServerAddress = TEXT("172.16.30.118:7777");
	const FString ServerAddress = TEXT("127.0.0.1:7777");
	BG_SHIN_LOG_INFO(TEXT("ServerAddress = %s"), *ServerAddress);

	ShowLoadingScreen();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		BG_SHIN_LOG_CONTROLLER(this, PC, "Before ClientTravel");
		PC->ClientTravel(ServerAddress, TRAVEL_Absolute);
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetPlayerController(World, 0) returned null"));
		HideLoadingScreen();
	}
}