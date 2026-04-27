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

protected:
	UFUNCTION()
	void OnRep_CurrentMatchState();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMatchState, BlueprintReadOnly, Category = "BG|GameState")
	EBG_MatchState CurrentMatchState = EBG_MatchState::Lobby;
};