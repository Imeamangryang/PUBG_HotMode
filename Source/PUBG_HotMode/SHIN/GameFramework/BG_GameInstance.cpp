#include "BG_GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Utils/BG_LogHelper.h"

void UBG_GameInstance::Init()
{
	Super::Init();

	BG_SHIN_LOG_EVENT_BLOCK(this, "GameInstance Init",
		TEXT("GameInstance initialized"));
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

	const FString ServerAddress = TEXT("172.16.30.118:7777");
	BG_SHIN_LOG_INFO(TEXT("ServerAddress = %s"), *ServerAddress);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		BG_SHIN_LOG_CONTROLLER(this, PC, "Before ClientTravel");
		PC->ClientTravel(ServerAddress, TRAVEL_Absolute);
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetPlayerController(World, 0) returned null"));
	}
}