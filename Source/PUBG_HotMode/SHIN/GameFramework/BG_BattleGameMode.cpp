#include "BG_BattleGameMode.h"
#include "BG_GameState.h"
#include "Player/BG_PlayerController.h"
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

		StartPreparationPhase();
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

void ABG_BattleGameMode::StartPreparationPhase()
{
	ABG_GameState* BGGameState = GetGameState<ABG_GameState>();
	if (!BGGameState)
	{
		BG_SHIN_LOG_ERROR(TEXT("StartPreparationPhase failed because GameState was null"));
		return;
	}

	BGGameState->SetPreparationPhase(true);
	BGGameState->SetPreparationTimeRemaining(PreparationDuration);

	BG_SHIN_LOG_INFO(TEXT("Preparation phase started. Duration = %d"), PreparationDuration);

	GetWorldTimerManager().SetTimer(
		PreparationTimerHandle,
		this,
		&ABG_BattleGameMode::TickPreparationPhase,
		1.0f,
		true
	);
}

void ABG_BattleGameMode::TickPreparationPhase()
{
	ABG_GameState* BGGameState = GetGameState<ABG_GameState>();
	if (!BGGameState)
	{
		BG_SHIN_LOG_ERROR(TEXT("TickPreparationPhase failed because GameState was null"));
		GetWorldTimerManager().ClearTimer(PreparationTimerHandle);
		return;
	}

	const int32 NewTimeRemaining = BGGameState->GetPreparationTimeRemaining() - 1;
	BGGameState->SetPreparationTimeRemaining(NewTimeRemaining);

	BG_SHIN_LOG_INFO(TEXT("PreparationTimeRemaining = %d"), NewTimeRemaining);

	if (NewTimeRemaining <= 0)
	{
		GetWorldTimerManager().ClearTimer(PreparationTimerHandle);

		BGGameState->SetPreparationPhase(false);
		BGGameState->SetPreparationTimeRemaining(0);
		BGGameState->SetMatchState(EBG_MatchState::Combat);

		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (ABG_PlayerController* BGPlayerController = Cast<ABG_PlayerController>(It->Get()))
				{
					BG_SHIN_LOG_INFO(TEXT("Calling Client_HideBattleWaitingTimeUI on %s"), *BGPlayerController->GetName());
					BGPlayerController->Client_HideBattleWaitingTimeUI();
				}
			}
		}
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ABG_AirplaneActor* Airplane = GetWorld()->SpawnActor<ABG_AirplaneActor>(
			AirplaneClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams
		);

		if (Airplane)
		{
			Airplane->InitializeFlightPath(BlueZoneCenter, BlueZoneRadius, 45.0f);
			Airplane->StartFlight();
		}

		BG_SHIN_LOG_INFO(TEXT("Preparation phase ended. MatchState set to Combat"));
	}
	
	
}