#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BG_HUD.generated.h"

class UCompassWidget;

UCLASS()
class PUBG_HOTMODE_API ABG_HUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

protected:
	// 방위 위젯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCompassWidget> CompassWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UCompassWidget> CompassWidget;
};