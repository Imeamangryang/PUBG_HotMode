#include "BG_GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UBG_GameInstance::Init()
{
	Super::Init();
}

void UBG_GameInstance::ConnectToDedicatedServer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FString ServerAddress = TEXT("172.16.30.118:7777");

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->ClientTravel(ServerAddress, TRAVEL_Absolute);
	}
}