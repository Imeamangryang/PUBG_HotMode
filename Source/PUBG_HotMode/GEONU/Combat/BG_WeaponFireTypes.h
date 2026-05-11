// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "BG_WeaponFireTypes.generated.h"

UENUM(BlueprintType)
enum class EBG_WeaponFireMode : uint8
{
	None UMETA(DisplayName="None"),
	SemiAuto UMETA(DisplayName="Semi Auto"),
	FullAuto UMETA(DisplayName="Full Auto"),
	BoltAction UMETA(DisplayName="Bolt Action"),
	Melee UMETA(DisplayName="Melee")
};

UENUM(BlueprintType)
enum class EBG_WeaponFireImplementation : uint8
{
	None UMETA(DisplayName="None"),
	Projectile UMETA(DisplayName="Projectile"),
	Hitscan UMETA(DisplayName="Hitscan"),
	Melee UMETA(DisplayName="Melee")
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_WeaponFireSpecRow : public FTableRowBase
{
	GENERATED_BODY()

	/// Weapon item tag; must match with the DataTable row name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(Categories="Item.Weapon"))
	FGameplayTag WeaponItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire")
	EBG_WeaponFireMode FireMode = EBG_WeaponFireMode::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire")
	EBG_WeaponFireImplementation FireImplementation = EBG_WeaponFireImplementation::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(ClampMin="0.0"))
	float Damage = 0.f;

	/// Effective fire range in centimeters
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(ClampMin="0.0"))
	float Range = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(ClampMin="0.0"))
	float FireCooldown = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(ClampMin="0"))
	int32 AmmoCost = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(ClampMin="1"))
	int32 PelletCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire", meta=(ClampMin="0.0"))
	float SpreadAngleDegrees = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/// Server projectile actor class. The concrete projectile base is introduced in a later implementation step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Projectile")
	TSoftClassPtr<AActor> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Projectile", meta=(ClampMin="0.0"))
	float ProjectileInitialSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Projectile", meta=(ClampMin="0.0"))
	float ProjectileGravityScale = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Throw")
	bool bCanThrow = false;

	/// Server throw projectile actor class. The concrete projectile base is introduced in a later implementation step.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Throw")
	TSoftClassPtr<AActor> ThrowProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Throw", meta=(ClampMin="0.0"))
	float ThrowMaxDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Throw", meta=(ClampMin="0.0"))
	float ThrowDamageMax = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Throw", meta=(ClampMin="0.0"))
	float ThrowDamageMin = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Throw", meta=(ClampMin="0.0"))
	float ThrowDamageFalloffStartDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon Fire|Debug", meta=(ClampMin="0.0"))
	float DebugDrawDuration = 0.f;
};
