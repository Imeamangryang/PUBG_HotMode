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

UDataTable* UBG_ItemDataRegistry::GetWeaponFireSpecTable() const
{
	return WeaponFireSpecs.Get();
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
		return nullptr;

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
		return nullptr;

	return ItemRow;
}

bool UBG_ItemDataRegistry::HasValidWeaponFireSpecRow(FGameplayTag WeaponItemTag) const
{
	return FindWeaponFireSpecRow(WeaponItemTag) != nullptr;
}

const FBG_WeaponFireSpecRow* UBG_ItemDataRegistry::FindWeaponFireSpecRow(
	const FGameplayTag& WeaponItemTag,
	FString* OutFailureReason) const
{
	UDataTable* FireSpecTable = nullptr;
	if (!ValidateWeaponFireSpecTable(FireSpecTable, OutFailureReason))
		return nullptr;

	if (!WeaponItemTag.IsValid())
	{
		SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s received an invalid weapon item tag for fire spec lookup."),
			                *GetNameSafe(this)));
		return nullptr;
	}

	const FName RowName = WeaponItemTag.GetTagName();
	const uint8* RowData = FireSpecTable->FindRowUnchecked(RowName);
	if (!RowData)
	{
		SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s is missing weapon fire spec row %s in table %s."),
			                *GetNameSafe(this),
			                *RowName.ToString(),
			                *GetNameSafe(FireSpecTable)));
		return nullptr;
	}

	const FBG_WeaponFireSpecRow* FireSpecRow = reinterpret_cast<const FBG_WeaponFireSpecRow*>(RowData);
	if (!ValidateWeaponFireSpecRowData(RowName, *FireSpecRow, OutFailureReason))
		return nullptr;

	return FireSpecRow;
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

	if (WeaponFireSpecs)
	{
		bIsValid = ValidateWeaponFireSpecs() && bIsValid;
	}

	return bIsValid;
}

bool UBG_ItemDataRegistry::ValidateWeaponFireSpecs() const
{
	UDataTable* FireSpecTable = nullptr;
	if (!ValidateWeaponFireSpecTable(FireSpecTable))
		return false;

	bool bIsValid = true;
	for (const TPair<FName, uint8*>& RowPair : FireSpecTable->GetRowMap())
	{
		if (!RowPair.Value)
		{
			SetRegistryFailure(
				nullptr,
				FString::Printf(TEXT("%s contains a null weapon fire spec row pointer for row %s in table %s."),
				                *GetNameSafe(this),
				                *RowPair.Key.ToString(),
				                *GetNameSafe(FireSpecTable)));
			bIsValid = false;
			continue;
		}

		const FBG_WeaponFireSpecRow* FireSpecRow =
			reinterpret_cast<const FBG_WeaponFireSpecRow*>(RowPair.Value);
		if (!ValidateWeaponFireSpecRowData(RowPair.Key, *FireSpecRow))
		{
			bIsValid = false;
		}
	}

	if (!ValidateWeaponRowsHaveFireSpecs())
	{
		bIsValid = false;
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
		return false;

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

	if (ExpectedItemType == EBG_ItemType::Weapon)
	{
		const FBG_WeaponItemDataRow& WeaponRow = static_cast<const FBG_WeaponItemDataRow&>(ItemRow);
		if (WeaponRow.EquippedWeaponClass.IsNull())
		{
			return SetRegistryFailure(
				OutFailureReason,
				FString::Printf(TEXT("Weapon item row %s has no EquippedWeaponClass assigned."),
				                *RowName.ToString()));
		}
	}

	return true;
}

bool UBG_ItemDataRegistry::ValidateWeaponFireSpecTable(
	UDataTable*& OutFireSpecTable,
	FString* OutFailureReason) const
{
	OutFireSpecTable = nullptr;

	UDataTable* FireSpecTable = GetWeaponFireSpecTable();
	if (!FireSpecTable)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("%s has no WeaponFireSpecs DataTable assigned."),
			                *GetNameSafe(this)));
	}

	const UScriptStruct* ActualRowStruct = FireSpecTable->GetRowStruct();
	if (!ActualRowStruct)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec DataTable %s has no row struct."),
			                *GetNameSafe(FireSpecTable)));
	}

	const UScriptStruct* ExpectedRowStruct = FBG_WeaponFireSpecRow::StaticStruct();
	if (!ActualRowStruct->IsChildOf(ExpectedRowStruct))
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec DataTable %s uses row struct %s, expected %s."),
			                *GetNameSafe(FireSpecTable),
			                *GetNameSafe(ActualRowStruct),
			                *GetNameSafe(ExpectedRowStruct)));
	}

	OutFireSpecTable = FireSpecTable;
	return true;
}

bool UBG_ItemDataRegistry::ValidateWeaponFireSpecRowData(
	FName RowName, const FBG_WeaponFireSpecRow& FireSpecRow,
	FString* OutFailureReason) const
{
	if (!FireSpecRow.WeaponItemTag.IsValid())
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s has an invalid WeaponItemTag."),
			                *RowName.ToString()));
	}

	if (RowName != FireSpecRow.WeaponItemTag.GetTagName())
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s has WeaponItemTag %s; RowName and WeaponItemTag must match."),
			                *RowName.ToString(),
			                *FireSpecRow.WeaponItemTag.ToString()));
	}

	if (FireSpecRow.FireMode == EBG_WeaponFireMode::None)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s has FireMode None."),
			                *RowName.ToString()));
	}

	if (FireSpecRow.FireImplementation == EBG_WeaponFireImplementation::None)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s has FireImplementation None."),
			                *RowName.ToString()));
	}

	if (FireSpecRow.Damage < 0.f || FireSpecRow.Range < 0.f || FireSpecRow.FireCooldown < 0.f)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s has negative damage, range, or cooldown."),
			                *RowName.ToString()));
	}

	if (FireSpecRow.AmmoCost < 0 || FireSpecRow.PelletCount < 1 || FireSpecRow.SpreadAngleDegrees < 0.f)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s has invalid ammo cost, pellet count, or spread."),
			                *RowName.ToString()));
	}

	if (FireSpecRow.FireMode == EBG_WeaponFireMode::Melee && FireSpecRow.AmmoCost != 0)
	{
		return SetRegistryFailure(
			OutFailureReason,
			FString::Printf(TEXT("Weapon fire spec row %s is melee but AmmoCost is %d."),
			                *RowName.ToString(),
			                FireSpecRow.AmmoCost));
	}

	if (FireSpecRow.bCanThrow)
	{
		if (FireSpecRow.ThrowMaxDistance <= 0.f)
		{
			return SetRegistryFailure(
				OutFailureReason,
				FString::Printf(TEXT("Weapon fire spec row %s can throw but ThrowMaxDistance is not positive."),
				                *RowName.ToString()));
		}

		if (FireSpecRow.ThrowDamageMax < FireSpecRow.ThrowDamageMin)
		{
			return SetRegistryFailure(
				OutFailureReason,
				FString::Printf(TEXT("Weapon fire spec row %s has ThrowDamageMax lower than ThrowDamageMin."),
				                *RowName.ToString()));
		}

		if (FireSpecRow.ThrowDamageFalloffStartDistance < 0.f
			|| FireSpecRow.ThrowDamageFalloffStartDistance > FireSpecRow.ThrowMaxDistance)
		{
			return SetRegistryFailure(
				OutFailureReason,
				FString::Printf(TEXT("Weapon fire spec row %s has invalid ThrowDamageFalloffStartDistance."),
				                *RowName.ToString()));
		}
	}

	return true;
}

bool UBG_ItemDataRegistry::ValidateWeaponRowsHaveFireSpecs(FString* OutFailureReason) const
{
	UDataTable* WeaponTable = nullptr;
	if (!ValidateTableForType(EBG_ItemType::Weapon, WeaponTable, OutFailureReason))
		return false;

	UDataTable* FireSpecTable = nullptr;
	if (!ValidateWeaponFireSpecTable(FireSpecTable, OutFailureReason))
		return false;

	bool bIsValid = true;
	for (const TPair<FName, uint8*>& RowPair : WeaponTable->GetRowMap())
	{
		if (!RowPair.Value)
		{
			SetRegistryFailure(
				OutFailureReason,
				FString::Printf(TEXT("%s contains a null weapon item row pointer for row %s in table %s."),
				                *GetNameSafe(this),
				                *RowPair.Key.ToString(),
				                *GetNameSafe(WeaponTable)));
			bIsValid = false;
			continue;
		}

		const FBG_ItemDataRow* ItemRow = reinterpret_cast<const FBG_ItemDataRow*>(RowPair.Value);
		if (!ValidateItemRowData(EBG_ItemType::Weapon, RowPair.Key, *ItemRow, OutFailureReason))
		{
			bIsValid = false;
			continue;
		}

		const FName FireSpecRowName = ItemRow->ItemTag.GetTagName();
		if (!FireSpecTable->FindRowUnchecked(FireSpecRowName))
		{
			SetRegistryFailure(
				OutFailureReason,
				FString::Printf(TEXT("Weapon item row %s has no matching weapon fire spec row in table %s."),
				                *FireSpecRowName.ToString(),
				                *GetNameSafe(FireSpecTable)));
			bIsValid = false;
		}
	}

	return bIsValid;
}
