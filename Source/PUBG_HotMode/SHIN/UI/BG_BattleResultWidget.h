#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BG_BattleResultWidget.generated.h"

class UTextBlock;
class ABG_GameState;
class ABG_PlayerState;

UCLASS()
class PUBG_HOTMODE_API UBG_BattleResultWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void RefreshResultText();

	UFUNCTION()
	void HandleBattlePlayerCountChanged();

	UFUNCTION()
	void HandleFinalRankChanged(int32 NewFinalRank);

	void BindStateDelegates();
	void UnbindStateDelegates();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Textblock_Rank = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Textblock_PlayerCount = nullptr;

	UPROPERTY()
	TObjectPtr<ABG_GameState> CachedGameState = nullptr;

	UPROPERTY()
	TObjectPtr<ABG_PlayerState> CachedPlayerState = nullptr;
};