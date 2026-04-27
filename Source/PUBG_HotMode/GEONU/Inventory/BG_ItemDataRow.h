// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"
#include "BG_ItemTypes.h"
#include "BG_ItemDataRow.generated.h"

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_ItemDataRow : public FTableRowBase
{
	GENERATED_BODY()

	/// item table category
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	EBG_ItemType ItemType = EBG_ItemType::None;

	/// ID; must match with the DataTable row name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	/// Display name for UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	/// Description for UI tooltip
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText Description;

	/// UI icon data
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Display")
	TSoftObjectPtr<UTexture2D> Icon;

	/// Actor display mesh data
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Display")
	TSoftObjectPtr<UStaticMesh> WorldStaticMesh;

	/// Unit weight of an item
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Inventory", meta=(ClampMin="0.0"))
	float UnitWeight = 0.f;

	/// Determines if the same item can be combined into a single inventory entry
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Inventory")
	bool bStackable = false;

	/// Maximum number of items that can be contained in a single inventory stack entry
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Inventory", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_AmmoItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_HealItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()

	/// Amount of health restored by this item
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heal", meta=(ClampMin="0.0"))
	float HealAmount = 0.f;

	/// Maximum health that can be restored by this item
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heal", meta=(ClampMin="0.0"))
	float HealCap = 100.f;

	/// Time to wait while using the item
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heal", meta=(ClampMin="0.0"))
	float UseDuration = 0.f;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_BoostItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()

	/// Amount of Boost Gauge addition by this item
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boost", meta=(ClampMin="0.0"))
	float BoostAmount = 0.f;

	/// Time to wait while using the item
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boost", meta=(ClampMin="0.0"))
	float UseDuration = 0.f;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_ThrowableItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()

	/// Time to spend preparing the throw
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throwable", meta=(ClampMin="0.0"))
	float UseDuration = 0.f;

	/// Delay time before the actual effect is triggered after the throw is confirmed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Throwable", meta=(ClampMin="0.0"))
	float FuseDuration = 0.f;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_WeaponItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()

	/// Weapon equip slot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	EBG_WeaponEquipSlot EquipSlot = EBG_WeaponEquipSlot::None;

	/// Weapon pose category used by the equipment adapter to align the character's posture
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon")
	EBG_WeaponPoseCategory WeaponPoseCategory = EBG_WeaponPoseCategory::None;

	/// Ammo item tag consumed by reload
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon", meta=(Categories="Item.Ammo"))
	FGameplayTag AmmoItemTag;

	/// Maximum loaded ammo this weapon can hold
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0"))
	int32 MagazineSize = 0;

	/// Reload time before confirming ammo consumption and reload
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.0"))
	float ReloadDuration = 0.f;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_BackpackItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()

	/// Backpack tier
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Backpack", meta=(ClampMin="0"))
	int32 EquipmentLevel = 0;

	/// Additional carrying capacity granted while equipped
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Backpack", meta=(ClampMin="0.0"))
	float WeightBonus = 0.f;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_ArmorItemDataRow : public FBG_ItemDataRow
{
	GENERATED_BODY()

	/// Armor slot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor")
	EBG_ArmorEquipSlot EquipSlot = EBG_ArmorEquipSlot::None;

	/// Armor tier
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor", meta=(ClampMin="0"))
	int32 EquipmentLevel = 0;

	/// Maximum durability of the armor when equipped
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor", meta=(ClampMin="0.0"))
	float MaxDurability = 0.f;

	/// Damage reduction applied when the armor is valid
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Armor", meta=(ClampMin="0.0"))
	float DamageReduction = 0.f;
};
