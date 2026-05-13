#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BG_Scope.generated.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class PUBG_HOTMODE_API ABG_Scope : public AActor
{
	GENERATED_BODY()

public:
	ABG_Scope();

	UFUNCTION(BlueprintCallable, Category="Scope")
	void SetScopeActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category="Scope")
	bool IsScopeActive() const { return bIsScopeActive; }

	UFUNCTION(BlueprintCallable, Category="Scope")
	void SetScopeRenderTarget(UTextureRenderTarget2D* NewRenderTarget);

	UFUNCTION(BlueprintPure, Category="Scope")
	UTextureRenderTarget2D* GetScopeRenderTarget() const { return ScopeRenderTarget; }

	UFUNCTION(BlueprintPure, Category="Scope")
	USceneCaptureComponent2D* GetSceneCaptureComponent() const { return SceneCaptureComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scope", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ScopeRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scope", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ScopeBodyMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scope", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scope", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTextureRenderTarget2D> ScopeRenderTarget = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Scope", meta=(AllowPrivateAccess="true"))
	bool bIsScopeActive = false;
};

