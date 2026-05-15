// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "Combat/BG_WeaponFireComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Player/BG_Character.h"
#include "UI/BG_HealthViewModel.h"
#include "UObject/UnrealType.h"

namespace BG::Tests
{
	struct FHealthViewModelAmmoFixture
	{
		FTestWorldWrapper WorldWrapper;
		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<APlayerController> PlayerController = nullptr;
		TObjectPtr<ABG_Character> Character = nullptr;
		TObjectPtr<UBG_HealthViewModel> ViewModel = nullptr;
		TObjectPtr<UBG_InventoryComponent> InventoryComponent = nullptr;
		TObjectPtr<UBG_EquipmentComponent> EquipmentComponent = nullptr;
		TObjectPtr<UBG_WeaponFireComponent> WeaponFireComponent = nullptr;

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

			ViewModel = NewObject<UBG_HealthViewModel>(PlayerController.Get());
			Test.TestNotNull(TEXT("PlayerController has a HUD ViewModel."), ViewModel.Get());
			if (!ViewModel)
				return false;

			PlayerController->AddInstanceComponent(ViewModel.Get());
			ViewModel->RegisterComponent();

			InventoryComponent = Character->GetInventoryComponent();
			EquipmentComponent = Character->GetEquipmentComponent();
			WeaponFireComponent = Character->GetWeaponFireComponent();

			Test.TestNotNull(TEXT("Character has inventory component."), InventoryComponent.Get());
			Test.TestNotNull(TEXT("Character has equipment component."), EquipmentComponent.Get());
			Test.TestNotNull(TEXT("Character has weapon fire component."), WeaponFireComponent.Get());
			if (!InventoryComponent || !EquipmentComponent || !WeaponFireComponent)
				return false;

			CacheWeaponFireCharacter();
			ViewModel->NotifyPossessedCharacterReady(Character.Get());
			return true;
		}

	private:
		void CacheWeaponFireCharacter() const
		{
			if (FObjectPropertyBase* CharacterProperty =
				FindFProperty<FObjectPropertyBase>(UBG_WeaponFireComponent::StaticClass(), TEXT("CachedCharacter")))
			{
				void* ValuePtr = CharacterProperty->ContainerPtrToValuePtr<void>(WeaponFireComponent.Get());
				CharacterProperty->SetObjectPropertyValue(ValuePtr, Character.Get());
			}
		}

		void ClearWeaponSocketName(const FName PropertyName) const
		{
			if (FNameProperty* NameProperty = FindFProperty<FNameProperty>(ABG_Character::StaticClass(), PropertyName))
			{
				NameProperty->SetPropertyValue_InContainer(Character.Get(), NAME_None);
			}
		}
	};

	bool TryRequestHUDAmmoTag(FAutomationTestBase& Test, const TCHAR* TagName, FGameplayTag& OutTag)
	{
		OutTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		Test.TestTrue(FString::Printf(TEXT("GameplayTag exists: %s"), TagName), OutTag.IsValid());
		return OutTag.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGActiveWeaponAmmoRenderDataDefaultContractTest,
	"PUBG_HotMode.GEONU.UI.HealthViewModel.ActiveWeaponAmmoRenderDataDefaultsAreSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGActiveWeaponAmmoRenderDataDefaultContractTest::RunTest(const FString& Parameters)
{
	const FBGActiveWeaponAmmoRenderData RenderData;

	TestFalse(TEXT("Default data has no active gun."), RenderData.bHasActiveGun);
	TestEqual(TEXT("Default active weapon slot is none."), RenderData.ActiveWeaponSlot, EBG_EquipmentSlot::None);
	TestFalse(TEXT("Default active weapon item tag is invalid."), RenderData.ActiveWeaponItemTag.IsValid());
	TestFalse(TEXT("Default ammo item tag is invalid."), RenderData.AmmoItemTag.IsValid());
	TestEqual(TEXT("Default loaded ammo is zero."), RenderData.LoadedAmmo, 0);
	TestEqual(TEXT("Default magazine size is zero."), RenderData.MagazineSize, 0);
	TestEqual(TEXT("Default reserve ammo is zero."), RenderData.ReserveAmmo, 0);
	TestFalse(TEXT("Default infinite debug ammo flag is false."), RenderData.bUsesInfiniteDebugAmmo);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGHealthViewModelActiveWeaponAmmoDefaultGetterTest,
	"PUBG_HotMode.GEONU.UI.HealthViewModel.ActiveWeaponAmmoGetterReturnsSafeDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGHealthViewModelActiveWeaponAmmoDefaultGetterTest::RunTest(const FString& Parameters)
{
	UBG_HealthViewModel* ViewModel = NewObject<UBG_HealthViewModel>();
	TestNotNull(TEXT("ViewModel can be created for ammo data contract checks."), ViewModel);
	if (!ViewModel)
		return false;

	ViewModel->ForceUpdateAllAttributes();
	const FBGActiveWeaponAmmoRenderData RenderData = ViewModel->GetActiveWeaponAmmoData();

	TestFalse(TEXT("Unbound ViewModel has no active gun."), RenderData.bHasActiveGun);
	TestEqual(TEXT("Unbound ViewModel active weapon slot is none."), RenderData.ActiveWeaponSlot, EBG_EquipmentSlot::None);
	TestFalse(TEXT("Unbound ViewModel active weapon item tag is invalid."), RenderData.ActiveWeaponItemTag.IsValid());
	TestFalse(TEXT("Unbound ViewModel ammo item tag is invalid."), RenderData.AmmoItemTag.IsValid());
	TestEqual(TEXT("Unbound ViewModel loaded ammo is zero."), RenderData.LoadedAmmo, 0);
	TestEqual(TEXT("Unbound ViewModel magazine size is zero."), RenderData.MagazineSize, 0);
	TestEqual(TEXT("Unbound ViewModel reserve ammo is zero."), RenderData.ReserveAmmo, 0);
	TestFalse(TEXT("Unbound ViewModel infinite debug ammo flag is false."), RenderData.bUsesInfiniteDebugAmmo);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGHealthViewModelActiveWeaponAmmoUsesCharacterStateTest,
	"PUBG_HotMode.GEONU.UI.HealthViewModel.ActiveWeaponAmmoUsesCharacterState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGHealthViewModelActiveWeaponAmmoUsesCharacterStateTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponTag;
	if (!BG::Tests::TryRequestHUDAmmoTag(*this, TEXT("Item.Weapon.AR.M416"), WeaponTag))
		return false;

	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestHUDAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	AddExpectedError(TEXT("CancelReload failed because CachedCharacter was null."),
	                 EAutomationExpectedErrorFlags::Contains, 1);

	BG::Tests::FHealthViewModelAmmoFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	TestTrue(TEXT("Reserve ammo can be added before weapon equip."),
	         Fixture.InventoryComponent->Auth_AddItem(EBG_ItemType::Ammo, AmmoTag, 40, AddedQuantity));
	TestEqual(TEXT("Initial reserve ammo setup adds the requested quantity."), AddedQuantity, 40);

	FGameplayTag ReplacedWeaponTag;
	int32 ReplacedLoadedAmmo = 0;
	TestTrue(TEXT("Weapon can be equipped for HUD ammo calculation."),
	         Fixture.EquipmentComponent->Auth_EquipWeapon(
		         EBG_EquipmentSlot::PrimaryA,
		         WeaponTag,
		         17,
		         ReplacedWeaponTag,
		         ReplacedLoadedAmmo));

	const FBGActiveWeaponAmmoRenderData EquippedData = Fixture.ViewModel->GetActiveWeaponAmmoData();
	TestTrue(TEXT("Equipped gun is visible to HUD ammo data."), EquippedData.bHasActiveGun);
	TestEqual(TEXT("Active weapon slot matches equipment state."), EquippedData.ActiveWeaponSlot, EBG_EquipmentSlot::PrimaryA);
	TestEqual(TEXT("Active weapon item tag matches equipment state."), EquippedData.ActiveWeaponItemTag, WeaponTag);
	TestEqual(TEXT("Ammo item tag comes from active weapon data."), EquippedData.AmmoItemTag, AmmoTag);
	TestEqual(TEXT("Loaded ammo comes from active weapon runtime state."), EquippedData.LoadedAmmo, 17);
	TestEqual(TEXT("Magazine size comes from active weapon runtime state."), EquippedData.MagazineSize, 30);
	TestEqual(TEXT("Reserve ammo comes from inventory quantity."), EquippedData.ReserveAmmo, 40);
	TestFalse(TEXT("Infinite debug ammo starts disabled."), EquippedData.bUsesInfiniteDebugAmmo);

	int32 AddedMoreQuantity = 0;
	TestTrue(TEXT("Reserve ammo quantity change succeeds after binding."),
	         Fixture.InventoryComponent->Auth_AddItem(EBG_ItemType::Ammo, AmmoTag, 10, AddedMoreQuantity));
	TestEqual(TEXT("Reserve ammo delta setup adds the requested quantity."), AddedMoreQuantity, 10);

	const FBGActiveWeaponAmmoRenderData ReserveUpdatedData = Fixture.ViewModel->GetActiveWeaponAmmoData();
	TestEqual(TEXT("Inventory quantity delegate refreshes HUD reserve ammo."),
	          ReserveUpdatedData.ReserveAmmo,
	          50);

	ABG_EquippedWeaponBase* ActiveWeapon = Fixture.EquipmentComponent->GetActiveEquippedWeaponActor();
	TestNotNull(TEXT("Active equipped weapon actor exists."), ActiveWeapon);
	if (!ActiveWeapon)
		return false;

	ActiveWeapon->SetUseInfiniteDebugAmmo(true);
	Fixture.WeaponFireComponent->OnAmmoChanged.Broadcast(
		ActiveWeapon->GetLoadedAmmo(),
		ActiveWeapon->GetMagazineCapacity());

	const FBGActiveWeaponAmmoRenderData InfiniteData = Fixture.ViewModel->GetActiveWeaponAmmoData();
	TestTrue(TEXT("Weapon fire ammo delegate refreshes infinite debug ammo flag."),
	         InfiniteData.bUsesInfiniteDebugAmmo);
	TestEqual(TEXT("Infinite debug ammo keeps HUD reserve data as real inventory quantity."),
	          InfiniteData.ReserveAmmo,
	          50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGHealthViewModelBackpackStatusUsesInventoryStateTest,
	"PUBG_HotMode.GEONU.UI.HealthViewModel.BackpackStatusUsesInventoryState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGHealthViewModelBackpackStatusUsesInventoryStateTest::RunTest(const FString& Parameters)
{
	FGameplayTag BackpackTag;
	if (!BG::Tests::TryRequestHUDAmmoTag(*this, TEXT("Item.Equipment.Backpack.Lv2"), BackpackTag))
		return false;

	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestHUDAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	AddExpectedError(TEXT("RefreshBackpackVisualAttachment failed because BackpackSocketName was None."),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("CancelReload failed because CachedCharacter was null."),
	                 EAutomationExpectedErrorFlags::Contains, 1);

	BG::Tests::FHealthViewModelAmmoFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	TestEqual(TEXT("Initial backpack level is empty."), Fixture.ViewModel->GetBackpackLevel(), 0);

	const FBGInventoryWeightRenderData InitialWeightData = Fixture.ViewModel->GetInventoryWeightData();
	TestEqual(TEXT("Initial HUD current inventory weight matches inventory component."),
	          InitialWeightData.CurrentWeight,
	          Fixture.InventoryComponent->GetCurrentWeight());
	TestEqual(TEXT("Initial HUD max inventory weight matches inventory component."),
	          InitialWeightData.MaxWeight,
	          Fixture.InventoryComponent->GetMaxWeight());

	FGameplayTag ReplacedBackpackTag;
	TestTrue(TEXT("Backpack can be equipped for HUD status data."),
	         Fixture.EquipmentComponent->Auth_EquipBackpack(BackpackTag, ReplacedBackpackTag));
	TestFalse(TEXT("First backpack equip has no replacement."), ReplacedBackpackTag.IsValid());
	TestEqual(TEXT("HUD backpack level follows equipped backpack row."),
	          Fixture.ViewModel->GetBackpackLevel(),
	          2);

	FBGInventoryWeightRenderData EquippedWeightData = Fixture.ViewModel->GetInventoryWeightData();
	TestEqual(TEXT("HUD max inventory weight includes equipped backpack bonus."),
	          EquippedWeightData.MaxWeight,
	          Fixture.InventoryComponent->GetMaxWeight());
	TestEqual(TEXT("HUD current inventory weight remains synced after backpack equip."),
	          EquippedWeightData.CurrentWeight,
	          Fixture.InventoryComponent->GetCurrentWeight());

	int32 AddedQuantity = 0;
	TestTrue(TEXT("Inventory item can be added for HUD fill data."),
	         Fixture.InventoryComponent->Auth_AddItem(EBG_ItemType::Ammo, AmmoTag, 10, AddedQuantity));
	TestEqual(TEXT("Inventory add accepts requested ammo quantity."), AddedQuantity, 10);

	const FBGInventoryWeightRenderData FilledWeightData = Fixture.ViewModel->GetInventoryWeightData();
	TestEqual(TEXT("HUD current inventory weight updates after item add."),
	          FilledWeightData.CurrentWeight,
	          Fixture.InventoryComponent->GetCurrentWeight());
	TestEqual(TEXT("HUD max inventory weight remains synced after item add."),
	          FilledWeightData.MaxWeight,
	          Fixture.InventoryComponent->GetMaxWeight());

	const float ExpectedFillPercent = FilledWeightData.MaxWeight > KINDA_SMALL_NUMBER
		                                  ? FMath::Clamp(
			                                  FilledWeightData.CurrentWeight / FilledWeightData.MaxWeight,
			                                  0.f,
			                                  1.f)
		                                  : 0.f;
	TestTrue(TEXT("HUD inventory fill percent is current over max."),
	         FMath::IsNearlyEqual(FilledWeightData.FillPercent, ExpectedFillPercent));
	TestTrue(TEXT("HUD inventory fill data reports usable capacity."), FilledWeightData.bHasCapacity);

	FGameplayTag RemovedBackpackTag;
	TestTrue(TEXT("Backpack can be unequipped after HUD status check."),
	         Fixture.EquipmentComponent->Auth_UnequipBackpack(RemovedBackpackTag));
	TestEqual(TEXT("Unequip reports the removed backpack tag."), RemovedBackpackTag, BackpackTag);
	TestEqual(TEXT("HUD backpack level clears after backpack unequip."),
	          Fixture.ViewModel->GetBackpackLevel(),
	          0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
