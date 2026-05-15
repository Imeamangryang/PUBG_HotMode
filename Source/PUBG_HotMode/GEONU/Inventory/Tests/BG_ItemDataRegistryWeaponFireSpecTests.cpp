// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "Combat/BG_WeaponFireTypes.h"
#include "Inventory/BG_ItemDataRegistry.h"

namespace BG::Tests
{
	UDataTable* CreateTransientDataTable(UScriptStruct* RowStruct)
	{
		UDataTable* DataTable = NewObject<UDataTable>(GetTransientPackage());
		DataTable->RowStruct = RowStruct;
		return DataTable;
	}

	bool TryRequestGameplayTag(FAutomationTestBase& Test, const TCHAR* TagName, FGameplayTag& OutTag)
	{
		OutTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		Test.TestTrue(FString::Printf(TEXT("GameplayTag exists: %s"), TagName), OutTag.IsValid());
		return OutTag.IsValid();
	}

	FBG_WeaponItemDataRow MakeWeaponItemRow(const FGameplayTag& WeaponItemTag)
	{
		FBG_WeaponItemDataRow Row;
		Row.ItemType = EBG_ItemType::Weapon;
		Row.ItemTag = WeaponItemTag;
		Row.DisplayName = FText::FromString(TEXT("M416"));
		Row.EquipSlot = EBG_WeaponEquipSlot::Primary;
		Row.WeaponPoseCategory = EBG_WeaponPoseCategory::Rifle;
		Row.AmmoItemTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Ammo.556")), false);
		Row.MagazineSize = 30;
		Row.EquippedWeaponClass = ABG_EquippedWeaponBase::StaticClass();
		return Row;
	}

	FBG_WeaponFireSpecRow MakeM416FireSpecRow(const FGameplayTag& WeaponItemTag)
	{
		FBG_WeaponFireSpecRow Row;
		Row.WeaponItemTag = WeaponItemTag;
		Row.FireMode = EBG_WeaponFireMode::FullAuto;
		Row.FireImplementation = EBG_WeaponFireImplementation::Projectile;
		Row.Damage = 40.f;
		Row.Range = 100000.f;
		Row.FireCooldown = 0.086f;
		Row.AmmoCost = 1;
		Row.PelletCount = 1;
		Row.ProjectileInitialSpeed = 88000.f;
		return Row;
	}

	UBG_ItemDataRegistry* CreateRegistryWithWeaponTables()
	{
		UBG_ItemDataRegistry* Registry = NewObject<UBG_ItemDataRegistry>(GetTransientPackage());
		Registry->WeaponItems = CreateTransientDataTable(FBG_WeaponItemDataRow::StaticStruct());
		Registry->WeaponFireSpecs = CreateTransientDataTable(FBG_WeaponFireSpecRow::StaticStruct());
		return Registry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGItemDataRegistryFindsWeaponFireSpecTest,
	"PUBG_HotMode.GEONU.Inventory.ItemDataRegistry.FindsWeaponFireSpecByWeaponTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGItemDataRegistryFindsWeaponFireSpecTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponItemTag;
	if (!BG::Tests::TryRequestGameplayTag(*this, TEXT("Item.Weapon.AR.M416"), WeaponItemTag))
		return false;

	UBG_ItemDataRegistry* Registry = BG::Tests::CreateRegistryWithWeaponTables();
	Registry->WeaponItems->AddRow(WeaponItemTag.GetTagName(), BG::Tests::MakeWeaponItemRow(WeaponItemTag));
	Registry->WeaponFireSpecs->AddRow(WeaponItemTag.GetTagName(), BG::Tests::MakeM416FireSpecRow(WeaponItemTag));

	TestTrue(TEXT("Weapon fire specs validate when row name and WeaponItemTag match."),
	         Registry->ValidateWeaponFireSpecs());

	const FBG_WeaponFireSpecRow* FoundRow = Registry->FindWeaponFireSpecRow(WeaponItemTag);
	TestNotNull(TEXT("FindWeaponFireSpecRow returns the matching row."), FoundRow);
	TestEqual(TEXT("M416 seed damage is returned."), FoundRow ? FoundRow->Damage : 0.f, 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGItemDataRegistryRejectsMismatchedWeaponFireSpecTest,
	"PUBG_HotMode.GEONU.Inventory.ItemDataRegistry.RejectsMismatchedWeaponFireSpecRowName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGItemDataRegistryRejectsMismatchedWeaponFireSpecTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponItemTag;
	if (!BG::Tests::TryRequestGameplayTag(*this, TEXT("Item.Weapon.AR.M416"), WeaponItemTag))
		return false;

	UBG_ItemDataRegistry* Registry = BG::Tests::CreateRegistryWithWeaponTables();
	Registry->WeaponItems->AddRow(WeaponItemTag.GetTagName(), BG::Tests::MakeWeaponItemRow(WeaponItemTag));
	Registry->WeaponFireSpecs->AddRow(FName(TEXT("Item.Weapon.AR.WrongRow")),
	                                  BG::Tests::MakeM416FireSpecRow(WeaponItemTag));

	AddExpectedError(TEXT("RowName and WeaponItemTag must match"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("has no matching weapon fire spec row"), EAutomationExpectedErrorFlags::Contains, 1);

	TestFalse(TEXT("Weapon fire specs reject rows whose RowName and WeaponItemTag differ."),
	          Registry->ValidateWeaponFireSpecs());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGItemDataRegistryRejectsWeaponWithoutEquippedClassTest,
	"PUBG_HotMode.GEONU.Inventory.ItemDataRegistry.RejectsWeaponWithoutEquippedWeaponClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGItemDataRegistryRejectsWeaponWithoutEquippedClassTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponItemTag;
	if (!BG::Tests::TryRequestGameplayTag(*this, TEXT("Item.Weapon.AR.M416"), WeaponItemTag))
		return false;

	UBG_ItemDataRegistry* Registry = BG::Tests::CreateRegistryWithWeaponTables();
	FBG_WeaponItemDataRow WeaponRow = BG::Tests::MakeWeaponItemRow(WeaponItemTag);
	WeaponRow.EquippedWeaponClass.Reset();

	Registry->WeaponItems->AddRow(WeaponItemTag.GetTagName(), WeaponRow);
	Registry->WeaponFireSpecs->AddRow(WeaponItemTag.GetTagName(), BG::Tests::MakeM416FireSpecRow(WeaponItemTag));

	AddExpectedError(TEXT("has no EquippedWeaponClass assigned"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Weapon rows without an equipped actor class are rejected."),
	          Registry->ValidateWeaponFireSpecs());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
