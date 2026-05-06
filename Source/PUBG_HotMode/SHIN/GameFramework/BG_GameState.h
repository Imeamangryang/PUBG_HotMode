#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BG_GameState.generated.h"

UENUM(BlueprintType)
enum class EBG_MatchState : uint8
{
	Lobby      UMETA(DisplayName = "Lobby"),
	PreMatch   UMETA(DisplayName = "PreMatch"),
	InAircraft UMETA(DisplayName = "InAircraft"),
	Combat     UMETA(DisplayName = "Combat"),
	Result     UMETA(DisplayName = "Result")
};

UCLASS()
class PUBG_HOTMODE_API ABG_GameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ABG_GameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "BG|GameState")
	void SetMatchState(EBG_MatchState NewState);

	UFUNCTION(BlueprintPure, Category = "BG|GameState")
	EBG_MatchState GetMatchState() const { return CurrentMatchState; }
	
	UFUNCTION(BlueprintPure, Category = "BG|GameState")
	bool IsPreparationPhase() const { return bIsPreparationPhase; }

	UFUNCTION(BlueprintPure, Category = "BG|GameState")
	int32 GetPreparationTimeRemaining() const { return PreparationTimeRemaining; }

	void SetPreparationPhase(bool bInPreparationPhase);
	void SetPreparationTimeRemaining(int32 NewTimeRemaining);

protected:
	UFUNCTION()
	void OnRep_CurrentMatchState();
	
	UFUNCTION()
	void OnRep_PreparationPhase();

	UFUNCTION()
	void OnRep_PreparationTimeRemaining();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMatchState, BlueprintReadOnly, Category = "BG|GameState")
	EBG_MatchState CurrentMatchState = EBG_MatchState::Lobby;
	
	UPROPERTY(ReplicatedUsing = OnRep_PreparationPhase, BlueprintReadOnly, Category = "BG|GameState")
	bool bIsPreparationPhase = false;

	UPROPERTY(ReplicatedUsing = OnRep_PreparationTimeRemaining, BlueprintReadOnly, Category = "BG|GameState")
	int32 PreparationTimeRemaining = 0;
};