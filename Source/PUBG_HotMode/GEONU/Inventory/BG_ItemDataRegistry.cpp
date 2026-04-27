// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_ItemDataRegistry.h"

#include "PUBG_HotMode/PUBG_HotMode.h"

// --- Utility functions for consistent error reporting and data validation ---
namespace
{
	// config 검색, asset 파일명, C++ id가 같은 대상을 가리키도록 묶는 데이터 파이프라인 계약
	constexpr const TCHAR* ItemDataRegistryTypeName = TEXT("BG_ItemDataRegistry");
	constexpr const TCHAR* DefaultItemDataRegistryAssetName = TEXT("DA_ItemDataRegistry");

	FString GetItemTypeName(EBG_ItemType ItemType)
	{
		const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
		return ItemTypeEnum ? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(ItemType)) : TEXT("<Unknown>");
	}

	bool SetRegistryFailure(FString* OutFailureReason, const FString& FailureReason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}

		// 호출자가 bool 결과만 보더라도 데이터 문제는 로그에 반드시 남아야 한다.
		LOGE(TEXT("%s"), *FailureReason);
		return false;
	}
}

// --- Primary Asset ---

FPrimaryAssetType UBG_ItemDataRegistry::GetRegistryPrimaryAssetType()
{
	return FPrimaryAssetType(ItemDataRegistryTypeName);
}

FName UBG_ItemDataRegistry::GetDefaultRegistryAssetName()
{
	return FName(DefaultItemDataRegistryAssetName);
}

FPrimaryAssetId UBG_ItemDataRegistry::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetRegistryPrimaryAssetType(), GetFName());
}

// --- Table Access ---

UDataTable* UBG_ItemDataRegistry::GetDataTableForType(EBG_ItemType ItemType) const
{
	switch (ItemType)
	{
	case EBG_ItemType::Ammo:
		return AmmoItems.Get();
	case EBG_ItemType::Heal:
		return HealItems.Get();
	case EBG_ItemType::Boost:
		return BoostItems.Get();
	case EBG_ItemType::Throwable:
		return ThrowableItems.Get();
	case EBG_ItemType::Weapon:
		return WeaponItems.Get();
	case EBG_ItemType::Armor:
		return ArmorItems.Get();
	case EBG_ItemType::Backpack:
		return BackpackItems.Get();
	default:
		return nullptr;
	}
}

// --- Item Data Queries ---

bool UBG_ItemDataRegistry::HasValidItemRow(EBG_ItemType ItemType, FGameplayTag ItemTag) const
{
	return FindItemRow(ItemType, ItemTag) != nullptr;
}

bool UBG_ItemDataRegistry::HasValidItemRowByName(EBG_ItemType ItemType, FName ItemTagName) const
{
	if (ItemTagName.IsNone())
	{
		SetRegistryFailure(
			nullptr,
			FString::Printf(TEXT("%s received an empty item tag name for item type %s."),
			                *GetNameSafe(this),
			                *GetItemTypeName(ItemType)));
		return false;
	}

	const FGameplayTag ItemTag = FGameplayTag::RequestGameplayTag(ItemTagName, false);
	if (!ItemTag.IsValid())
	{
		SetRegistryFailure(
			nullptr,
			FString::Printf(TEXT("%s could not find gameplay tag %s for item type %s."),
			                *GetNameSafe(this),
			                *ItemTagName.ToString(),
			                *GetItemTypeName(ItemType)));
		return false;
	}

	return HasValidItemRow(ItemType, ItemTag);
}

const FBG_ItemDataRow* UBG_ItemDataRegistry::FindItemRow(
	EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	FString* OutFailureReason) const
{
	UDataTable* ItemTable = nullptr;
	if (!ValidateTableForType(ItemType, ItemTable, OutFailureReason))
	{
		return nullptr;
	}

	if (!ItemTag.IsValid())
	{
		SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s received an invalid item tag for item type %s."),
			                *GetNameSafe(this),
			                *GetItemTypeName(ItemType)));
		return nullptr;
	}

	const FName RowName = ItemTag.GetTagName();
	// RowName = ItemTag
	const uint8* RowData = ItemTable->FindRowUnchecked(RowName);
	if (!RowData)
	{
		SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s is missing row %s in table %s for item type %s."),
			                *GetNameSafe(this),
			                *RowName.ToString(),
			                *GetNameSafe(ItemTable),
			                *GetItemTypeName(ItemType)));
		return nullptr;
	}

	// DataTable은 파생 Row struct의 바이트를 보관하므로, 위의 table 검증이 base view의 안전 조건
	const FBG_ItemDataRow* ItemRow = reinterpret_cast<const FBG_ItemDataRow*>(RowData);
	if (!ValidateItemRowData(ItemType, RowName, *ItemRow, OutFailureReason))
	{
		return nullptr;
	}

	return ItemRow;
}

// --- Validation ---

bool UBG_ItemDataRegistry::ValidateRegistry() const
{
	static const EBG_ItemType ItemTypesToValidate[] =
	{
		EBG_ItemType::Ammo,
		EBG_ItemType::Heal,
		EBG_ItemType::Boost,
		EBG_ItemType::Throwable,
		EBG_ItemType::Weapon,
		EBG_ItemType::Armor,
		EBG_ItemType::Backpack,
	};

	bool bIsValid = true;
	for (const EBG_ItemType ItemType : ItemTypesToValidate)
	{
		// 시작 시점에 모든 table을 훑는다.
		UDataTable* ItemTable = nullptr;
		if (!ValidateTableForType(ItemType, ItemTable))
		{
			bIsValid = false;
			continue;
		}

		for (const TPair<FName, uint8*>& RowPair : ItemTable->GetRowMap())
		{
			if (!RowPair.Value)
			{
				SetRegistryFailure(
					nullptr,
					FString::Printf(TEXT("%s contains a null row pointer for row %s in table %s."),
					                *GetNameSafe(this),
					                *RowPair.Key.ToString(),
					                *GetNameSafe(ItemTable)));
				bIsValid = false;
				continue;
			}

			const FBG_ItemDataRow* ItemRow = reinterpret_cast<const FBG_ItemDataRow*>(RowPair.Value);
			if (!ValidateItemRowData(ItemType, RowPair.Key, *ItemRow))
			{
				bIsValid = false;
			}
		}
	}

	return bIsValid;
}

// --- Internal Helpers ---

const UScriptStruct* UBG_ItemDataRegistry::GetExpectedRowStructForType(EBG_ItemType ItemType) const
{
	switch (ItemType)
	{
	case EBG_ItemType::Ammo:
		return FBG_AmmoItemDataRow::StaticStruct();
	case EBG_ItemType::Heal:
		return FBG_HealItemDataRow::StaticStruct();
	case EBG_ItemType::Boost:
		return FBG_BoostItemDataRow::StaticStruct();
	case EBG_ItemType::Throwable:
		return FBG_ThrowableItemDataRow::StaticStruct();
	case EBG_ItemType::Weapon:
		return FBG_WeaponItemDataRow::StaticStruct();
	case EBG_ItemType::Armor:
		return FBG_ArmorItemDataRow::StaticStruct();
	case EBG_ItemType::Backpack:
		return FBG_BackpackItemDataRow::StaticStruct();
	default:
		return nullptr;
	}
}

bool UBG_ItemDataRegistry::ValidateTableForType(
	EBG_ItemType ItemType, UDataTable*& OutDataTable,
	FString* OutFailureReason) const
{
	OutDataTable = nullptr;

	const UScriptStruct* ExpectedRowStruct = GetExpectedRowStructForType(ItemType);
	if (!ExpectedRowStruct)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s does not support item type %s."),
			                *GetNameSafe(this),
			                *GetItemTypeName(ItemType)));
	}

	UDataTable* ItemTable = GetDataTableForType(ItemType);
	if (!ItemTable)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s has no DataTable assigned for item type %s."),
			                *GetNameSafe(this),
			                *GetItemTypeName(ItemType)));
	}

	const UScriptStruct* ActualRowStruct = ItemTable->GetRowStruct();
	if (!ActualRowStruct)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("DataTable %s has no row struct for item type %s."),
			                *GetNameSafe(ItemTable),
			                *GetItemTypeName(ItemType)));
	}

	if (!ActualRowStruct->IsChildOf(ExpectedRowStruct))
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("DataTable %s uses row struct %s, expected %s for item type %s."),
			                *GetNameSafe(ItemTable),
			                *GetNameSafe(ActualRowStruct),
			                *GetNameSafe(ExpectedRowStruct),
			                *GetItemTypeName(ItemType)));
	}

	OutDataTable = ItemTable;
	return true;
}

bool UBG_ItemDataRegistry::ValidateTypedRowStruct(
	EBG_ItemType ItemType, const UScriptStruct* RequestedRowStruct,
	FString* OutFailureReason) const
{
	if (!RequestedRowStruct || !RequestedRowStruct->IsChildOf(FBG_ItemDataRow::StaticStruct()))
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s received an invalid typed row struct for item type %s."),
			                *GetNameSafe(this),
			                *GetItemTypeName(ItemType)));
	}

	UDataTable* ItemTable = nullptr;
	if (!ValidateTableForType(ItemType, ItemTable, OutFailureReason))
	{
		return false;
	}

	const UScriptStruct* ActualRowStruct = ItemTable->GetRowStruct();
	if (!ActualRowStruct || !ActualRowStruct->IsChildOf(RequestedRowStruct))
	{
		// template 호출자가 유효한 Row를 잘못된 파생 타입으로 읽는 실수를 막는다.
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Typed lookup requested row struct %s for item type %s, but table %s uses %s."),
			                *GetNameSafe(RequestedRowStruct),
			                *GetItemTypeName(ItemType),
			                *GetNameSafe(ItemTable),
			                *GetNameSafe(ActualRowStruct)));
	}

	return true;
}

bool UBG_ItemDataRegistry::ValidateItemRowData(
	EBG_ItemType ExpectedItemType, FName RowName,
	const FBG_ItemDataRow& ItemRow, FString* OutFailureReason) const
{
	if (ItemRow.ItemType != ExpectedItemType)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Item row %s has item type %s, expected %s."),
			                *RowName.ToString(),
			                *GetItemTypeName(ItemRow.ItemType),
			                *GetItemTypeName(ExpectedItemType)));
	}

	if (!ItemRow.ItemTag.IsValid())
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Item row %s has an invalid ItemTag."),
			                *RowName.ToString()));
	}

	if (RowName != ItemRow.ItemTag.GetTagName())
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Item row %s has ItemTag %s; RowName and ItemTag must match."),
			                *RowName.ToString(),
			                *ItemRow.ItemTag.ToString()));
	}

	return true;
}
