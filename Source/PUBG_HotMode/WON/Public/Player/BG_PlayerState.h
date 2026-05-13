#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BG_PlayerState.generated.h"

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

protected:
	UFUNCTION()
	void OnRep_ReadyToStart();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_ReadyToStart, BlueprintReadOnly, Category = "BG|Lobby")
	bool bReadyToStart = false;
};