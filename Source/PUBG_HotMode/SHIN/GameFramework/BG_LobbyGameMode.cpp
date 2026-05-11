#include "BG_LobbyGameMode.h"
#include "BG_GameState.h"
#include "Character/BG_LobbyPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/BG_LogHelper.h"

ABG_LobbyGameMode::ABG_LobbyGameMode()
{
	bUseSeamlessTravel = false;
	GameStateClass = ABG_GameState::StaticClass();
	PlayerControllerClass = ABG_LobbyPlayerController::StaticClass();
}

void ABG_LobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	BG_SHIN_LOG_EVENT_BLOCK(this, "LobbyGameMode BeginPlay",
		TEXT("GameMode=%s PlayerControllerClass=%s"),
		*GetClass()->GetName(),
		*BGLogHelper::SafeClass(PlayerControllerClass));

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		BGGameState->SetMatchState(EBG_MatchState::Lobby);
		BGGameState->MarkLobbyPlayerListDirty();
		BG_SHIN_LOG_INFO(TEXT("MatchState set to Lobby"));
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetGameState<ABG_GameState>() returned null"));
	}
}

void ABG_LobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	BG_SHIN_LOG_EVENT_BLOCK(this, "LobbyGameMode PostLogin",
		TEXT("Player joined lobby"));

	BG_SHIN_LOG_CONTROLLER(this, NewPlayer, "Lobby PostLogin");

	if (!NewPlayer)
	{
		BG_SHIN_LOG_ERROR(TEXT("PostLogin failed because NewPlayer was null"));
		return;
	}

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		BGGameState->MarkLobbyPlayerListDirty();
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("PostLogin could not mark lobby player list dirty because GameState was null"));
	}

	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsWithTag(this, FName(TEXT("LobbyCamera")), FoundCameras);

	BG_SHIN_LOG_INFO(TEXT("Found LobbyCamera count = %d"), FoundCameras.Num());

	if (FoundCameras.Num() > 0)
	{
		BG_SHIN_LOG_ACTOR(this, FoundCameras[0], "SetViewTarget target");
		NewPlayer->SetViewTargetWithBlend(FoundCameras[0], 0.0f);
	}
	else
	{
		BG_SHIN_LOG_WARN(TEXT("No actor with tag LobbyCamera was found"));
	}
}

void ABG_LobbyGameMode::Logout(AController* Exiting)
{
	BG_SHIN_LOG_EVENT_BLOCK(this, "LobbyGameMode Logout",
		TEXT("Player leaving lobby"));

	BG_SHIN_LOG_CONTROLLER(this, Exiting, "Lobby Logout");

	Super::Logout(Exiting);

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		BGGameState->MarkLobbyPlayerListDirty();
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("Logout could not mark lobby player list dirty because GameState was null"));
	}
}

void ABG_LobbyGameMode::RequestStartGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("RequestStartGame failed because World was null"));
		return;
	}

	BG_SHIN_LOG_EVENT_BLOCK(this, "LobbyGameMode RequestStartGame",
		TEXT("Start game requested"));

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		if (BGGameState->GetMatchState() != EBG_MatchState::Lobby)
		{
			BG_SHIN_LOG_WARN(TEXT("Cannot start game. Current state is not Lobby"));
			return;
		}
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetGameState<ABG_GameState>() returned null"));
		return;
	}

	const FString TravelURL =
		TEXT("/Game/SHIN/Maps/BattleMap?listen?game=/Game/SHIN/Blueprints/BP_BattleGameMode.BP_BattleGameMode_C");

	BG_SHIN_LOG_INFO(TEXT("ServerTravel URL = %s"), *TravelURL);

	World->ServerTravel(TravelURL, true);
}

void ABG_LobbyGameMode::NotifyStartRequested()
{
	if (bStartRequested)
	{
		BG_SHIN_LOG_WARN(TEXT("NotifyStartRequested skipped because start was already requested"));
		return;
	}

	BG_SHIN_LOG_EVENT_BLOCK(this, "LobbyGameMode NotifyStartRequested",
		TEXT("Broadcasting loading screen and scheduling DoServerTravel timer"));

	bStartRequested = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("NotifyStartRequested failed because World was null"));
		bStartRequested = false;
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABG_LobbyPlayerController* LobbyPC = Cast<ABG_LobbyPlayerController>(It->Get()))
		{
			BG_SHIN_LOG_INFO(TEXT("Calling Client_ShowLoadingScreen on %s"), *LobbyPC->GetName());
			LobbyPC->Client_ShowLoadingScreen();
		}
		else
		{
			BG_SHIN_LOG_WARN(TEXT("PlayerController is not ABG_LobbyPlayerController"));
		}
	}

	FTimerHandle TimerHandle;

	GetWorldTimerManager().SetTimer(
		TimerHandle,
		this,
		&ABG_LobbyGameMode::DoServerTravel,
		0.5f,
		false
	);

	BG_SHIN_LOG_INFO(TEXT("DoServerTravel timer scheduled with delay = 0.5"));
}

void ABG_LobbyGameMode::DoServerTravel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("DoServerTravel failed because World was null"));
		bStartRequested = false;
		return;
	}

	BG_SHIN_LOG_EVENT_BLOCK(this, "Lobby -> Battle ServerTravel",
		TEXT("Checking connected player controllers before travel"));

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		BG_SHIN_LOG_CONTROLLER(this, PC, "Travel Check");

		if (!IsValid(PC) || !PC->PlayerState)
		{
			BG_SHIN_LOG_WARN(TEXT("Abort Travel: Player not ready"));
			bStartRequested = false;
			return;
		}
	}

	const FString TravelURL =
		TEXT("/Game/SHIN/Maps/BattleMap?listen?game=/Game/SHIN/Blueprints/BP_BattleGameMode.BP_BattleGameMode_C");

	BG_SHIN_LOG_INFO(TEXT("ServerTravel URL = %s"), *TravelURL);

	World->ServerTravel(TravelURL, true);
}