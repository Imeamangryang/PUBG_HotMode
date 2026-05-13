#include "BG_GameState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_PlayerState.h"

ABG_GameState::ABG_GameState()
{
	bReplicates = true;
}

void ABG_GameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_GameState, CurrentMatchState);
	DOREPLIFETIME(ABG_GameState, bIsPreparationPhase);
	DOREPLIFETIME(ABG_GameState, PreparationTimeRemaining);
	DOREPLIFETIME(ABG_GameState, LobbyPlayerListRevision);
}

void ABG_GameState::SetMatchState(EBG_MatchState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentMatchState == NewState)
	{
		return;
	}

	CurrentMatchState = NewState;
	OnRep_CurrentMatchState();
}

void ABG_GameState::SetPreparationPhase(bool bInPreparationPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsPreparationPhase == bInPreparationPhase)
	{
		return;
	}

	bIsPreparationPhase = bInPreparationPhase;
	OnRep_PreparationPhase();
}

void ABG_GameState::SetPreparationTimeRemaining(int32 NewTimeRemaining)
{
	if (!HasAuthority())
	{
		return;
	}

	if (PreparationTimeRemaining == NewTimeRemaining)
	{
		return;
	}

	PreparationTimeRemaining = NewTimeRemaining;
	OnRep_PreparationTimeRemaining();
}

TArray<FString> ABG_GameState::GetLobbyPlayerNames() const
{
	TArray<FString> LobbyPlayerNames;
	TSet<FString> UniqueNames;

	UE_LOG(LogTemp, Warning, TEXT("[BG_GameState] GetLobbyPlayerNames start. PlayerArrayNum=%d"), PlayerArray.Num());

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (!IsValid(PlayerState))
		{
			UE_LOG(LogTemp, Warning, TEXT("[BG_GameState] Skipping invalid PlayerState"));
			continue;
		}

		const FString PlayerName = PlayerState->GetPlayerName().TrimStartAndEnd();

		UE_LOG(LogTemp, Warning, TEXT("[BG_GameState] Candidate PlayerState=%s PlayerName=%s"),
			*GetNameSafe(PlayerState),
			*PlayerName);

		if (PlayerName.IsEmpty())
		{
			continue;
		}

		if (UniqueNames.Contains(PlayerName))
		{
			continue;
		}

		UniqueNames.Add(PlayerName);
		LobbyPlayerNames.Add(PlayerName);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BG_GameState] GetLobbyPlayerNames result count=%d"), LobbyPlayerNames.Num());

	return LobbyPlayerNames;
}

void ABG_GameState::MarkLobbyPlayerListDirty()
{
	if (!HasAuthority())
	{
		return;
	}

	++LobbyPlayerListRevision;
	OnRep_LobbyPlayerListRevision();
}

int32 ABG_GameState::GetLobbyTotalPlayerCount() const
{
	int32 TotalPlayerCount = 0;

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (!IsValid(PlayerState))
		{
			continue;
		}

		++TotalPlayerCount;
	}

	return TotalPlayerCount;
}

int32 ABG_GameState::GetLobbyReadyPlayerCount() const
{
	int32 ReadyPlayerCount = 0;

	for (APlayerState* PlayerState : PlayerArray)
	{
		const ABG_PlayerState* BGPlayerState = Cast<ABG_PlayerState>(PlayerState);
		if (!IsValid(BGPlayerState))
		{
			continue;
		}

		if (BGPlayerState->IsReadyToStart())
		{
			++ReadyPlayerCount;
		}
	}

	return ReadyPlayerCount;
}

void ABG_GameState::OnRep_CurrentMatchState()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_GameState] MatchState changed to: %s"),
		*StaticEnum<EBG_MatchState>()->GetNameStringByValue(static_cast<int64>(CurrentMatchState)));
}

void ABG_GameState::OnRep_PreparationPhase()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_GameState] PreparationPhase changed: %s"),
		bIsPreparationPhase ? TEXT("true") : TEXT("false"));
}

void ABG_GameState::OnRep_PreparationTimeRemaining()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_GameState] PreparationTimeRemaining changed: %d"),
		PreparationTimeRemaining);
}

void ABG_GameState::OnRep_LobbyPlayerListRevision()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_GameState] LobbyPlayerListRevision changed: %d"),
		LobbyPlayerListRevision);

	OnLobbyPlayerListChanged.Broadcast();
}

