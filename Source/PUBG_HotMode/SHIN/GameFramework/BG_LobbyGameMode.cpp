#include "BG_LobbyGameMode.h"
#include "BG_GameState.h"
#include "Character/BG_LobbyPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ABG_LobbyGameMode::ABG_LobbyGameMode()
{
	bUseSeamlessTravel = false;
	GameStateClass = ABG_GameState::StaticClass();
	PlayerControllerClass = ABG_LobbyPlayerController::StaticClass();
}

void ABG_LobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		BGGameState->SetMatchState(EBG_MatchState::Lobby);
	}

	UE_LOG(LogTemp, Log, TEXT("[LobbyGameMode] Lobby BeginPlay"));
}

void ABG_LobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	TArray<AActor*> FoundCameras;
	UGameplayStatics::GetAllActorsWithTag(this, FName(TEXT("LobbyCamera")), FoundCameras);

	if (FoundCameras.Num() > 0)
	{
		NewPlayer->SetViewTargetWithBlend(FoundCameras[0], 0.0f);
	}
}

void ABG_LobbyGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameMode] Player left: %s"),
		*GetNameSafe(Exiting));

	Super::Logout(Exiting);
}

void ABG_LobbyGameMode::RequestStartGame()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ABG_GameState* BGGameState = GetGameState<ABG_GameState>())
	{
		if (BGGameState->GetMatchState() != EBG_MatchState::Lobby)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LobbyGameMode] Cannot start game. Current state is not Lobby."));
			return;
		}
	}

	//const FString TravelURL = BattleMapName.ToString() + TEXT("?game=/Game/SHIN/Blueprints/BP_BattleGameMode.BP_BattleGameMode_C"); 
	const FString TravelURL =
	TEXT("/Game/SHIN/Maps/BattleMap?listen?game=/Game/SHIN/Blueprints/BP_BattleGameMode.BP_BattleGameMode_C");
	
	UE_LOG(LogTemp, Log, TEXT("[LobbyGameMode] ServerTravel to: %s"), *TravelURL);

	World->ServerTravel(TravelURL, true);
}

void ABG_LobbyGameMode::NotifyStartRequested()
{
	if (bStartRequested)
	{
		return;
	}

	bStartRequested = true;

	FTimerHandle TimerHandle;
	
	GetWorldTimerManager().SetTimer(
		TimerHandle,
		this,
		&ABG_LobbyGameMode::DoServerTravel,
		0.5f,
		false
	);
}

void ABG_LobbyGameMode::DoServerTravel()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();

		if (!IsValid(PC) || !PC->PlayerState)
		{
			UE_LOG(LogTemp, Warning, TEXT("Abort Travel: Player not ready"));
			bStartRequested = false;
			return;
		}
	}

	const FString TravelURL =
		TEXT("/Game/SHIN/Maps/BattleMap?listen?game=/Game/SHIN/Blueprints/BP_BattleGameMode.BP_BattleGameMode_C");

	GetWorld()->ServerTravel(TravelURL, true);
}