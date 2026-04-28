#include "BG_LobbyPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/BG_LobbyGameMode.h"
#include "Utils/BG_LogHelper.h"

ABG_LobbyPlayerController::ABG_LobbyPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ABG_LobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	BG_SHIN_LOG_EVENT_BLOCK(this, "LobbyPlayerController BeginPlay",
		TEXT("IsLocalController=%s"),
		IsLocalController() ? TEXT("true") : TEXT("false"));

	BG_SHIN_LOG_CONTROLLER(this, this, "Lobby PC BeginPlay");

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
		BG_SHIN_LOG_WARN(TEXT("ShowLobbyUI skipped because controller is not local"));
		return;
	}

	if (LobbyWidgetInstance)
	{
		BG_SHIN_LOG_WARN(TEXT("ShowLobbyUI skipped because LobbyWidgetInstance already exists"));
		return;
	}

	if (!LobbyWidgetClass)
	{
		BG_SHIN_LOG_ERROR(TEXT("LobbyWidgetClass is not set"));
		return;
	}

	LobbyWidgetInstance = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
	if (!LobbyWidgetInstance)
	{
		BG_SHIN_LOG_ERROR(TEXT("CreateWidget failed for LobbyWidgetClass=%s"), *BGLogHelper::SafeClass(LobbyWidgetClass));
		return;
	}

	BG_SHIN_LOG_INFO(TEXT("Lobby widget created: %s"), *BGLogHelper::SafeName(LobbyWidgetInstance));
	LobbyWidgetInstance->AddToViewport();
	BG_SHIN_LOG_INFO(TEXT("Lobby widget added to viewport"));
}

void ABG_LobbyPlayerController::RequestStartGame()
{
	BG_SHIN_LOG_EVENT_BLOCK(this, "Client RequestStartGame",
		TEXT("Sending Server_RequestStartGame RPC"));

	Server_RequestStartGame();
}

void ABG_LobbyPlayerController::Server_RequestStartGame_Implementation()
{
	BG_SHIN_LOG_EVENT_BLOCK(this, "Server_RequestStartGame",
		TEXT("RPC arrived on server"));

	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("GetWorld() returned null"));
		return;
	}

	if (ABG_LobbyGameMode* GM = World->GetAuthGameMode<ABG_LobbyGameMode>())
	{
		BG_SHIN_LOG_INFO(TEXT("NotifyStartRequested will be called on %s"), *GM->GetName());
		GM->NotifyStartRequested();
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetAuthGameMode<ABG_LobbyGameMode>() returned null"));
	}
}