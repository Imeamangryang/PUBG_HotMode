#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BG_GameInstance.generated.h"

UCLASS()
class PUBG_HOTMODE_API UBG_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintCallable)
	void ConnectToDedicatedServer();
};