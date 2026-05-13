#include "Combat/BG_Scope.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"

ABG_Scope::ABG_Scope()
{
	PrimaryActorTick.bCanEverTick = false;

	ScopeRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ScopeRoot"));
	SetRootComponent(ScopeRootComponent);

	ScopeBodyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScopeBodyMesh"));
	ScopeBodyMeshComponent->SetupAttachment(ScopeRootComponent);
	ScopeBodyMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScopeBodyMeshComponent->SetGenerateOverlapEvents(false);

	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ScopeSceneCapture"));
	SceneCaptureComponent->SetupAttachment(ScopeRootComponent);
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;
	SceneCaptureComponent->SetVisibility(false);
	SceneCaptureComponent->SetHiddenInGame(true);
}

void ABG_Scope::SetScopeActive(bool bNewActive)
{
	bIsScopeActive = bNewActive;

	if (!SceneCaptureComponent)
	{
		return;
	}

	SceneCaptureComponent->bCaptureEveryFrame = bNewActive;
	SceneCaptureComponent->bCaptureOnMovement = bNewActive;
	SceneCaptureComponent->SetVisibility(bNewActive);
	SceneCaptureComponent->SetHiddenInGame(!bNewActive);

	if (bNewActive)
	{
		SceneCaptureComponent->CaptureScene();
	}
}

void ABG_Scope::SetScopeRenderTarget(UTextureRenderTarget2D* NewRenderTarget)
{
	ScopeRenderTarget = NewRenderTarget;

	if (SceneCaptureComponent)
	{
		SceneCaptureComponent->TextureTarget = ScopeRenderTarget;
	}
}

