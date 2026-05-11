#include "UI/BG_WeaponIconCaptureComponent.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemDataRow.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PUBG_HotMode/PUBG_HotMode.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FRotator SideViewCaptureRotation = FRotationMatrix::MakeFromX(FVector(0.f, 1.f, 0.f)).Rotator();
	const FVector SideViewCaptureRelativeLocation(0.f, -1.f, 0.f);
	const FVector PreviewLightRelativeLocation(-120.f, -220.f, 140.f);
	const TCHAR* DefaultPreviewBrushMaterialPath =
		TEXT("/Script/Engine.Material'/Game/GEONU/Materials/M_UIPreviewBrush.M_UIPreviewBrush'");
	constexpr float PreviewLightIntensity = 8000.f;
}

UBG_WeaponIconCaptureComponent::UBG_WeaponIconCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PreviewBrushMaterialFinder(
		DefaultPreviewBrushMaterialPath);
	if (PreviewBrushMaterialFinder.Succeeded())
	{
		PreviewBrushMaterial = PreviewBrushMaterialFinder.Object;
	}
	else
	{
		LOGE(TEXT("%s: Failed to load default PreviewBrushMaterial from %s."),
			*GetNameSafe(this),
			DefaultPreviewBrushMaterialPath);
	}
}

void UBG_WeaponIconCaptureComponent::BeginPlay()
{
	Super::BeginPlay();
	StartRenderTargetPrewarm();
}

void UBG_WeaponIconCaptureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRenderTargetPrewarm();
	ClearPreviewCache();
	AvailableRenderTargets.Reset();
	DestroyCaptureRig();
	Super::EndPlay(EndPlayReason);
}

UTextureRenderTarget2D* UBG_WeaponIconCaptureComponent::GetOrCreateWeaponPreview(
	const FBGEquipmentSlotRenderData& SlotData)
{
	if (!IsCaptureAllowed(TEXT("GetOrCreateWeaponPreview")))
		return nullptr;

	TSoftClassPtr<ABG_EquippedWeaponBase> EquippedWeaponClass;
	if (!ResolveWeaponClass(SlotData, EquippedWeaponClass))
		return nullptr;

	const FName PreviewIconKey = ResolvePreviewIconKey(SlotData, EquippedWeaponClass);
	if (PreviewIconKey.IsNone())
	{
		LOGE(TEXT("%s: GetOrCreateWeaponPreview failed because PreviewIconKey was none for item %s slot %d."),
			*GetNameSafe(this),
			*SlotData.ItemTag.ToString(),
			static_cast<int32>(SlotData.Slot));
		return nullptr;
	}

	if (FBGWeaponIconPreviewCacheEntry* CachedEntry = PreviewCache.Find(PreviewIconKey))
	{
		if (CachedEntry->RenderTarget && IsValid(CachedEntry->PreviewActor))
		{
			if (!CachedEntry->BrushResource)
			{
				CachedEntry->BrushResource = BuildPreviewBrushResource(CachedEntry->RenderTarget);
			}

			CachedEntry->LastAccessSerial = ++PreviewAccessSerial;
			return CachedEntry->RenderTarget;
		}

		DestroyPreviewEntry(*CachedEntry);
		PreviewCache.Remove(PreviewIconKey);
	}

	UClass* LoadedClass = EquippedWeaponClass.LoadSynchronous();
	TSubclassOf<ABG_EquippedWeaponBase> LoadedWeaponClass = LoadedClass;
	if (!LoadedWeaponClass)
	{
		LOGE(TEXT("%s: GetOrCreateWeaponPreview failed to load EquippedWeaponClass %s for item %s."),
			*GetNameSafe(this),
			*EquippedWeaponClass.ToSoftObjectPath().ToString(),
			*SlotData.ItemTag.ToString());
		return nullptr;
	}

	if (!InitializeCaptureRig())
		return nullptr;

	UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(PreviewIconKey);
	if (!RenderTarget)
		return nullptr;

	ABG_EquippedWeaponBase* PreviewActor = SpawnPreviewActor(LoadedWeaponClass, PreviewIconKey);
	if (!PreviewActor)
		return nullptr;

	if (!CapturePreviewActor(PreviewActor, RenderTarget))
	{
		PreviewActor->Destroy();
		return nullptr;
	}

	FBGWeaponIconPreviewCacheEntry NewEntry;
	NewEntry.RenderTarget = RenderTarget;
	NewEntry.BrushResource = BuildPreviewBrushResource(RenderTarget);
	NewEntry.PreviewActor = PreviewActor;
	NewEntry.EquippedWeaponClass = LoadedWeaponClass;
	NewEntry.LastAccessSerial = ++PreviewAccessSerial;
	PreviewCache.Add(PreviewIconKey, NewEntry);
	TrimPreviewCache();

	return RenderTarget;
}

UObject* UBG_WeaponIconCaptureComponent::GetOrCreateWeaponSlotIconResource(
	const FBGEquipmentSlotRenderData& SlotData)
{
	if (!SlotData.bEquipped)
		return nullptr;

	if (SlotData.bUseRuntimePreviewIcon)
	{
		if (UTextureRenderTarget2D* RuntimePreview = GetOrCreateWeaponPreview(SlotData))
		{
			if (!SlotData.PreviewIconKey.IsNone())
			{
				if (FBGWeaponIconPreviewCacheEntry* CachedEntry = PreviewCache.Find(SlotData.PreviewIconKey))
				{
					if (!CachedEntry->BrushResource)
					{
						CachedEntry->BrushResource = BuildPreviewBrushResource(RuntimePreview);
					}

					if (CachedEntry->BrushResource)
					{
						return CachedEntry->BrushResource;
					}
				}
			}

			return RuntimePreview;
		}
	}

	if (SlotData.Icon.IsNull())
		return nullptr;

	UTexture2D* StaticIcon = SlotData.Icon.LoadSynchronous();
	if (!StaticIcon)
	{
		LOGE(TEXT("%s: GetOrCreateWeaponSlotIconResource failed to load static fallback icon %s for %s."),
			*GetNameSafe(this),
			*SlotData.Icon.ToSoftObjectPath().ToString(),
			*SlotData.ItemTag.ToString());
		return nullptr;
	}

	return StaticIcon;
}

FVector2D UBG_WeaponIconCaptureComponent::GetPreviewRenderTargetSize() const
{
	return FVector2D(
		static_cast<float>(GetClampedRenderTargetWidth()),
		static_cast<float>(GetClampedRenderTargetHeight()));
}

void UBG_WeaponIconCaptureComponent::InvalidateWeaponPreview(FName PreviewIconKey)
{
	if (PreviewIconKey.IsNone())
	{
		LOGE(TEXT("%s: InvalidateWeaponPreview failed because PreviewIconKey was none."), *GetNameSafe(this));
		return;
	}

	FBGWeaponIconPreviewCacheEntry RemovedEntry;
	if (!PreviewCache.RemoveAndCopyValue(PreviewIconKey, RemovedEntry))
	{
		LOGW(TEXT("%s: InvalidateWeaponPreview skipped because PreviewIconKey %s was not cached."),
			*GetNameSafe(this),
			*PreviewIconKey.ToString());
		return;
	}

	DestroyPreviewEntry(RemovedEntry);
}

void UBG_WeaponIconCaptureComponent::ClearPreviewCache()
{
	for (TPair<FName, FBGWeaponIconPreviewCacheEntry>& CachedPreview : PreviewCache)
	{
		DestroyPreviewEntry(CachedPreview.Value);
	}

	PreviewCache.Reset();
	PreviewAccessSerial = 0;
}

FName UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
	const FGameplayTag& ItemTag,
	EBG_EquipmentSlot Slot,
	const TSoftClassPtr<ABG_EquippedWeaponBase>& EquippedWeaponClass)
{
	if (!ItemTag.IsValid() || Slot == EBG_EquipmentSlot::None || EquippedWeaponClass.IsNull())
		return NAME_None;

	const FString KeyString = FString::Printf(
		TEXT("%s|%d|%s"),
		*ItemTag.ToString(),
		static_cast<int32>(Slot),
		*EquippedWeaponClass.ToSoftObjectPath().ToString());
	return FName(*KeyString);
}

bool UBG_WeaponIconCaptureComponent::IsCaptureAllowed(const TCHAR* OperationName) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController)
	{
		LOGE(TEXT("%s: %s failed because owner was not APlayerController. Owner=%s."),
			*GetNameSafe(this),
			OperationName,
			*GetNameSafe(GetOwner()));
		return false;
	}

	return PlayerController->IsLocalController();
}

bool UBG_WeaponIconCaptureComponent::InitializeCaptureRig()
{
	if (IsValid(CaptureRigActor)
		&& PreviewRootComponent
		&& SceneCaptureComponent
		&& PreviewLightComponent)
	{
		return true;
	}

	DestroyCaptureRig();

	UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: InitializeCaptureRig failed because World was null."), *GetNameSafe(this));
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		LOGE(TEXT("%s: InitializeCaptureRig failed because owner was null."), *GetNameSafe(this));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	CaptureRigActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		PreviewSceneOrigin,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!CaptureRigActor)
	{
		LOGE(TEXT("%s: InitializeCaptureRig failed because SpawnActor returned null."), *GetNameSafe(this));
		return false;
	}

	CaptureRigActor->SetReplicates(false);
	CaptureRigActor->SetActorEnableCollision(false);

	PreviewRootComponent = NewObject<USceneComponent>(CaptureRigActor, TEXT("WeaponIconPreviewRoot"));
	if (!PreviewRootComponent)
	{
		LOGE(TEXT("%s: InitializeCaptureRig failed because PreviewRootComponent allocation returned null."),
			*GetNameSafe(this));
		DestroyCaptureRig();
		return false;
	}

	CaptureRigActor->SetRootComponent(PreviewRootComponent);
	CaptureRigActor->AddInstanceComponent(PreviewRootComponent);
	PreviewRootComponent->RegisterComponent();

	SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(CaptureRigActor, TEXT("WeaponIconSceneCapture"));
	if (!SceneCaptureComponent)
	{
		LOGE(TEXT("%s: InitializeCaptureRig failed because SceneCaptureComponent allocation returned null."),
			*GetNameSafe(this));
		DestroyCaptureRig();
		return false;
	}

	CaptureRigActor->AddInstanceComponent(SceneCaptureComponent);
	SceneCaptureComponent->SetupAttachment(PreviewRootComponent);
	SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCaptureComponent->OrthoWidth = DefaultOrthoWidth;
	ConfigureSceneCaptureSource();
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;
	SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCaptureComponent->RegisterComponent();

	PreviewLightComponent = NewObject<UPointLightComponent>(CaptureRigActor, TEXT("WeaponIconPreviewLight"));
	if (!PreviewLightComponent)
	{
		LOGE(TEXT("%s: InitializeCaptureRig failed because PreviewLightComponent allocation returned null."),
			*GetNameSafe(this));
		DestroyCaptureRig();
		return false;
	}

	CaptureRigActor->AddInstanceComponent(PreviewLightComponent);
	PreviewLightComponent->SetupAttachment(PreviewRootComponent);
	PreviewLightComponent->SetRelativeLocation(PreviewLightRelativeLocation);
	PreviewLightComponent->SetIntensity(PreviewLightIntensity);
	PreviewLightComponent->SetCastShadows(false);
	PreviewLightComponent->RegisterComponent();

	return true;
}

void UBG_WeaponIconCaptureComponent::DestroyCaptureRig()
{
	if (IsValid(CaptureRigActor))
	{
		CaptureRigActor->Destroy();
	}

	CaptureRigActor = nullptr;
	PreviewRootComponent = nullptr;
	SceneCaptureComponent = nullptr;
	PreviewLightComponent = nullptr;
}

bool UBG_WeaponIconCaptureComponent::ResolveWeaponClass(
	const FBGEquipmentSlotRenderData& SlotData,
	TSoftClassPtr<ABG_EquippedWeaponBase>& OutEquippedWeaponClass) const
{
	OutEquippedWeaponClass.Reset();

	if (SlotData.ItemType != EBG_ItemType::Weapon)
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed because ItemType=%d was not Weapon."),
			*GetNameSafe(this),
			static_cast<int32>(SlotData.ItemType));
		return false;
	}

	if (!SlotData.bEquipped || !SlotData.ItemTag.IsValid())
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed because weapon slot data was not equipped or ItemTag was invalid. Slot=%d ItemTag=%s."),
			*GetNameSafe(this),
			static_cast<int32>(SlotData.Slot),
			*SlotData.ItemTag.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed because World was null."), *GetNameSafe(this));
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed because GameInstance was null."), *GetNameSafe(this));
		return false;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem =
		GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed because ItemDataRegistrySubsystem was null."),
			*GetNameSafe(this));
		return false;
	}

	FString FailureReason;
	const FBG_WeaponItemDataRow* WeaponRow =
		RegistrySubsystem->FindTypedItemRow<FBG_WeaponItemDataRow>(
			EBG_ItemType::Weapon,
			SlotData.ItemTag,
			&FailureReason);
	if (!WeaponRow)
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed to find weapon row. ItemTag=%s Reason=%s."),
			*GetNameSafe(this),
			*SlotData.ItemTag.ToString(),
			*FailureReason);
		return false;
	}

	if (WeaponRow->EquippedWeaponClass.IsNull())
	{
		LOGE(TEXT("%s: ResolveWeaponClass failed because weapon row %s had no EquippedWeaponClass."),
			*GetNameSafe(this),
			*SlotData.ItemTag.ToString());
		return false;
	}

	OutEquippedWeaponClass = WeaponRow->EquippedWeaponClass;
	return true;
}

FName UBG_WeaponIconCaptureComponent::ResolvePreviewIconKey(
	const FBGEquipmentSlotRenderData& SlotData,
	const TSoftClassPtr<ABG_EquippedWeaponBase>& EquippedWeaponClass) const
{
	if (!SlotData.PreviewIconKey.IsNone())
		return SlotData.PreviewIconKey;

	return MakePreviewIconKey(SlotData.ItemTag, SlotData.Slot, EquippedWeaponClass);
}

UTextureRenderTarget2D* UBG_WeaponIconCaptureComponent::CreateRenderTarget(FName PreviewIconKey)
{
	while (AvailableRenderTargets.Num() > 0)
	{
		UTextureRenderTarget2D* RenderTarget = AvailableRenderTargets.Pop(EAllowShrinking::No);
		if (RenderTarget)
			return RenderTarget;
	}

	return CreateRenderTargetObject(PreviewIconKey);
}

UTextureRenderTarget2D* UBG_WeaponIconCaptureComponent::CreateRenderTargetObject(FName PreviewIconKey)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	if (!RenderTarget)
	{
		LOGE(TEXT("%s: CreateRenderTarget failed because allocation returned null for PreviewIconKey=%s."),
			*GetNameSafe(this),
			*PreviewIconKey.ToString());
		return nullptr;
	}

	RenderTarget->RenderTargetFormat = PreviewBrushMaterial ? RTF_RGBA16f : RTF_RGBA8;
	RenderTarget->ClearColor = RenderTargetClearColor;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(GetClampedRenderTargetWidth(), GetClampedRenderTargetHeight());
	return RenderTarget;
}

void UBG_WeaponIconCaptureComponent::ReleaseRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
	if (!RenderTarget)
		return;

	if (AvailableRenderTargets.Num() >= FMath::Max(1, MaxCachedPreviews))
		return;

	AvailableRenderTargets.Add(RenderTarget);
}

void UBG_WeaponIconCaptureComponent::StartRenderTargetPrewarm()
{
	if (!bPrewarmRenderTargetsOnBeginPlay)
		return;

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
		return;

	UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: StartRenderTargetPrewarm failed because World was null."), *GetNameSafe(this));
		return;
	}

	if (RenderTargetPrewarmTimerHandle.IsValid())
		return;

	const float ClampedInterval = FMath::Max(0.001f, RenderTargetPrewarmInterval);
	World->GetTimerManager().SetTimer(
		RenderTargetPrewarmTimerHandle,
		this,
		&UBG_WeaponIconCaptureComponent::PrewarmRenderTargetStep,
		ClampedInterval,
		true,
		ClampedInterval);
}

void UBG_WeaponIconCaptureComponent::StopRenderTargetPrewarm()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		if (RenderTargetPrewarmTimerHandle.IsValid())
		{
			LOGE(TEXT("%s: StopRenderTargetPrewarm failed because World was null."), *GetNameSafe(this));
			RenderTargetPrewarmTimerHandle.Invalidate();
		}
		return;
	}

	World->GetTimerManager().ClearTimer(RenderTargetPrewarmTimerHandle);
}

void UBG_WeaponIconCaptureComponent::PrewarmRenderTargetStep()
{
	const int32 TargetPrewarmCount = FMath::Clamp(
		PrewarmedRenderTargetCount,
		0,
		FMath::Max(1, MaxCachedPreviews));
	if (AvailableRenderTargets.Num() + PreviewCache.Num() >= TargetPrewarmCount)
	{
		StopRenderTargetPrewarm();
		return;
	}

	UTextureRenderTarget2D* RenderTarget = CreateRenderTargetObject(NAME_None);
	if (!RenderTarget)
	{
		StopRenderTargetPrewarm();
		return;
	}

	AvailableRenderTargets.Add(RenderTarget);
}

UObject* UBG_WeaponIconCaptureComponent::BuildPreviewBrushResource(UTextureRenderTarget2D* RenderTarget) const
{
	if (!RenderTarget)
	{
		LOGE(TEXT("%s: BuildPreviewBrushResource failed because RenderTarget was null."), *GetNameSafe(this));
		return nullptr;
	}

	if (!PreviewBrushMaterial)
		return RenderTarget;

	UMaterialInstanceDynamic* PreviewMaterialInstance = UMaterialInstanceDynamic::Create(PreviewBrushMaterial, GetOwner());
	if (!PreviewMaterialInstance)
	{
		LOGE(TEXT("%s: BuildPreviewBrushResource failed because material instance allocation returned null. Material=%s."),
			*GetNameSafe(this),
			*GetNameSafe(PreviewBrushMaterial.Get()));
		return RenderTarget;
	}

	PreviewMaterialInstance->SetTextureParameterValue(PreviewTextureParameterName, RenderTarget);
	return PreviewMaterialInstance;
}

void UBG_WeaponIconCaptureComponent::ConfigureSceneCaptureSource() const
{
	if (!SceneCaptureComponent)
	{
		LOGE(TEXT("%s: ConfigureSceneCaptureSource failed because SceneCaptureComponent was null."), *GetNameSafe(this));
		return;
	}

	SceneCaptureComponent->CaptureSource = PreviewBrushMaterial
		? ESceneCaptureSource::SCS_SceneColorHDR
		: ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCaptureComponent->bConsiderUnrenderedOpaquePixelAsFullyTranslucent = PreviewBrushMaterial != nullptr;

	TArray<FEngineShowFlagsSetting> ShowFlagSettings;
	ShowFlagSettings.Reserve(5);

	FEngineShowFlagsSetting AtmosphereFlag;
	AtmosphereFlag.ShowFlagName = TEXT("Atmosphere");
	AtmosphereFlag.Enabled = false;
	ShowFlagSettings.Add(AtmosphereFlag);

	FEngineShowFlagsSetting CloudFlag;
	CloudFlag.ShowFlagName = TEXT("Cloud");
	CloudFlag.Enabled = false;
	ShowFlagSettings.Add(CloudFlag);

	FEngineShowFlagsSetting FogFlag;
	FogFlag.ShowFlagName = TEXT("Fog");
	FogFlag.Enabled = false;
	ShowFlagSettings.Add(FogFlag);

	FEngineShowFlagsSetting VolumetricFogFlag;
	VolumetricFogFlag.ShowFlagName = TEXT("VolumetricFog");
	VolumetricFogFlag.Enabled = false;
	ShowFlagSettings.Add(VolumetricFogFlag);

	FEngineShowFlagsSetting SkyLightingFlag;
	SkyLightingFlag.ShowFlagName = TEXT("SkyLighting");
	SkyLightingFlag.Enabled = false;
	ShowFlagSettings.Add(SkyLightingFlag);

	SceneCaptureComponent->SetShowFlagSettings(ShowFlagSettings);
}

ABG_EquippedWeaponBase* UBG_WeaponIconCaptureComponent::SpawnPreviewActor(
	TSubclassOf<ABG_EquippedWeaponBase> EquippedWeaponClass,
	FName PreviewIconKey)
{
	if (!EquippedWeaponClass)
	{
		LOGE(TEXT("%s: SpawnPreviewActor failed because EquippedWeaponClass was null for PreviewIconKey=%s."),
			*GetNameSafe(this),
			*PreviewIconKey.ToString());
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: SpawnPreviewActor failed because World was null for PreviewIconKey=%s."),
			*GetNameSafe(this),
			*PreviewIconKey.ToString());
		return nullptr;
	}

	const FVector SpawnLocation =
		PreviewSceneOrigin + FVector(PreviewActorSpacing * static_cast<float>(PreviewCache.Num()), 0.f, 0.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ABG_EquippedWeaponBase* PreviewActor = World->SpawnActor<ABG_EquippedWeaponBase>(
		EquippedWeaponClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!PreviewActor)
	{
		LOGE(TEXT("%s: SpawnPreviewActor failed because SpawnActor returned null for class %s."),
			*GetNameSafe(this),
			*GetNameSafe(EquippedWeaponClass.Get()));
		return nullptr;
	}

	PreviewActor->SetReplicates(false);
	PreviewActor->SetReplicateMovement(false);
	PreviewActor->SetActorEnableCollision(false);
	PreviewActor->SetActorHiddenInGame(false);
	ApplyPreviewTransform(PreviewActor, SpawnLocation);
	DisablePreviewActorCollision(PreviewActor);
	return PreviewActor;
}

bool UBG_WeaponIconCaptureComponent::CapturePreviewActor(
	ABG_EquippedWeaponBase* PreviewActor,
	UTextureRenderTarget2D* RenderTarget)
{
	if (!IsValid(PreviewActor))
	{
		LOGE(TEXT("%s: CapturePreviewActor failed because PreviewActor was invalid."), *GetNameSafe(this));
		return false;
	}

	if (!RenderTarget)
	{
		LOGE(TEXT("%s: CapturePreviewActor failed because RenderTarget was null."), *GetNameSafe(this));
		return false;
	}

	if (!SceneCaptureComponent)
	{
		LOGE(TEXT("%s: CapturePreviewActor failed because SceneCaptureComponent was null."), *GetNameSafe(this));
		return false;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PreviewActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.Num() == 0)
	{
		LOGE(TEXT("%s: CapturePreviewActor failed because %s had no primitive components to capture."),
			*GetNameSafe(this),
			*GetNameSafe(PreviewActor));
		return false;
	}

	SceneCaptureComponent->ClearShowOnlyComponents();
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
			continue;

		PrimitiveComponent->SetVisibility(true, true);
		PrimitiveComponent->SetHiddenInGame(false, true);
		SceneCaptureComponent->ShowOnlyComponent(PrimitiveComponent);
	}

	if (SceneCaptureComponent->ShowOnlyComponents.Num() == 0)
	{
		LOGE(TEXT("%s: CapturePreviewActor failed because %s had no valid primitive components."),
			*GetNameSafe(this),
			*GetNameSafe(PreviewActor));
		return false;
	}

	FBox PreviewBounds(ForceInit);
	if (!BuildPreviewBounds(PrimitiveComponents, PreviewBounds))
		return false;

	SceneCaptureComponent->TextureTarget = RenderTarget;
	ConfigureSceneCaptureSource();
	ConfigureCaptureCameraForBounds(PreviewBounds);
	SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;
	SceneCaptureComponent->CaptureScene();

	return true;
}

void UBG_WeaponIconCaptureComponent::ApplyPreviewTransform(
	ABG_EquippedWeaponBase* PreviewActor,
	const FVector& PreviewBaseLocation) const
{
	if (!PreviewActor)
	{
		LOGE(TEXT("%s: ApplyPreviewTransform failed because PreviewActor was null."), *GetNameSafe(this));
		return;
	}

	FTransform PreviewWorldTransform = PreviewActor->GetPreviewTransform();
	PreviewWorldTransform.SetLocation(PreviewBaseLocation + PreviewWorldTransform.GetLocation());
	PreviewActor->SetActorTransform(PreviewWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	PreviewActor->UpdateComponentTransforms();
}

bool UBG_WeaponIconCaptureComponent::BuildPreviewBounds(
	const TArray<UPrimitiveComponent*>& PrimitiveComponents,
	FBox& OutBounds) const
{
	OutBounds.Init();

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
			continue;

		const FBox ComponentBox =
			PrimitiveComponent->CalcBounds(PrimitiveComponent->GetComponentTransform()).GetBox();
		if (ComponentBox.IsValid)
		{
			OutBounds += ComponentBox;
		}
	}

	if (!OutBounds.IsValid)
	{
		LOGE(TEXT("%s: BuildPreviewBounds failed because no valid primitive bounds were available."),
			*GetNameSafe(this));
		return false;
	}

	return true;
}

float UBG_WeaponIconCaptureComponent::CalculateOrthoWidthForBounds(const FBox& PreviewBounds) const
{
	if (!PreviewBounds.IsValid)
	{
		LOGE(TEXT("%s: CalculateOrthoWidthForBounds failed because PreviewBounds was invalid."), *GetNameSafe(this));
		return FMath::Max(1.f, DefaultOrthoWidth);
	}

	const FVector BoundsSize = PreviewBounds.GetSize();
	const float RenderTargetAspectRatio =
		static_cast<float>(GetClampedRenderTargetWidth()) / static_cast<float>(GetClampedRenderTargetHeight());
	const float RequiredWidth = FMath::Max(BoundsSize.X, BoundsSize.Z * RenderTargetAspectRatio);
	if (RequiredWidth <= KINDA_SMALL_NUMBER)
	{
		LOGE(TEXT("%s: CalculateOrthoWidthForBounds failed because bounds size was too small. Size=%s"),
			*GetNameSafe(this),
			*BoundsSize.ToString());
		return FMath::Max(1.f, DefaultOrthoWidth);
	}

	return FMath::Max(1.f, RequiredWidth * FMath::Max(1.f, OrthoWidthPadding));
}

void UBG_WeaponIconCaptureComponent::ConfigureCaptureCameraForBounds(const FBox& PreviewBounds)
{
	if (!CaptureRigActor)
	{
		LOGE(TEXT("%s: ConfigureCaptureCameraForBounds failed because CaptureRigActor was null."), *GetNameSafe(this));
		return;
	}

	if (!SceneCaptureComponent)
	{
		LOGE(TEXT("%s: ConfigureCaptureCameraForBounds failed because SceneCaptureComponent was null."),
			*GetNameSafe(this));
		return;
	}

	const FVector BoundsCenter = PreviewBounds.GetCenter();
	const FVector BoundsExtent = PreviewBounds.GetExtent();
	const float ResolvedCaptureDistance = FMath::Max(CaptureDistance, BoundsExtent.Y + 50.f);

	CaptureRigActor->SetActorLocation(BoundsCenter);
	SceneCaptureComponent->SetRelativeLocation(SideViewCaptureRelativeLocation * ResolvedCaptureDistance);
	SceneCaptureComponent->SetRelativeRotation(SideViewCaptureRotation);
	SceneCaptureComponent->OrthoWidth = CalculateOrthoWidthForBounds(PreviewBounds);
}

void UBG_WeaponIconCaptureComponent::DestroyPreviewEntry(FBGWeaponIconPreviewCacheEntry& Entry)
{
	if (IsValid(Entry.PreviewActor))
	{
		Entry.PreviewActor->Destroy();
	}

	Entry.PreviewActor = nullptr;
	Entry.BrushResource = nullptr;
	ReleaseRenderTarget(Entry.RenderTarget);
	Entry.RenderTarget = nullptr;
	Entry.EquippedWeaponClass = nullptr;
	Entry.LastAccessSerial = 0;
}

void UBG_WeaponIconCaptureComponent::TrimPreviewCache()
{
	const int32 ClampedMaxCachedPreviews = FMath::Max(1, MaxCachedPreviews);
	while (PreviewCache.Num() > ClampedMaxCachedPreviews)
	{
		FName OldestKey;
		int32 OldestAccessSerial = TNumericLimits<int32>::Max();

		for (const TPair<FName, FBGWeaponIconPreviewCacheEntry>& CachedPreview : PreviewCache)
		{
			if (CachedPreview.Value.LastAccessSerial < OldestAccessSerial)
			{
				OldestKey = CachedPreview.Key;
				OldestAccessSerial = CachedPreview.Value.LastAccessSerial;
			}
		}

		if (OldestKey.IsNone())
			return;

		InvalidateWeaponPreview(OldestKey);
	}
}

void UBG_WeaponIconCaptureComponent::DisablePreviewActorCollision(AActor* PreviewActor) const
{
	if (!PreviewActor)
	{
		LOGE(TEXT("%s: DisablePreviewActorCollision failed because PreviewActor was null."), *GetNameSafe(this));
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PreviewActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
			continue;

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		PrimitiveComponent->CanCharacterStepUpOn = ECB_No;
	}
}

int32 UBG_WeaponIconCaptureComponent::GetClampedRenderTargetWidth() const
{
	return FMath::Clamp(RenderTargetWidth, 64, 2048);
}

int32 UBG_WeaponIconCaptureComponent::GetClampedRenderTargetHeight() const
{
	return FMath::Clamp(RenderTargetHeight, 64, 2048);
}
