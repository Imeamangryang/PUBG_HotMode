#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BG_PlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinalRankChanged, int32, NewFinalRank);

UCLASS()
class PUBG_HOTMODE_API ABG_PlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABG_PlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "BG|Lobby")
	bool IsReadyToStart() const { return bReadyToStart; }

	void SetReadyToStart(bool bInReadyToStart);

	UFUNCTION(BlueprintPure, Category = "BG|Battle")
	int32 GetFinalRank() const { return FinalRank; }

	UFUNCTION(BlueprintPure, Category = "BG|Battle")
	bool HasFinalRank() const { return FinalRank > 0; }

	void SetFinalRank(int32 InFinalRank);
	
	UPROPERTY(BlueprintAssignable, Category = "BG|Battle")
	FOnFinalRankChanged OnFinalRankChanged;

protected:
	UFUNCTION()
	void OnRep_ReadyToStart();

	UFUNCTION()
	void OnRep_FinalRank();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_ReadyToStart, BlueprintReadOnly, Category = "BG|Lobby")
	bool bReadyToStart = false;

	UPROPERTY(ReplicatedUsing = OnRep_FinalRank, BlueprintReadOnly, Category = "BG|Battle")
	int32 FinalRank = 0;
};