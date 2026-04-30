#include "BG_BattleGameMode.h"
#include "BG_GameState.h"
#include "Player/BG_PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Utils/BG_LogHelper.h"

ABG_BattleGameMode::ABG_BattleGameMode()
{
	GameStateClass = ABG_GameState::StaticClass();
	PlayerControllerClass = ABG_PlayerController::StaticClass();
}

void ABG_BattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	BG_SHIN_LOG_EVENT_BLOCK(this, "BattleGameMode BeginPlay",
		TEXT("GameMode=%s GameStateClass=%s PlayerControllerClass=%s DefaultPawnClass=%s"),
		*GetClass()->GetName(),
		*BGLogHelper::SafeClass(GameStateClass),
		*BGLogHelper::SafeClass(PlayerControllerClass),
		*BGLogHelper::SafeClass(DefaultPawnClass));

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		BGGameState->SetMatchState(EBG_MatchState::PreMatch);
		BG_SHIN_LOG_INFO(TEXT("MatchState set to PreMatch"));
	}
	else
	{
		BG_SHIN_LOG_ERROR(TEXT("GetGameState<ABG_GameState>() returned null"));
	}
}

void ABG_BattleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	BG_SHIN_LOG_EVENT_BLOCK(this, "BattleGameMode PostLogin",
		TEXT("NewPlayerClass=%s"),
		*BGLogHelper::SafeClass(NewPlayer));

	BG_SHIN_LOG_CONTROLLER(this, NewPlayer, "Battle PostLogin");
}

void ABG_BattleGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	BG_SHIN_LOG_EVENT_BLOCK(this, "HandleStartingNewPlayer",
		TEXT("About to call RestartPlayer"));

	BG_SHIN_LOG_CONTROLLER(this, NewPlayer, "Before RestartPlayer");

	if (!NewPlayer)
	{
		BG_SHIN_LOG_ERROR(TEXT("HandleStartingNewPlayer failed because NewPlayer was null"));
		return;
	}

	RestartPlayer(NewPlayer);

	BG_SHIN_LOG_CONTROLLER(this, NewPlayer, "After RestartPlayer");
}

void ABG_BattleGameMode::SpawnAndPossessPlayer(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		BG_SHIN_LOG_ERROR(TEXT("SpawnAndPossessPlayer failed because NewPlayer was null"));
		return;
	}

	BG_SHIN_LOG_EVENT_BLOCK(this, "SpawnAndPossessPlayer",
		TEXT("Starting spawn flow for controller=%s"),
		*BGLogHelper::SafeName(NewPlayer));

	BG_SHIN_LOG_CONTROLLER(this, NewPlayer, "Before SpawnAndPossessPlayer");

	if (NewPlayer->GetPawn())
	{
		BG_SHIN_LOG_WARN(TEXT("Player already has pawn: %s"), *BGLogHelper::SafeName(NewPlayer->GetPawn()));
		return;
	}

	AActor* StartSpot = ChoosePlayerStart(NewPlayer);
	if (!StartSpot)
	{
		BG_SHIN_LOG_ERROR(TEXT("No PlayerStart found for controller: %s"), *BGLogHelper::SafeName(NewPlayer));
		return;
	}

	BG_SHIN_LOG_ACTOR(this, StartSpot, "Chosen StartSpot");

	if (!DefaultPawnClass)
	{
		BG_SHIN_LOG_ERROR(TEXT("DefaultPawnClass is not set"));
		return;
	}

	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, StartSpot->GetActorTransform());
	if (!NewPawn)
	{
		BG_SHIN_LOG_ERROR(TEXT("Failed to spawn pawn for controller: %s"), *BGLogHelper::SafeName(NewPlayer));
		return;
	}

	BG_SHIN_LOG_ACTOR(this, NewPawn, "Spawned Pawn");

	NewPlayer->Possess(NewPawn);

	BG_SHIN_LOG_CONTROLLER(this, NewPlayer, "After Possess");
}

void ABG_BattleGameMode::HandlePlayerDeath_Implementation(AController* VictimController, APawn* VictimPawn)
{
	if (!VictimController)
	{
		BG_SHIN_LOG_ERROR(TEXT("HandlePlayerDeath failed because VictimController was null"));
		return;
	}

	BG_SHIN_LOG_EVENT_BLOCK(this, "HandlePlayerDeath",
		TEXT("VictimController=%s VictimPawn=%s"),
		*BGLogHelper::SafeName(VictimController),
		*BGLogHelper::SafeName(VictimPawn));
}
