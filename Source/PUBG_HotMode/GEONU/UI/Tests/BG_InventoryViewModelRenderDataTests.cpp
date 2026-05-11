// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "UI/BG_InventoryViewModel.h"
#include "UI/BG_WeaponIconCaptureComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGEquipmentSlotRenderDataDefaultContractTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.EquipmentSlotRenderDataDefaultsAreSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGEquipmentSlotRenderDataDefaultContractTest::RunTest(const FString& Parameters)
{
	const FBGEquipmentSlotRenderData RenderData;

	TestEqual(TEXT("Default slot is empty."), RenderData.Slot, EBG_EquipmentSlot::None);
	TestEqual(TEXT("Default item type is empty."), RenderData.ItemType, EBG_ItemType::None);
	TestFalse(TEXT("Default item tag is invalid."), RenderData.ItemTag.IsValid());
	TestNull(TEXT("Default preview icon resource is null."), RenderData.PreviewIconResource);
	TestFalse(TEXT("Default slot has no preview icon brush."), RenderData.bHasPreviewIconBrush);
	TestTrue(TEXT("Default preview key is none."), RenderData.PreviewIconKey.IsNone());

	TestEqual(TEXT("Default quantity is zero."), RenderData.Quantity, 0);
	TestEqual(TEXT("Default loaded ammo is zero."), RenderData.LoadedAmmo, 0);
	TestEqual(TEXT("Default magazine size is zero."), RenderData.MagazineSize, 0);
	TestEqual(TEXT("Default reserve ammo is zero."), RenderData.ReserveAmmo, 0);
	TestEqual(TEXT("Default durability is zero."), RenderData.Durability, 0.f);
	TestEqual(TEXT("Default max durability is zero."), RenderData.MaxDurability, 0.f);
	TestEqual(TEXT("Default equipment level is zero."), RenderData.EquipmentLevel, 0);
	TestEqual(TEXT("Default backpack weight bonus is zero."), RenderData.BackpackWeightBonus, 0.f);

	TestFalse(TEXT("Default slot is not equipped."), RenderData.bEquipped);
	TestFalse(TEXT("Default slot is not active."), RenderData.bActive);
	TestFalse(TEXT("Default slot has no display row."), RenderData.bHasDisplayRow);
	TestFalse(TEXT("Default slot does not request runtime preview."), RenderData.bUseRuntimePreviewIcon);
	TestFalse(TEXT("Default slot cannot be dragged."), RenderData.bCanDrag);
	TestFalse(TEXT("Default slot cannot be dropped to ground."), RenderData.bCanDropToGround);
	TestFalse(TEXT("Default slot cannot swap with primary slot."), RenderData.bCanSwapWithPrimarySlot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryViewModelEquipmentSlotMapContractTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.EquipmentSlotsUseFixedSlotMapContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelEquipmentSlotMapContractTest::RunTest(const FString& Parameters)
{
	const UBG_InventoryViewModel* ViewModel = NewObject<UBG_InventoryViewModel>();
	TestNotNull(TEXT("ViewModel can be created for render data contract checks."), ViewModel);
	if (!ViewModel)
		return false;

	TestEqual(TEXT("Default equipment slot map starts empty before character binding."),
	          ViewModel->GetEquipmentSlots().Num(), 0);

	FBGEquipmentSlotRenderData PrimaryAData;
	TestFalse(TEXT("Unbound ViewModel has no cached PrimaryA slot data."),
	          ViewModel->GetEquipmentSlotData(EBG_EquipmentSlot::PrimaryA, PrimaryAData));
	TestEqual(TEXT("Fallback PrimaryA data preserves the requested slot."),
	          PrimaryAData.Slot, EBG_EquipmentSlot::PrimaryA);
	TestEqual(TEXT("Fallback PrimaryA data exposes weapon item type."),
	          PrimaryAData.ItemType, EBG_ItemType::Weapon);
	TestFalse(TEXT("Fallback PrimaryA data remains unequipped."), PrimaryAData.bEquipped);

	FBGEquipmentSlotRenderData NoneData;
	TestFalse(TEXT("None is not a valid fixed equipment slot."),
	          ViewModel->GetEquipmentSlotData(EBG_EquipmentSlot::None, NoneData));
	TestEqual(TEXT("Invalid slot fallback stays empty."), NoneData.Slot, EBG_EquipmentSlot::None);
	TestEqual(TEXT("Invalid slot fallback has no item type."), NoneData.ItemType, EBG_ItemType::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryViewModelCommandValidationTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.EquipmentCommandsRejectInvalidInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelCommandValidationTest::RunTest(const FString& Parameters)
{
	UBG_InventoryViewModel* ViewModel = NewObject<UBG_InventoryViewModel>();
	TestNotNull(TEXT("ViewModel can be created for command validation checks."), ViewModel);
	if (!ViewModel)
		return false;

	AddExpectedError(TEXT("DropEquipment failed because EquipmentSlot was None."),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("SwapWeaponSlots failed because"),
	                 EAutomationExpectedErrorFlags::Contains, 3);
	AddExpectedError(TEXT("SelectThrowable failed because ThrowableItemTag was invalid."),
	                 EAutomationExpectedErrorFlags::Contains, 1);

	TestFalse(TEXT("DropEquipment rejects None before forwarding to equipment component."),
	          ViewModel->DropEquipment(EBG_EquipmentSlot::None));
	TestFalse(TEXT("SwapWeaponSlots rejects None as source slot."),
	          ViewModel->SwapWeaponSlots(EBG_EquipmentSlot::None, EBG_EquipmentSlot::PrimaryA));
	TestFalse(TEXT("SwapWeaponSlots rejects non-primary target slot."),
	          ViewModel->SwapWeaponSlots(EBG_EquipmentSlot::PrimaryA, EBG_EquipmentSlot::Pistol));
	TestFalse(TEXT("SwapWeaponSlots rejects identical source and target slots."),
	          ViewModel->SwapWeaponSlots(EBG_EquipmentSlot::PrimaryA, EBG_EquipmentSlot::PrimaryA));
	TestFalse(TEXT("SelectThrowable rejects invalid item tag before forwarding to equipment component."),
	          ViewModel->SelectThrowable(FGameplayTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGWeaponIconCapturePreviewKeyContractTest,
	"PUBG_HotMode.GEONU.UI.WeaponIconCapture.PreviewKeyUsesWeaponIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGWeaponIconCapturePreviewKeyContractTest::RunTest(const FString& Parameters)
{
	const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.AR.M416"), false);
	TestTrue(TEXT("Known weapon gameplay tag exists."), WeaponTag.IsValid());

	const TSoftClassPtr<ABG_EquippedWeaponBase> WeaponClass(ABG_EquippedWeaponBase::StaticClass());
	const FName PrimaryAKey = UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
		WeaponTag,
		EBG_EquipmentSlot::PrimaryA,
		WeaponClass);
	const FName PrimaryAKeyAgain = UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
		WeaponTag,
		EBG_EquipmentSlot::PrimaryA,
		WeaponClass);
	const FName PrimaryBKey = UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
		WeaponTag,
		EBG_EquipmentSlot::PrimaryB,
		WeaponClass);

	TestFalse(TEXT("Valid weapon preview key is not none."), PrimaryAKey.IsNone());
	TestEqual(TEXT("Preview key is stable for the same item, slot, and class."),
	          PrimaryAKey, PrimaryAKeyAgain);
	TestNotEqual(TEXT("Preview key changes when the equipment slot changes."),
	             PrimaryAKey, PrimaryBKey);
	TestTrue(TEXT("Invalid item tag produces no preview key."),
	         UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
		         FGameplayTag(),
		         EBG_EquipmentSlot::PrimaryA,
		         WeaponClass).IsNone());
	TestTrue(TEXT("Invalid equipment slot produces no preview key."),
	         UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
		         WeaponTag,
		         EBG_EquipmentSlot::None,
		         WeaponClass).IsNone());
	TestTrue(TEXT("Missing weapon class produces no preview key."),
	         UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
		         WeaponTag,
		         EBG_EquipmentSlot::PrimaryA,
		         TSoftClassPtr<ABG_EquippedWeaponBase>()).IsNone());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
