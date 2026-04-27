#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BG_LobbyGameMode.generated.h"

UCLASS()
class PUBG_HOTMODE_API ABG_LobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABG_LobbyGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "BG|Lobby")
	void RequestStartGame();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "BG|Lobby")
	FName BattleMapName = TEXT("/Game/SHIN/Maps/BattleMap");
};