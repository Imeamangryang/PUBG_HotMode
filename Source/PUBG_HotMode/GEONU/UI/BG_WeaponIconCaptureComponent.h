#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"
#include "UI/BG_InventoryViewModel.h"
#include "BG_WeaponIconCaptureComponent.generated.h"

class ABG_EquippedWeaponBase;
class UMaterialInterface;
class UPointLightComponent;
class UPrimitiveComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UTextureRenderTarget2D;

USTRUCT()
struct FBGWeaponIconPreviewCacheEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UObject> BrushResource = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABG_EquippedWeaponBase> PreviewActor = nullptr;

	UPROPERTY(Transient)
	TSubclassOf<ABG_EquippedWeaponBase> EquippedWeaponClass = nullptr;

	UPROPERTY(Transient)
	int32 LastAccessSerial = 0;
};

UCLASS(BlueprintType, ClassGroup="UI", meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_WeaponIconCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_WeaponIconCaptureComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
	UTextureRenderTarget2D* GetOrCreateWeaponPreview(const FBGEquipmentSlotRenderData& SlotData);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
	UObject* GetOrCreateWeaponSlotIconResource(const FBGEquipmentSlotRenderData& SlotData);

	UFUNCTION(BlueprintPure, Category="Inventory UI|Weapon Preview")
	FVector2D GetPreviewRenderTargetSize() const;

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
	void InvalidateWeaponPreview(FName PreviewIconKey);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Weapon Preview")
	void ClearPreviewCache();

	static FName MakePreviewIconKey(
		const FGameplayTag& ItemTag,
		EBG_EquipmentSlot Slot,
		const TSoftClassPtr<ABG_EquippedWeaponBase>& EquippedWeaponClass);

private:
	bool IsCaptureAllowed(const TCHAR* OperationName) const;
	bool InitializeCaptureRig();
	void DestroyCaptureRig();
	bool ResolveWeaponClass(
		const FBGEquipmentSlotRenderData& SlotData,
		TSoftClassPtr<ABG_EquippedWeaponBase>& OutEquippedWeaponClass) const;
	FName ResolvePreviewIconKey(
		const FBGEquipmentSlotRenderData& SlotData,
		const TSoftClassPtr<ABG_EquippedWeaponBase>& EquippedWeaponClass) const;
	UTextureRenderTarget2D* CreateRenderTarget(FName PreviewIconKey);
	UTextureRenderTarget2D* CreateRenderTargetObject(FName PreviewIconKey);
	void ReleaseRenderTarget(UTextureRenderTarget2D* RenderTarget);
	UObject* BuildPreviewBrushResource(UTextureRenderTarget2D* RenderTarget) const;
	void ConfigureSceneCaptureSource() const;
	void StartRenderTargetPrewarm();
	void StopRenderTargetPrewarm();
	void PrewarmRenderTargetStep();
	ABG_EquippedWeaponBase* SpawnPreviewActor(
		TSubclassOf<ABG_EquippedWeaponBase> EquippedWeaponClass,
		FName PreviewIconKey);
	bool CapturePreviewActor(ABG_EquippedWeaponBase* PreviewActor, UTextureRenderTarget2D* RenderTarget);
	void ApplyPreviewTransform(ABG_EquippedWeaponBase* PreviewActor, const FVector& PreviewBaseLocation) const;
	bool BuildPreviewBounds(const TArray<UPrimitiveComponent*>& PrimitiveComponents, FBox& OutBounds) const;
	float CalculateOrthoWidthForBounds(const FBox& PreviewBounds) const;
	void ConfigureCaptureCameraForBounds(const FBox& PreviewBounds);
	void DestroyPreviewEntry(FBGWeaponIconPreviewCacheEntry& Entry);
	void TrimPreviewCache();
	void DisablePreviewActorCollision(AActor* PreviewActor) const;
	int32 GetClampedRenderTargetWidth() const;
	int32 GetClampedRenderTargetHeight() const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="64", ClampMax="2048"))
	int32 RenderTargetWidth = 512;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="64", ClampMax="2048"))
	int32 RenderTargetHeight = 256;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true"))
	FLinearColor RenderTargetClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview|Transparency", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> PreviewBrushMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview|Transparency", meta=(AllowPrivateAccess="true"))
	FName PreviewTextureParameterName = TEXT("PreviewTexture");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="1"))
	int32 MaxCachedPreviews = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview|Performance", meta=(AllowPrivateAccess="true"))
	bool bPrewarmRenderTargetsOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview|Performance", meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 PrewarmedRenderTargetCount = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview|Performance", meta=(AllowPrivateAccess="true", ClampMin="0.001"))
	float RenderTargetPrewarmInterval = 0.02f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float CaptureDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float DefaultOrthoWidth = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float OrthoWidthPadding = 1.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true"))
	FVector PreviewSceneOrigin = FVector(0.f, 0.f, -100000.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Weapon Preview", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float PreviewActorSpacing = 500.f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CaptureRigActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PreviewRootComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> PreviewLightComponent = nullptr;

	UPROPERTY(Transient)
	TMap<FName, FBGWeaponIconPreviewCacheEntry> PreviewCache;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> AvailableRenderTargets;

	FTimerHandle RenderTargetPrewarmTimerHandle;

	UPROPERTY(Transient)
	int32 PreviewAccessSerial = 0;
};
