#include "BG_GameState.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

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
	if (!HasAuthority()) // 서버만 State 변경 가능
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

TArray<APlayerState*> ABG_GameState::GetLobbyPlayerStates() const
{
	TArray<APlayerState*> LobbyPlayerStates;

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (!PlayerState)
		{
			continue;
		}

		LobbyPlayerStates.Add(PlayerState);
	}

	return LobbyPlayerStates;
}

TArray<FString> ABG_GameState::GetLobbyPlayerNames() const
{
	TArray<FString> LobbyPlayerNames;

	for (APlayerState* PlayerState : PlayerArray)
	{
		if (!PlayerState)
		{
			continue;
		}

		LobbyPlayerNames.Add(PlayerState->GetPlayerName());
	}

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