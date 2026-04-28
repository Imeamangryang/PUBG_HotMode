#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BG_BattleGameMode.generated.h"

UCLASS()
class PUBG_HOTMODE_API ABG_BattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABG_BattleGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

protected:
	void SpawnAndPossessPlayer(APlayerController* NewPlayer);
};