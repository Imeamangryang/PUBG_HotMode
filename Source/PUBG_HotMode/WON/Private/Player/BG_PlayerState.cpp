#include "Player/BG_PlayerState.h"
#include "Net/UnrealNetwork.h"

ABG_PlayerState::ABG_PlayerState()
{
}

void ABG_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_PlayerState, bReadyToStart);
	DOREPLIFETIME(ABG_PlayerState, FinalRank);
}

void ABG_PlayerState::SetReadyToStart(bool bInReadyToStart)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("SetReadyToStart failed because authority was missing"));
		return;
	}

	if (bReadyToStart == bInReadyToStart)
	{
		return;
	}

	bReadyToStart = bInReadyToStart;
	OnRep_ReadyToStart();
}

void ABG_PlayerState::OnRep_ReadyToStart()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_PlayerState] ReadyToStart changed: %s"),
		bReadyToStart ? TEXT("true") : TEXT("false"));
}

void ABG_PlayerState::SetFinalRank(int32 InFinalRank)
{
	UE_LOG(LogTemp, Warning, TEXT("[BG_PlayerState] SetFinalRank called. HasAuthority=%s Current=%d New=%d Player=%s"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		FinalRank,
		InFinalRank,
		*GetNameSafe(this));

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("SetFinalRank failed because authority was missing"));
		return;
	}

	if (FinalRank == InFinalRank)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG_PlayerState] SetFinalRank skipped because value is unchanged"));
		return;
	}

	FinalRank = InFinalRank;
	OnRep_FinalRank();
}
void ABG_PlayerState::OnRep_FinalRank()
{
	UE_LOG(LogTemp, Log, TEXT("[BG_PlayerState] FinalRank changed: %d"),
		FinalRank);
}