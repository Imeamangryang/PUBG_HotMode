#include "BG_HUD.h"
#include "UI/CompassWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void ABG_HUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* OwnerPlayerController = GetOwningPlayerController();
	if (!OwnerPlayerController)
	{
		UE_LOG(LogTemp, Error, TEXT("ABG_HUD::BeginPlay - OwningPlayerController is null."));
		return;
	}

	if (!CompassWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ABG_HUD::BeginPlay - CompassWidgetClass is null."));
		return;
	}

	if (CompassWidget)
	{
		return;
	}

	CompassWidget = CreateWidget<UCompassWidget>(OwnerPlayerController, CompassWidgetClass);
	if (!CompassWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ABG_HUD::BeginPlay - Failed to create CompassWidget."));
		return;
	}

	CompassWidget->AddToViewport();
}