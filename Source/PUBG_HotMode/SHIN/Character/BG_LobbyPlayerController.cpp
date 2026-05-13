#include "BG_LobbyPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/BG_LobbyGameMode.h"
#include "GameFramework/BG_GameInstance.h"
#include "GameFramework/BG_GameState.h"
#include "Player/BG_PlayerState.h"
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
		if (IsLobbyMap())
		{
			ShowLobbyUI();

			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
		}
		else
		{
			BG_SHIN_LOG_INFO(TEXT("BeginPlay skipped ShowLobbyUI because current map is not LobbyMap"));
		}
	}

	TrySendPendingNickNameToServer();
}

void ABG_LobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BG_SHIN_LOG_INFO(TEXT("OnRep_PlayerState called"));
	TrySendPendingNickNameToServer();
}

bool ABG_LobbyPlayerController::IsLobbyMap() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString MapName = World->GetMapName();

	return MapName.Contains(TEXT("Safe_House")) || MapName.Contains(TEXT("Lobby"));
}

bool ABG_LobbyPlayerController::CanSendNickNameToServer() const
{
	if (!IsLocalController())
	{
		return false;
	}

	if (bNickNameSentToServer)
	{
		return false;
	}

	if (!PlayerState)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Standalone)
	{
		return false;
	}

	return true;
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
		TEXT("Sending ready request to server"));

	Server_RequestStartGame();
}

void ABG_LobbyPlayerController::Server_RequestStartGame_Implementation()
{
	BG_SHIN_LOG_EVENT_BLOCK(this, "Server_RequestStartGame",
		TEXT("Ready toggle request arrived on server"));

	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("Server_RequestStartGame failed because World was null"));
		return;
	}

	ABG_LobbyGameMode* GM = World->GetAuthGameMode<ABG_LobbyGameMode>();
	if (!GM)
	{
		BG_SHIN_LOG_ERROR(TEXT("GetAuthGameMode<ABG_LobbyGameMode>() returned null"));
		return;
	}

	ABG_PlayerState* BGPlayerState = GetPlayerState<ABG_PlayerState>();
	if (!BGPlayerState)
	{
		BG_SHIN_LOG_ERROR(TEXT("Server_RequestStartGame failed because PlayerState was null"));
		return;
	}

	const bool bNewReadyState = !BGPlayerState->IsReadyToStart();
	BGPlayerState->SetReadyToStart(bNewReadyState);

	BG_SHIN_LOG_INFO(TEXT("Player %s Ready state changed to %s"),
		*GetNameSafe(BGPlayerState),
		bNewReadyState ? TEXT("true") : TEXT("false"));

	if (ABG_GameState* BGGameState = World->GetGameState<ABG_GameState>())
	{
		BGGameState->MarkLobbyPlayerListDirty();
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("Server_RequestStartGame failed because GameState was null"));
	}

	BG_SHIN_LOG_INFO(TEXT("NotifyStartRequested will be called on %s"), *GM->GetName());
	GM->NotifyStartRequested();
}

void ABG_LobbyPlayerController::Client_ShowLoadingScreen_Implementation()
{
	BG_SHIN_LOG_EVENT_BLOCK(this, "Client_ShowLoadingScreen",
		TEXT("Showing loading screen on client"));

	if (LobbyWidgetInstance)
	{
		LobbyWidgetInstance->RemoveFromParent();
		LobbyWidgetInstance = nullptr;
		BG_SHIN_LOG_INFO(TEXT("Lobby widget removed before showing loading screen"));
	}

	if (UBG_GameInstance* GI = GetGameInstance<UBG_GameInstance>())
	{
		GI->StartLoadingScreen();
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetGameInstance<UBG_GameInstance>() returned null"));
	}
}

void ABG_LobbyPlayerController::TrySendPendingNickNameToServer()
{
	if (!CanSendNickNameToServer())
	{
		BG_SHIN_LOG_INFO(TEXT("TrySendPendingNickNameToServer skipped because conditions were not met"));
		return;
	}

	UBG_GameInstance* BGGameInstance = Cast<UBG_GameInstance>(GetGameInstance());
	if (!BGGameInstance)
	{
		BG_SHIN_LOG_ERROR(TEXT("TrySendPendingNickNameToServer failed because GameInstance was not UBG_GameInstance"));
		return;
	}

	const FString PendingNickName = BGGameInstance->GetPendingPlayerNickName();
	if (PendingNickName.IsEmpty())
	{
		BG_SHIN_LOG_WARN(TEXT("TrySendPendingNickNameToServer skipped because PendingPlayerNickName was empty"));
		return;
	}

	BG_SHIN_LOG_INFO(TEXT("Sending PendingPlayerNickName to server: %s"), *PendingNickName);

	ServerSetPlayerNickName(PendingNickName);
	bNickNameSentToServer = true;
}

void ABG_LobbyPlayerController::ServerSetPlayerNickName_Implementation(const FString& InNickName)
{
	ABG_PlayerState* BGPlayerState = GetPlayerState<ABG_PlayerState>();
	if (!BGPlayerState)
	{
		BG_SHIN_LOG_ERROR(TEXT("ServerSetPlayerNickName failed because PlayerState was null"));
		return;
	}

	BGPlayerState->SetPlayerName(InNickName);
	BG_SHIN_LOG_INFO(TEXT("PlayerName set to %s"), *InNickName);

	ABG_GameState* BGGameState = GetWorld() ? GetWorld()->GetGameState<ABG_GameState>() : nullptr;
	if (!BGGameState)
	{
		BG_SHIN_LOG_ERROR(TEXT("ServerSetPlayerNickName failed because GameState was null"));
		return;
	}

	BGGameState->MarkLobbyPlayerListDirty();
}