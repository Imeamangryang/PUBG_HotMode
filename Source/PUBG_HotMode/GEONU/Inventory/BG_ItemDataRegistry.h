// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BG_ItemDataRow.h"
#include "BG_ItemDataRegistry.generated.h"

/// Own editable item tables and validated lookup contracts
UCLASS(BlueprintType)
class PUBG_HOTMODE_API UBG_ItemDataRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public: // --- Primary Asset ---
	/// Return registry primary asset type
	static FPrimaryAssetType GetRegistryPrimaryAssetType();

	/// Return default registry asset name
	static FName GetDefaultRegistryAssetName();

	/// Bind native asset to registry type
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public: // --- Table Access ---
	/// Select table by item type
	UFUNCTION(BlueprintPure, Category="Item Data")
	UDataTable* GetDataTableForType(EBG_ItemType ItemType) const;

public: // --- Item Data Queries ---
	/// Check row by GameplayTag
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool HasValidItemRow(EBG_ItemType ItemType, FGameplayTag ItemTag) const;

	/// Check row by tag name
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool HasValidItemRowByName(EBG_ItemType ItemType, FName ItemTagName) const;

	/// Find validated base row
	const FBG_ItemDataRow* FindItemRow(
		EBG_ItemType ItemType, const FGameplayTag& ItemTag,
		FString* OutFailureReason = nullptr) const;

	/// Find validated typed row
	template <typename RowType>
	const RowType* FindTypedItemRow(
		EBG_ItemType ItemType, const FGameplayTag& ItemTag,
		FString* OutFailureReason = nullptr) const
	{
		static_assert(TIsDerivedFrom<RowType, FBG_ItemDataRow>::IsDerived, "RowType must derive from FBG_ItemDataRow.");

		// 아래 cast는 선택된 table이 요청한 Row 계열을 담고 있음이 확인된 뒤에만 안전
		if (!ValidateTypedRowStruct(ItemType, RowType::StaticStruct(), OutFailureReason))
		{
			return nullptr;
		}

		const FBG_ItemDataRow* ItemRow = FindItemRow(ItemType, ItemTag, OutFailureReason);
		return ItemRow ? static_cast<const RowType*>(ItemRow) : nullptr;
	}

public: // --- Validation ---
	/// Validate all registry content
	UFUNCTION(BlueprintCallable, Category="Item Data")
	bool ValidateRegistry() const;

public: // --- Data Tables ---
	/// Ammo: stack size, carry weight, reload cost
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> AmmoItems;

	/// Heal: heal amount, use time
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> HealItems;

	/// Boost: boost amount, use time
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> BoostItems;

	/// Throwable: tag selection, inventory-managed quantity
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> ThrowableItems;

	/// Weapon: equipment slot and ammo compatibility
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> WeaponItems;

	/// Armor: durability and damage reduction contracts
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> ArmorItems;

	/// Backpack: extends inventory capacity, not part of regular stack list
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Data")
	TObjectPtr<UDataTable> BackpackItems;

private: // --- Internal Helpers ---
	/// Resolve expected row struct
	const UScriptStruct* GetExpectedRowStructForType(EBG_ItemType ItemType) const;

	/// Validate table assignment and row struct
	bool ValidateTableForType(
		EBG_ItemType ItemType, UDataTable*& OutDataTable,
		FString* OutFailureReason = nullptr) const;

	/// Guard typed row casts
	bool ValidateTypedRowStruct(
		EBG_ItemType ItemType, const UScriptStruct* RequestedRowStruct,
		FString* OutFailureReason = nullptr) const;

	/// Validate row invariants
	bool ValidateItemRowData(
		EBG_ItemType ExpectedItemType, FName RowName, const FBG_ItemDataRow& ItemRow,
		FString* OutFailureReason = nullptr) const;
};
