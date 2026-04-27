#include "BG_GameState.h"
#include "Net/UnrealNetwork.h"

ABG_GameState::ABG_GameState()
{
	bReplicates = true;
}

void ABG_GameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_GameState, CurrentMatchState);
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

void ABG_GameState::OnRep_CurrentMatchState()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_GameState] MatchState changed to: %s"),
		*StaticEnum<EBG_MatchState>()->GetNameStringByValue(static_cast<int64>(CurrentMatchState)));
}