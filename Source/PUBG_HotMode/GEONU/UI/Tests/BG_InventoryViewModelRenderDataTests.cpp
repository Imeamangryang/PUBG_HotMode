// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_WorldItemBase.h"
#include "Player/BG_Character.h"
#include "UI/BG_InventoryViewModel.h"
#include "UI/BG_WeaponIconCaptureComponent.h"
#include "UObject/UnrealType.h"

namespace BG::Tests
{
	struct FInventoryViewModelFixture
	{
		FTestWorldWrapper WorldWrapper;
		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<APlayerController> PlayerController = nullptr;
		TObjectPtr<ABG_Character> Character = nullptr;
		TObjectPtr<UBG_InventoryViewModel> ViewModel = nullptr;
		TObjectPtr<UBG_InventoryComponent> InventoryComponent = nullptr;
		TObjectPtr<UBG_EquipmentComponent> EquipmentComponent = nullptr;
		TObjectPtr<UBG_WeaponIconCaptureComponent> WeaponIconCaptureComponent = nullptr;

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
			{
				WorldWrapper.ForwardErrorMessages(&Test);
				return false;
			}

			World = WorldWrapper.GetTestWorld();
			Test.TestNotNull(TEXT("Test world is created."), World.Get());
			if (!World)
				return false;

			UGameInstance* GameInstance = World->GetGameInstance();
			Test.TestNotNull(TEXT("Test world has a game instance."), GameInstance);
			if (!GameInstance)
				return false;

			UBG_ItemDataRegistrySubsystem* RegistrySubsystem =
				GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
			Test.TestNotNull(TEXT("Item data registry subsystem is available."), RegistrySubsystem);
			if (!RegistrySubsystem)
				return false;

			if (!Test.TestTrue(TEXT("Item data registry is loaded."),
			                   RegistrySubsystem->IsRegistryLoaded() || RegistrySubsystem->LoadRegistry()))
			{
				return false;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			PlayerController = World->SpawnActor<APlayerController>(
				APlayerController::StaticClass(), FTransform::Identity, SpawnParameters);
			Test.TestNotNull(TEXT("PlayerController is spawned."), PlayerController.Get());
			if (!PlayerController)
				return false;

			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			Test.TestNotNull(TEXT("LocalPlayer is created for local controller checks."), LocalPlayer);
			if (!LocalPlayer)
				return false;
			PlayerController->SetPlayer(LocalPlayer);

			WeaponIconCaptureComponent = NewObject<UBG_WeaponIconCaptureComponent>(PlayerController.Get());
			Test.TestNotNull(TEXT("Weapon icon capture component is created."), WeaponIconCaptureComponent.Get());
			if (!WeaponIconCaptureComponent)
				return false;

			PlayerController->AddInstanceComponent(WeaponIconCaptureComponent.Get());
			WeaponIconCaptureComponent->RegisterComponent();

			Character = World->SpawnActor<ABG_Character>(
				ABG_Character::StaticClass(), FTransform::Identity, SpawnParameters);
			Test.TestNotNull(TEXT("Character is spawned."), Character.Get());
			if (!Character)
				return false;

			ClearWeaponSocketName(TEXT("WeaponHandSocketName"));
			ClearWeaponSocketName(TEXT("WeaponBackSocketName"));
			ClearWeaponSocketName(TEXT("BackpackSocketName"));

			if (!Character->HasActorBegunPlay())
			{
				Character->DispatchBeginPlay();
			}

			PlayerController->Possess(Character.Get());
			Test.TestTrue(TEXT("Test PlayerController is local."), PlayerController->IsLocalController());
			if (!PlayerController->IsLocalController())
				return false;

			ViewModel = NewObject<UBG_InventoryViewModel>(PlayerController.Get());
			Test.TestNotNull(TEXT("PlayerController has an inventory ViewModel."), ViewModel.Get());
			if (!ViewModel)
				return false;

			PlayerController->AddInstanceComponent(ViewModel.Get());
			ViewModel->RegisterComponent();

			InventoryComponent = Character->GetInventoryComponent();
			EquipmentComponent = Character->GetEquipmentComponent();

			Test.TestNotNull(TEXT("Character has inventory component."), InventoryComponent.Get());
			Test.TestNotNull(TEXT("Character has equipment component."), EquipmentComponent.Get());
			if (!InventoryComponent || !EquipmentComponent)
				return false;

			ViewModel->NotifyPossessedCharacterReady(Character.Get());
			return true;
		}

	private:
		void ClearWeaponSocketName(const FName PropertyName) const
		{
			if (FNameProperty* NameProperty = FindFProperty<FNameProperty>(ABG_Character::StaticClass(), PropertyName))
			{
				NameProperty->SetPropertyValue_InContainer(Character.Get(), NAME_None);
			}
		}
	};

	bool TryRequestInventoryUITag(FAutomationTestBase& Test, const TCHAR* TagName, FGameplayTag& OutTag)
	{
		OutTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		Test.TestTrue(FString::Printf(TEXT("GameplayTag exists: %s"), TagName), OutTag.IsValid());
		return OutTag.IsValid();
	}
}

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
	TestEqual(TEXT("Default preview icon display size is zero."),
	          RenderData.PreviewIconDisplaySize,
	          FVector2D::ZeroVector);
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
	FBGInventoryViewModelEquipmentSlotAmmoUsesCharacterStateTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.EquipmentSlotAmmoUsesCharacterState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelEquipmentSlotAmmoUsesCharacterStateTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponTag;
	if (!BG::Tests::TryRequestInventoryUITag(*this, TEXT("Item.Weapon.AR.M416"), WeaponTag))
		return false;

	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestInventoryUITag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	AddExpectedError(TEXT("CancelReload failed because CachedCharacter was null."),
	                 EAutomationExpectedErrorFlags::Contains, 1);

	BG::Tests::FInventoryViewModelFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	TestTrue(TEXT("Reserve ammo can be added before weapon equip."),
	         Fixture.InventoryComponent->Auth_AddItem(EBG_ItemType::Ammo, AmmoTag, 40, AddedQuantity));
	TestEqual(TEXT("Initial reserve ammo setup adds the requested quantity."), AddedQuantity, 40);

	FGameplayTag ReplacedWeaponTag;
	int32 ReplacedLoadedAmmo = 0;
	TestTrue(TEXT("Weapon can be equipped for inventory ammo render data."),
	         Fixture.EquipmentComponent->Auth_EquipWeapon(
		         EBG_EquipmentSlot::PrimaryA,
		         WeaponTag,
		         17,
		         ReplacedWeaponTag,
		         ReplacedLoadedAmmo));

	FBGEquipmentSlotRenderData EquippedData;
	TestTrue(TEXT("Equipped PrimaryA slot data is cached."),
	         Fixture.ViewModel->GetEquipmentSlotData(EBG_EquipmentSlot::PrimaryA, EquippedData));
	TestTrue(TEXT("PrimaryA is equipped."), EquippedData.bEquipped);
	TestEqual(TEXT("PrimaryA item tag matches equipped weapon."), EquippedData.ItemTag, WeaponTag);
	TestEqual(TEXT("Loaded ammo comes from equipped weapon runtime state."), EquippedData.LoadedAmmo, 17);
	TestEqual(TEXT("Magazine size comes from equipped weapon runtime state."), EquippedData.MagazineSize, 30);
	TestEqual(TEXT("Reserve ammo comes from matching inventory ammo quantity."), EquippedData.ReserveAmmo, 40);

	ABG_EquippedWeaponBase* WeaponActor =
		Fixture.EquipmentComponent->GetEquippedWeaponActor(EBG_EquipmentSlot::PrimaryA);
	TestNotNull(TEXT("PrimaryA equipped weapon actor exists."), WeaponActor);
	if (!WeaponActor)
		return false;

	WeaponActor->ApplyWeaponRuntimeDefinition(
		WeaponActor->GetWeaponPoseType(),
		AmmoTag,
		27,
		9);
	Fixture.EquipmentComponent->OnEquipmentChanged.Broadcast();

	FBGEquipmentSlotRenderData RuntimeUpdatedData;
	TestTrue(TEXT("PrimaryA slot data remains cached after runtime weapon update."),
	         Fixture.ViewModel->GetEquipmentSlotData(EBG_EquipmentSlot::PrimaryA, RuntimeUpdatedData));
	TestEqual(TEXT("Actor runtime loaded ammo takes priority over equipment fallback."),
	          RuntimeUpdatedData.LoadedAmmo, 9);
	TestEqual(TEXT("Actor runtime magazine size takes priority over weapon row fallback."),
	          RuntimeUpdatedData.MagazineSize, 27);
	TestEqual(TEXT("Actor runtime ammo tag still resolves inventory reserve ammo."),
	          RuntimeUpdatedData.ReserveAmmo, 40);

	int32 AddedMoreQuantity = 0;
	TestTrue(TEXT("Reserve ammo quantity change succeeds after binding."),
	         Fixture.InventoryComponent->Auth_AddItem(EBG_ItemType::Ammo, AmmoTag, 10, AddedMoreQuantity));
	TestEqual(TEXT("Reserve ammo delta setup adds the requested quantity."), AddedMoreQuantity, 10);

	FBGEquipmentSlotRenderData ReserveUpdatedData;
	TestTrue(TEXT("PrimaryA slot data remains cached after inventory quantity update."),
	         Fixture.ViewModel->GetEquipmentSlotData(EBG_EquipmentSlot::PrimaryA, ReserveUpdatedData));
	TestEqual(TEXT("Inventory quantity change refreshes equipment slot reserve ammo."),
	          ReserveUpdatedData.ReserveAmmo,
	          50);
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
	AddExpectedError(TEXT("MoveEquippedWeaponSlot failed because"),
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
	TestFalse(TEXT("MoveEquippedWeaponSlot rejects None as source slot."),
	          ViewModel->MoveEquippedWeaponSlot(EBG_EquipmentSlot::None, EBG_EquipmentSlot::PrimaryA));
	TestFalse(TEXT("MoveEquippedWeaponSlot rejects non-primary target slot."),
	          ViewModel->MoveEquippedWeaponSlot(EBG_EquipmentSlot::PrimaryA, EBG_EquipmentSlot::Pistol));
	TestFalse(TEXT("MoveEquippedWeaponSlot rejects identical source and target slots."),
	          ViewModel->MoveEquippedWeaponSlot(EBG_EquipmentSlot::PrimaryA, EBG_EquipmentSlot::PrimaryA));
	TestFalse(TEXT("SelectThrowable rejects invalid item tag before forwarding to equipment component."),
	          ViewModel->SelectThrowable(FGameplayTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryViewModelBackpackSlotRenderDataTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.BackpackSlotExposesIconAndLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelBackpackSlotRenderDataTest::RunTest(const FString& Parameters)
{
	FGameplayTag BackpackTag;
	if (!BG::Tests::TryRequestInventoryUITag(*this, TEXT("Item.Equipment.Backpack.Lv3"), BackpackTag))
		return false;

	AddExpectedError(TEXT("RefreshBackpackVisualAttachment failed because BackpackSocketName was None."),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("CancelReload failed because CachedCharacter was null."),
	                 EAutomationExpectedErrorFlags::Contains, 1);

	BG::Tests::FInventoryViewModelFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	FGameplayTag ReplacedBackpackTag;
	TestTrue(TEXT("Backpack can be equipped for inventory slot render data."),
	         Fixture.EquipmentComponent->Auth_EquipBackpack(BackpackTag, ReplacedBackpackTag));
	TestFalse(TEXT("First backpack equip has no replacement."), ReplacedBackpackTag.IsValid());

	FBGEquipmentSlotRenderData BackpackData;
	TestTrue(TEXT("Backpack slot data is cached after equip."),
	         Fixture.ViewModel->GetEquipmentSlotData(EBG_EquipmentSlot::Backpack, BackpackData));
	TestTrue(TEXT("Backpack slot is equipped."), BackpackData.bEquipped);
	TestEqual(TEXT("Backpack slot item type is backpack."),
	          BackpackData.ItemType,
	          EBG_ItemType::Backpack);
	TestEqual(TEXT("Backpack slot item tag matches equipped backpack."),
	          BackpackData.ItemTag,
	          BackpackTag);
	TestEqual(TEXT("Backpack slot exposes backpack level."),
	          BackpackData.EquipmentLevel,
	          3);
	TestEqual(TEXT("Backpack slot exposes backpack weight bonus."),
	          BackpackData.BackpackWeightBonus,
	          150.f);
	TestFalse(TEXT("Backpack slot exposes an icon soft reference."),
	          BackpackData.Icon.IsNull());
	TestTrue(TEXT("Backpack slot has display row data."),
	         BackpackData.bHasDisplayRow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryViewModelMoveEquippedWeaponSlotTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.MoveEquippedWeaponSlotSwapsPrimaryWeapons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelMoveEquippedWeaponSlotTest::RunTest(const FString& Parameters)
{
	FGameplayTag PrimaryAWeaponTag;
	if (!BG::Tests::TryRequestInventoryUITag(*this, TEXT("Item.Weapon.AR.M416"), PrimaryAWeaponTag))
		return false;

	FGameplayTag PrimaryBWeaponTag;
	if (!BG::Tests::TryRequestInventoryUITag(*this, TEXT("Item.Weapon.SR.Kar98k"), PrimaryBWeaponTag))
		return false;

	BG::Tests::FInventoryViewModelFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	FGameplayTag ReplacedWeaponTag;
	int32 ReplacedLoadedAmmo = 0;
	TestTrue(TEXT("PrimaryA weapon can be equipped before drag/drop swap."),
	         Fixture.EquipmentComponent->Auth_EquipWeapon(
		         EBG_EquipmentSlot::PrimaryA,
		         PrimaryAWeaponTag,
		         17,
		         ReplacedWeaponTag,
		         ReplacedLoadedAmmo));
	TestTrue(TEXT("PrimaryB weapon can be equipped before drag/drop swap."),
	         Fixture.EquipmentComponent->Auth_EquipWeapon(
		         EBG_EquipmentSlot::PrimaryB,
		         PrimaryBWeaponTag,
		         3,
		         ReplacedWeaponTag,
		         ReplacedLoadedAmmo));

	TestTrue(TEXT("MoveEquippedWeaponSlot accepts PrimaryA to PrimaryB drag/drop swap."),
	         Fixture.ViewModel->MoveEquippedWeaponSlot(EBG_EquipmentSlot::PrimaryA, EBG_EquipmentSlot::PrimaryB));
	TestEqual(TEXT("PrimaryA receives PrimaryB weapon after drag/drop swap."),
	          Fixture.EquipmentComponent->GetEquippedItemTag(EBG_EquipmentSlot::PrimaryA),
	          PrimaryBWeaponTag);
	TestEqual(TEXT("PrimaryB receives PrimaryA weapon after drag/drop swap."),
	          Fixture.EquipmentComponent->GetEquippedItemTag(EBG_EquipmentSlot::PrimaryB),
	          PrimaryAWeaponTag);
	TestEqual(TEXT("PrimaryA receives PrimaryB loaded ammo after drag/drop swap."),
	          Fixture.EquipmentComponent->GetLoadedAmmo(EBG_EquipmentSlot::PrimaryA),
	          3);
	TestEqual(TEXT("PrimaryB receives PrimaryA loaded ammo after drag/drop swap."),
	          Fixture.EquipmentComponent->GetLoadedAmmo(EBG_EquipmentSlot::PrimaryB),
	          17);

	TestTrue(TEXT("MoveEquippedWeaponSlot accepts PrimaryB to PrimaryA drag/drop swap."),
	         Fixture.ViewModel->MoveEquippedWeaponSlot(EBG_EquipmentSlot::PrimaryB, EBG_EquipmentSlot::PrimaryA));
	TestEqual(TEXT("PrimaryA receives original PrimaryA weapon after reverse drag/drop swap."),
	          Fixture.EquipmentComponent->GetEquippedItemTag(EBG_EquipmentSlot::PrimaryA),
	          PrimaryAWeaponTag);
	TestEqual(TEXT("PrimaryB receives original PrimaryB weapon after reverse drag/drop swap."),
	          Fixture.EquipmentComponent->GetEquippedItemTag(EBG_EquipmentSlot::PrimaryB),
	          PrimaryBWeaponTag);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGPickupPromptRenderDataDefaultContractTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.PickupPromptRenderDataDefaultsAreSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGPickupPromptRenderDataDefaultContractTest::RunTest(const FString& Parameters)
{
	const FBGPickupPromptRenderData RenderData;

	TestFalse(TEXT("Default pickup prompt is hidden."), RenderData.bVisible);
	TestTrue(TEXT("Default pickup prompt display name is empty."), RenderData.DisplayName.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryViewModelPickupPromptTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.PickupPromptUsesCurrentWorldItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelPickupPromptTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestInventoryUITag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	AddExpectedError(TEXT("CancelReload failed because CachedCharacter was null."),
	                 EAutomationExpectedErrorFlags::Contains, 1);

	BG::Tests::FInventoryViewModelFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABG_WorldItemBase* WorldItem = Fixture.World->SpawnActor<ABG_WorldItemBase>(
		ABG_WorldItemBase::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(100.f, 0.f, 0.f)),
		SpawnParameters);
	TestNotNull(TEXT("World item is spawned for pickup prompt."), WorldItem);
	if (!WorldItem)
		return false;

	WorldItem->InitializeWorldItem(EBG_ItemType::Ammo, AmmoTag, 30);
	Fixture.Character->SetCurrentInteractableWeapon(WorldItem);
	Fixture.ViewModel->RefreshAllRenderData();

	const FBGPickupPromptRenderData PromptData = Fixture.ViewModel->GetPickupPromptData();
	TestTrue(TEXT("Current world item shows pickup prompt."), PromptData.bVisible);
	TestEqual(TEXT("Pickup prompt display name comes from item row."),
	          PromptData.DisplayName.ToString(),
	          FString(TEXT("5.56mm")));

	Fixture.Character->SetCurrentInteractableWeapon(nullptr);
	Fixture.ViewModel->RefreshAllRenderData();

	const FBGPickupPromptRenderData ClearedPromptData = Fixture.ViewModel->GetPickupPromptData();
	TestFalse(TEXT("Cleared current item hides pickup prompt."), ClearedPromptData.bVisible);
	TestTrue(TEXT("Cleared pickup prompt has no display name."), ClearedPromptData.DisplayName.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryFailureRenderDataDefaultContractTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.FailureRenderDataDefaultsAreSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryFailureRenderDataDefaultContractTest::RunTest(const FString& Parameters)
{
	const FBGInventoryFailureRenderData RenderData;

	TestEqual(TEXT("Default failure reason is none."), RenderData.FailReason, EBGInventoryFailReason::None);
	TestEqual(TEXT("Default failure item type is none."), RenderData.ItemType, EBG_ItemType::None);
	TestFalse(TEXT("Default failure item tag is invalid."), RenderData.ItemTag.IsValid());
	TestTrue(TEXT("Default failure display name is empty."), RenderData.DisplayName.IsEmpty());
	TestTrue(TEXT("Default failure user message is empty."), RenderData.UserMessage.IsEmpty());
	TestEqual(TEXT("Default failure highlight slot is none."),
	          RenderData.HighlightEquipmentSlot,
	          EBG_EquipmentSlot::None);
	TestFalse(TEXT("Default failure has no display row."), RenderData.bHasDisplayRow);
	TestFalse(TEXT("Default failure is not a capacity failure."), RenderData.bIsCapacityFailure);
	TestFalse(TEXT("Default failure does not flash backpack icon."), RenderData.bShouldFlashBackpackIcon);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryViewModelCapacityFailureFeedbackTest,
	"PUBG_HotMode.GEONU.UI.InventoryViewModel.CapacityFailuresRequestBackpackFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryViewModelCapacityFailureFeedbackTest::RunTest(const FString& Parameters)
{
	UBG_InventoryViewModel* ViewModel = NewObject<UBG_InventoryViewModel>();
	TestNotNull(TEXT("ViewModel can be created for failure feedback checks."), ViewModel);
	if (!ViewModel)
		return false;

	ViewModel->NotifyInventoryFailure(EBGInventoryFailReason::Overweight, EBG_ItemType::Ammo, FGameplayTag());
	const FBGInventoryFailureRenderData ItemCapacityFailure = ViewModel->GetLastFailure();
	TestEqual(TEXT("Item capacity failure exposes pickup message."),
	          ItemCapacityFailure.UserMessage.ToString(),
	          FString(TEXT("인벤토리 용량이 부족합니다.")));
	TestEqual(TEXT("Item capacity failure highlights backpack slot."),
	          ItemCapacityFailure.HighlightEquipmentSlot,
	          EBG_EquipmentSlot::Backpack);
	TestTrue(TEXT("Item capacity failure is marked as capacity failure."),
	         ItemCapacityFailure.bIsCapacityFailure);
	TestTrue(TEXT("Item capacity failure flashes backpack icon."),
	         ItemCapacityFailure.bShouldFlashBackpackIcon);

	ViewModel->NotifyInventoryFailure(EBGInventoryFailReason::Overweight, EBG_ItemType::Backpack, FGameplayTag());
	const FBGInventoryFailureRenderData BackpackCapacityFailure = ViewModel->GetLastFailure();
	TestEqual(TEXT("Backpack capacity failure exposes replacement message."),
	          BackpackCapacityFailure.UserMessage.ToString(),
	          FString(TEXT("너무 많은 아이템을 가지고 있습니다.")));
	TestEqual(TEXT("Backpack capacity failure highlights backpack slot."),
	          BackpackCapacityFailure.HighlightEquipmentSlot,
	          EBG_EquipmentSlot::Backpack);
	TestTrue(TEXT("Backpack capacity failure flashes backpack icon."),
	         BackpackCapacityFailure.bShouldFlashBackpackIcon);

	ViewModel->NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::None, FGameplayTag());
	const FBGInventoryFailureRenderData InvalidItemFailure = ViewModel->GetLastFailure();
	TestFalse(TEXT("Non-capacity failure does not flash backpack icon."),
	          InvalidItemFailure.bShouldFlashBackpackIcon);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGWeaponIconCaptureReferenceOrthoSizeTest,
	"PUBG_HotMode.GEONU.UI.WeaponIconCapture.ReferenceOrthoSizeUsesRenderTargetAspect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGWeaponIconCaptureReferenceOrthoSizeTest::RunTest(const FString& Parameters)
{
	const UBG_WeaponIconCaptureComponent* WeaponIconCaptureComponent = NewObject<UBG_WeaponIconCaptureComponent>();
	TestNotNull(TEXT("Weapon icon capture component can be created."), WeaponIconCaptureComponent);
	if (!WeaponIconCaptureComponent)
		return false;

	const FVector2D ReferenceOrthoSize = WeaponIconCaptureComponent->GetPreviewReferenceOrthoSize();
	TestEqual(TEXT("Default reference ortho width matches DefaultOrthoWidth."), ReferenceOrthoSize.X, 180.0);
	TestEqual(TEXT("Default reference ortho height follows the 512x256 preview aspect ratio."),
	          ReferenceOrthoSize.Y,
	          90.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
