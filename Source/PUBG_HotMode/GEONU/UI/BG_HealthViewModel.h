// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_ItemTypes.h"
#include "BG_HealthViewModel.generated.h"

// ── Shared delegate types ──────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGActiveWeaponAmmoRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo")
	bool bHasActiveGun = false;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo")
	EBG_EquipmentSlot ActiveWeaponSlot = EBG_EquipmentSlot::None;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(Categories="Item.Weapon"))
	FGameplayTag ActiveWeaponItemTag;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(Categories="Item.Ammo"))
	FGameplayTag AmmoItemTag;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(ClampMin="0"))
	int32 LoadedAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(ClampMin="0"))
	int32 MagazineSize = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo", meta=(ClampMin="0"))
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Ammo")
	bool bUsesInfiniteDebugAmmo = false;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGInventoryWeightRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="HUD|Inventory", meta=(ClampMin="0.0"))
	float CurrentWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Inventory", meta=(ClampMin="0.0"))
	float MaxWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Inventory", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FillPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="HUD|Inventory")
	bool bHasCapacity = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBGHUD_FloatBoolValueChanged, float, Value, bool, bValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_FloatValueChanged, float, Value);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_IntValueChanged, int32, Value);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_BoolValueChanged, bool, bValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_ActiveWeaponAmmoChanged, FBGActiveWeaponAmmoRenderData, AmmoData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_InventoryWeightChanged, FBGInventoryWeightRenderData, WeightData);

/**
 * Data bridge for widgets, providing delegates for each attribute.
 * Attach to PlayerController to subscribe the possessed character's HealthComponent.
 */
UCLASS(ClassGroup="UI", meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_HealthViewModel : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_HealthViewModel();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="HUD")
	void NotifyPossessedCharacterReady(class ABG_Character* InCharacter);

	UFUNCTION(BlueprintCallable, Category="HUD")
	void NotifyPossessedCharacterCleared();

public: // --- Per-attribute delegates ---
	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatBoolValueChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatValueChanged OnBoostChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatBoolValueChanged OnDBNOChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_IntValueChanged OnHelmetChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_IntValueChanged OnVestChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_IntValueChanged OnBackpackChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_BoolValueChanged OnBoostRunChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_BoolValueChanged OnRegenerationChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatBoolValueChanged OnBreathChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_ActiveWeaponAmmoChanged OnActiveWeaponAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_InventoryWeightChanged OnInventoryWeightChanged;

	UFUNCTION(BlueprintCallable, Category="HUD")
	void ForceUpdateAllAttributes();

public: // --- Getters (for initial values) ---
	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE void GetHealthData(float& OutHealthPercent, bool& bOutIsDead) const
	{
		OutHealthPercent = HealthPercent;
		bOutIsDead = bIsDead;
	}

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE float GetBoostPercent() const { return BoostPercent; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE void GetDBNOData(float& OutDBNOPercent, bool& bOutIsDBNO) const
	{
		OutDBNOPercent = DBNOPercent;
		bOutIsDBNO = bIsDBNO;
	}

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE int32 GetHelmetLevel() const { return HelmetLevel; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE int32 GetVestLevel() const { return VestLevel; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE int32 GetBackpackLevel() const { return BackpackLevel; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE bool GetHasBoostRun() const { return bHasBoostRun; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE bool GetHasRegeneration() const { return bHasRegeneration; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE void GetBreathData(float& OutBreathPercent, bool& bOutShouldShowBreath) const
	{
		OutBreathPercent = BreathPercent;
		bOutShouldShowBreath = bShouldShowBreath;
	}

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE FBGActiveWeaponAmmoRenderData GetActiveWeaponAmmoData() const { return ActiveWeaponAmmoData; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE FBGInventoryWeightRenderData GetInventoryWeightData() const { return InventoryWeightData; }

private: // --- State binding ---
	void BindCharacter(class ABG_Character* InCharacter);
	void Unbind();
	void Refresh();
	void RefreshEquipmentLevels();
	void RefreshActiveWeaponAmmoData();
	void RefreshInventoryWeightData();
	void ResetActiveWeaponAmmoData();
	FBGActiveWeaponAmmoRenderData BuildActiveWeaponAmmoData() const;
	FBGInventoryWeightRenderData BuildInventoryWeightData() const;
	int32 ResolveEquipmentLevel(EBG_EquipmentSlot EquipmentSlot) const;

	UFUNCTION()
	void ReceiveHealthChanged(float NewHealth, float MaxHealth, bool bNewDead);

	UFUNCTION()
	void ReceiveBoostChanged(float NewBoostGauge);

	UFUNCTION()
	void ReceiveInventoryChanged();

	UFUNCTION()
	void ReceiveInventoryItemQuantityChanged(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity);

	UFUNCTION()
	void ReceiveInventoryWeightChanged(float NewCurrentWeight, float NewMaxWeight);

	UFUNCTION()
	void ReceiveEquipmentChanged();

	UFUNCTION()
	void ReceiveActiveWeaponSlotChanged(EBG_EquipmentSlot ActiveWeaponSlot);

	UFUNCTION()
	void ReceiveWeaponAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo);

private: // --- Cached attribute values ---
	// Health
	float HealthPercent = 0.f;
	bool bIsDead = false;

	// Boost
	float BoostPercent = 0.f;

	// DBNO
	float DBNOPercent = 0.f;
	bool bIsDBNO = false;

	// Armor
	int32 HelmetLevel = 0;
	int32 VestLevel = 0;
	int32 BackpackLevel = 0;

	// Status effects
	bool bHasBoostRun = false;
	bool bHasRegeneration = false;

	// Breath
	float BreathPercent = 1.f;
	bool bShouldShowBreath = false;

	// Ammo
	FBGActiveWeaponAmmoRenderData ActiveWeaponAmmoData;

	// Inventory weight
	FBGInventoryWeightRenderData InventoryWeightData;

	UPROPERTY()
	TWeakObjectPtr<class UBG_HealthComponent> BoundHealthComponent;

	UPROPERTY()
	TWeakObjectPtr<class UBG_InventoryComponent> BoundInventoryComponent;

	UPROPERTY()
	TWeakObjectPtr<class UBG_EquipmentComponent> BoundEquipmentComponent;

	UPROPERTY()
	TWeakObjectPtr<class UBG_WeaponFireComponent> BoundWeaponFireComponent;
};
