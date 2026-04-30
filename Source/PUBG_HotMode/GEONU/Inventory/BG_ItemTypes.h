// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BG_ItemTypes.generated.h"

UENUM(BlueprintType)
enum class EBG_ItemType : uint8
{
	None UMETA(DisplayName="None"),
	Ammo UMETA(DisplayName="Ammo"),
	Heal UMETA(DisplayName="Heal"),
	Boost UMETA(DisplayName="Boost"),
	Throwable UMETA(DisplayName="Throwable"),
	Weapon UMETA(DisplayName="Weapon"),
	Armor UMETA(DisplayName="Armor"),
	Backpack UMETA(DisplayName="Backpack")
};

UENUM(BlueprintType)
enum class EBG_WeaponEquipSlot : uint8
{
	None UMETA(DisplayName="None"),
	Primary UMETA(DisplayName="Primary"),
	Pistol UMETA(DisplayName="Pistol"),
	Melee UMETA(DisplayName="Melee")
};

UENUM(BlueprintType)
enum class EBG_WeaponPoseCategory : uint8
{
	None UMETA(DisplayName="None"),
	Pistol UMETA(DisplayName="Pistol"),
	Rifle UMETA(DisplayName="Rifle"),
	Shotgun UMETA(DisplayName="Shotgun")
};

UENUM(BlueprintType)
enum class EBG_ArmorEquipSlot : uint8
{
	None UMETA(DisplayName="None"),
	Helmet UMETA(DisplayName="Helmet"),
	Vest UMETA(DisplayName="Vest")
};

UENUM(BlueprintType)
enum class EBGInventoryFailReason : uint8
{
	None UMETA(DisplayName="None"),
	InvalidItem UMETA(DisplayName="Invalid Item"),
	InvalidQuantity UMETA(DisplayName="Invalid Quantity"),
	TooFar UMETA(DisplayName="Too Far"),
	Overweight UMETA(DisplayName="Overweight"),
	SlotMismatch UMETA(DisplayName="Slot Mismatch"),
	MissingAmmo UMETA(DisplayName="Missing Ammo"),
	AlreadyUsingItem UMETA(DisplayName="Already Using Item"),
	HealthCapReached UMETA(DisplayName="Health Cap Reached"),
	ServerRejected UMETA(DisplayName="Server Rejected")
};
