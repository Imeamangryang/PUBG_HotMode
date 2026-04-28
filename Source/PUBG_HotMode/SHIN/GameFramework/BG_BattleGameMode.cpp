#include "BG_BattleGameMode.h"
#include "BG_GameState.h"
#include "Player/BG_PlayerController.h"
#include "GameFramework/PlayerStart.h"

ABG_BattleGameMode::ABG_BattleGameMode()
{
	GameStateClass = ABG_GameState::StaticClass();
	PlayerControllerClass = ABG_PlayerController::StaticClass();
}

void ABG_BattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		BGGameState->SetMatchState(EBG_MatchState::PreMatch);
	}

	UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] BeginPlay - MatchState set to PreMatch"));
}

void ABG_BattleGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	UE_LOG(LogTemp, Log, TEXT("[BattleGameMode] Player joined with controller: %s"),
		*GetNameSafe(NewPlayer ? NewPlayer->GetClass() : nullptr));
}

void ABG_BattleGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] HandleStartingNewPlayer for controller: %s"),
		*GetNameSafe(NewPlayer));
	
	RestartPlayer(NewPlayer);
}
 
void ABG_BattleGameMode::SpawnAndPossessPlayer(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] SpawnAndPossessPlayer failed because NewPlayer was null."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] SpawnAndPossessPlayer start - Controller: %s, ExistingPawn: %s"),
		*GetNameSafe(NewPlayer),
		*GetNameSafe(NewPlayer->GetPawn()));

	if (NewPlayer->GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] Player already has pawn: %s"),
			*GetNameSafe(NewPlayer->GetPawn()));
		return;
	}

	AActor* StartSpot = ChoosePlayerStart(NewPlayer);
	if (!StartSpot)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] No PlayerStart found for controller: %s"),
			*GetNameSafe(NewPlayer));
		return;
	}

	if (!DefaultPawnClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] DefaultPawnClass is not set."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] Spawning pawn at start spot: %s"),
		*GetNameSafe(StartSpot));

	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, StartSpot->GetActorTransform());
	if (!NewPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("[BattleGameMode] Failed to spawn pawn for controller: %s"),
			*GetNameSafe(NewPlayer));
		return;
	}

	NewPlayer->Possess(NewPawn);

	UE_LOG(LogTemp, Warning, TEXT("[BattleGameMode] Possessed pawn %s with controller %s"),
		*GetNameSafe(NewPawn),
		*GetNameSafe(NewPlayer));
}