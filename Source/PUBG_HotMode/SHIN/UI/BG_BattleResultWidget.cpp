#include "UI/BG_BattleResultWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/BG_GameState.h"
#include "Player/BG_PlayerState.h"

void UBG_BattleResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindStateDelegates();
	RefreshResultText();
}

void UBG_BattleResultWidget::NativeDestruct()
{
	UnbindStateDelegates();

	Super::NativeDestruct();
}

void UBG_BattleResultWidget::BindStateDelegates()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CachedGameState = World->GetGameState<ABG_GameState>();

	if (CachedGameState)
	{
		CachedGameState->OnBattlePlayerCountChanged.RemoveAll(this);
		CachedGameState->OnBattlePlayerCountChanged.AddDynamic(this, &UBG_BattleResultWidget::HandleBattlePlayerCountChanged);
	}

	if (APlayerController* OwningPlayerController = GetOwningPlayer())
	{
		CachedPlayerState = OwningPlayerController->GetPlayerState<ABG_PlayerState>();
	}

	if (CachedPlayerState)
	{
		CachedPlayerState->OnFinalRankChanged.RemoveAll(this);
		CachedPlayerState->OnFinalRankChanged.AddDynamic(this, &UBG_BattleResultWidget::HandleFinalRankChanged);
	}
}

void UBG_BattleResultWidget::UnbindStateDelegates()
{
	if (CachedGameState)
	{
		CachedGameState->OnBattlePlayerCountChanged.RemoveAll(this);
		CachedGameState = nullptr;
	}

	if (CachedPlayerState)
	{
		CachedPlayerState->OnFinalRankChanged.RemoveAll(this);
		CachedPlayerState = nullptr;
	}
}

void UBG_BattleResultWidget::HandleBattlePlayerCountChanged()
{
	RefreshResultText();
}

void UBG_BattleResultWidget::HandleFinalRankChanged(int32 NewFinalRank)
{
	RefreshResultText();
}

void UBG_BattleResultWidget::RefreshResultText()
{
	if (!Textblock_Rank || !Textblock_PlayerCount)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ABG_GameState* NewGameState = World->GetGameState<ABG_GameState>();
	if (CachedGameState != NewGameState)
	{
		if (CachedGameState)
		{
			CachedGameState->OnBattlePlayerCountChanged.RemoveAll(this);
		}

		CachedGameState = NewGameState;

		if (CachedGameState)
		{
			CachedGameState->OnBattlePlayerCountChanged.RemoveAll(this);
			CachedGameState->OnBattlePlayerCountChanged.AddDynamic(this, &UBG_BattleResultWidget::HandleBattlePlayerCountChanged);
		}
	}

	ABG_PlayerState* NewPlayerState = nullptr;
	if (APlayerController* OwningPlayerController = GetOwningPlayer())
	{
		NewPlayerState = OwningPlayerController->GetPlayerState<ABG_PlayerState>();
	}

	if (CachedPlayerState != NewPlayerState)
	{
		if (CachedPlayerState)
		{
			CachedPlayerState->OnFinalRankChanged.RemoveAll(this);
		}

		CachedPlayerState = NewPlayerState;

		if (CachedPlayerState)
		{
			CachedPlayerState->OnFinalRankChanged.RemoveAll(this);
			CachedPlayerState->OnFinalRankChanged.AddDynamic(this, &UBG_BattleResultWidget::HandleFinalRankChanged);
		}
	}

	const int32 FinalRank = CachedPlayerState ? CachedPlayerState->GetFinalRank() : 0;
	const int32 TotalPlayerCount = CachedGameState ? CachedGameState->GetBattleTotalPlayerCount() : 0;

	Textblock_Rank->SetText(FText::FromString(FString::Printf(TEXT("#%d"), FinalRank)));
	Textblock_PlayerCount->SetText(FText::FromString(FString::Printf(TEXT("/%d"), TotalPlayerCount)));
	
	UE_LOG(LogTemp, Warning, TEXT("[BG_BattleResultWidget] Widget=%s CachedPlayerState=%s FinalRank=%d TotalPlayerCount=%d"),
	*GetNameSafe(this),
	*GetNameSafe(CachedPlayerState),
	FinalRank,
	TotalPlayerCount);
	
	APlayerController* OwningPlayerController = GetOwningPlayer();
	UE_LOG(LogTemp, Warning, TEXT("[BG_BattleResultWidget] OwningPlayerController=%s PlayerState=%s"),
		*GetNameSafe(OwningPlayerController),
		*GetNameSafe(OwningPlayerController ? OwningPlayerController->PlayerState : nullptr));
}