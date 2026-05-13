#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BG_GameState.generated.h"

class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyPlayerListChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattlePlayerCountChanged);

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

	UFUNCTION(BlueprintPure, Category = "BG|Lobby")
	TArray<FString> GetLobbyPlayerNames() const;

	UFUNCTION(BlueprintCallable, Category = "BG|Lobby")
	void MarkLobbyPlayerListDirty();

	UFUNCTION(BlueprintCallable, Category = "BG|Lobby")
	void RecalculateLobbyPlayerCounts();

	UFUNCTION(BlueprintPure, Category = "BG|Lobby")
	int32 GetLobbyReadyPlayerCount() const { return LobbyReadyPlayerCount; }

	UFUNCTION(BlueprintPure, Category = "BG|Lobby")
	int32 GetLobbyTotalPlayerCount() const { return LobbyTotalPlayerCount; }

	UFUNCTION(BlueprintPure, Category = "BG|Battle")
	int32 GetBattleTotalPlayerCount() const { return BattleTotalPlayerCount; }

	UFUNCTION(BlueprintPure, Category = "BG|Battle")
	int32 GetAlivePlayerCount() const { return AlivePlayerCount; }

	void SetBattleTotalPlayerCount(int32 NewBattleTotalPlayerCount);
	void SetAlivePlayerCount(int32 NewAlivePlayerCount);

	UPROPERTY(BlueprintAssignable, Category = "BG|Lobby")
	FOnLobbyPlayerListChanged OnLobbyPlayerListChanged;

	UPROPERTY(BlueprintAssignable, Category = "BG|Battle")
	FOnBattlePlayerCountChanged OnBattlePlayerCountChanged;

protected:
	UFUNCTION()
	void OnRep_CurrentMatchState();
	
	UFUNCTION()
	void OnRep_PreparationPhase();

	UFUNCTION()
	void OnRep_PreparationTimeRemaining();

	UFUNCTION()
	void OnRep_LobbyPlayerListRevision();

	UFUNCTION()
	void OnRep_LobbyReadyPlayerCount();

	UFUNCTION()
	void OnRep_LobbyTotalPlayerCount();

	UFUNCTION()
	void OnRep_BattleTotalPlayerCount();

	UFUNCTION()
	void OnRep_AlivePlayerCount();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMatchState, BlueprintReadOnly, Category = "BG|GameState")
	EBG_MatchState CurrentMatchState = EBG_MatchState::Lobby;
	
	UPROPERTY(ReplicatedUsing = OnRep_PreparationPhase, BlueprintReadOnly, Category = "BG|GameState")
	bool bIsPreparationPhase = false;

	UPROPERTY(ReplicatedUsing = OnRep_PreparationTimeRemaining, BlueprintReadOnly, Category = "BG|GameState")
	int32 PreparationTimeRemaining = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyPlayerListRevision, BlueprintReadOnly, Category = "BG|Lobby")
	int32 LobbyPlayerListRevision = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyReadyPlayerCount, BlueprintReadOnly, Category = "BG|Lobby")
	int32 LobbyReadyPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyTotalPlayerCount, BlueprintReadOnly, Category = "BG|Lobby")
	int32 LobbyTotalPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_BattleTotalPlayerCount, BlueprintReadOnly, Category = "BG|Battle")
	int32 BattleTotalPlayerCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AlivePlayerCount, BlueprintReadOnly, Category = "BG|Battle")
	int32 AlivePlayerCount = 0;
};