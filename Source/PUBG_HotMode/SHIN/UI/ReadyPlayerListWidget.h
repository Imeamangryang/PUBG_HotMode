#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReadyPlayerListWidget.generated.h"

class UTextBlock;
class ABG_GameState;

UCLASS()
class PUBG_HOTMODE_API UReadyPlayerListWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void RefreshReadyPlayerCount();

	void BindGameStateDelegate();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReadyCountText;

	UPROPERTY()
	TObjectPtr<ABG_GameState> CachedGameState;
};