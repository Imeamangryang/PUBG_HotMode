// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BG_ItemDataRegistry.h"
#include "BG_ItemDataRegistrySubsystem.generated.h"

/// Provide cached registry access
UCLASS()
class PUBG_HOTMODE_API UBG_ItemDataRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	/// Load registry on startup
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/// Clear registry cache
	virtual void Deinitialize() override;

public: // --- Data Access ---
	/// Load and validate registry
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool LoadRegistry();

	/// Return cached registry
	UFUNCTION(BlueprintPure, Category="Item Data")
	UBG_ItemDataRegistry* GetRegistry() const;

	/// Report cache state
	UFUNCTION(BlueprintPure, Category="Item Data")
	bool IsRegistryLoaded() const;

	/// Recheck cached registry content
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool ValidateRegistry() const;

	/// Recheck cached weapon fire specs
	UFUNCTION(BlueprintCallable, Category="Item Data|Weapon Fire")
	bool ValidateWeaponFireSpecs() const;

public: // --- Item Data Queries ---
	/// Check row by GameplayTag
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool HasValidItemRow(EBG_ItemType ItemType, FGameplayTag ItemTag);

	/// Check row by tag name
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool HasValidItemRowByName(EBG_ItemType ItemType, FName ItemTagName);

	/// Find validated base row
	const FBG_ItemDataRow* FindItemRow(
		EBG_ItemType ItemType, const FGameplayTag& ItemTag,
		FString* OutFailureReason = nullptr);

	/// Check weapon fire spec row by GameplayTag
	UFUNCTION(BlueprintCallable, Category="Item Data|Weapon Fire")
	bool HasValidWeaponFireSpecRow(FGameplayTag WeaponItemTag);

	/// Find validated weapon fire spec row
	const FBG_WeaponFireSpecRow* FindWeaponFireSpecRow(
		const FGameplayTag& WeaponItemTag,
		FString* OutFailureReason = nullptr);

	/// Find validated typed row
	template <typename RowType>
	const RowType* FindTypedItemRow(
		EBG_ItemType ItemType, const FGameplayTag& ItemTag,
		FString* OutFailureReason = nullptr)
	{
		static_assert(TIsDerivedFrom<RowType, FBG_ItemDataRow>::IsDerived, "RowType must derive from FBG_ItemDataRow.");

		const UBG_ItemDataRegistry* Registry = GetLoadedRegistry(OutFailureReason);
		return Registry ? Registry->FindTypedItemRow<RowType>(ItemType, ItemTag, OutFailureReason) : nullptr;
	}

private: // --- Internal Helpers ---
	/// Get or load registry
	UBG_ItemDataRegistry* GetLoadedRegistry(FString* OutFailureReason = nullptr);

	/// Load registry through AssetManager
	UBG_ItemDataRegistry* LoadRegistryFromAssetManager() const;

	/// Resolve curated registry id
	bool TryResolveRegistryPrimaryAssetId(FPrimaryAssetId& OutRegistryId) const;

private: // --- Cached Data ---
	UPROPERTY(Transient)
	TObjectPtr<UBG_ItemDataRegistry> CachedRegistry;
};
