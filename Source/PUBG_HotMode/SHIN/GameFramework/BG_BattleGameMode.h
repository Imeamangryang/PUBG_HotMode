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

	UFUNCTION(BlueprintNativeEvent, Category = "Spectator")
	void HandlePlayerDeath(AController* VictimController, APawn* VictimPawn);

protected:
	void SpawnAndPossessPlayer(APlayerController* NewPlayer);
	void StartPreparationPhase();
	void TickPreparationPhase();
	
	void EvaluateMatchEnd();
	int32 GetAlivePlayerCount() const;
	AController* GetLastAlivePlayerController() const;
	void EndBattleMatch(AController* WinnerController);
	void CacheBattleStartPlayerCount();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "BG|Battle")
	int32 PreparationDuration = 30;

	UPROPERTY(Transient)
	TObjectPtr<class ABG_Airplane> PreparedAirplane = nullptr;

	FTimerHandle PreparationTimerHandle;
	
	UPROPERTY(Transient)
	TObjectPtr<class ABG_BlueZone> BlueZoneActor = nullptr;
	
	UPROPERTY(Transient)
	int32 BattleStartPlayerCount = 0;
};