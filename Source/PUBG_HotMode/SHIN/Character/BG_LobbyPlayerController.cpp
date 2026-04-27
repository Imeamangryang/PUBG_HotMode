#include "BG_LobbyPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/BG_LobbyGameMode.h"

ABG_LobbyPlayerController::ABG_LobbyPlayerController()
{
	bShowMouseCursor = true;

	DefaultMouseCursor = EMouseCursor::Default;
}

void ABG_LobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		ShowLobbyUI();

		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void ABG_LobbyPlayerController::ShowLobbyUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (LobbyWidgetInstance)
	{
		return;
	}

	if (!LobbyWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPlayerController] LobbyWidgetClass is not set."));
		return;
	}

	LobbyWidgetInstance = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
	if (!LobbyWidgetInstance)
	{
		return;
	}

	LobbyWidgetInstance->AddToViewport();
}

void ABG_LobbyPlayerController::RequestStartGame()
{
	Server_RequestStartGame();
	UE_LOG(LogTemp, Warning, TEXT("[LobbyPlayerController] Requesting game start from client."));
}

void ABG_LobbyPlayerController::Server_RequestStartGame_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ABG_LobbyGameMode* LobbyGameMode = World->GetAuthGameMode<ABG_LobbyGameMode>())
	{
		LobbyGameMode->RequestStartGame();
		
		UE_LOG(LogTemp, Warning, TEXT("[LobbyPlayerController] Requesting game start from server."));
	}
}