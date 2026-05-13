// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_InventoryTypes.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Player/BG_Character.h"

namespace BG::Tests
{
	struct FInventoryComponentTestFixture
	{
		FTestWorldWrapper WorldWrapper;
		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<AActor> Owner = nullptr;
		TObjectPtr<UBG_InventoryComponent> InventoryComponent = nullptr;

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
			Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
			Test.TestNotNull(TEXT("Inventory owner actor is spawned."), Owner.Get());
			if (!Owner)
				return false;

			InventoryComponent = NewObject<UBG_InventoryComponent>(Owner.Get());
			Test.TestNotNull(TEXT("Inventory component is created."), InventoryComponent.Get());
			if (!InventoryComponent)
				return false;

			Owner->AddInstanceComponent(InventoryComponent.Get());
			InventoryComponent->RegisterComponent();
			return true;
		}
	};

	struct FCharacterInventoryTestFixture
	{
		FTestWorldWrapper WorldWrapper;
		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<ABG_Character> Character = nullptr;
		TObjectPtr<UBG_InventoryComponent> InventoryComponent = nullptr;

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
			Character = World->SpawnActor<ABG_Character>(
				ABG_Character::StaticClass(), FTransform::Identity, SpawnParameters);
			Test.TestNotNull(TEXT("Character is spawned."), Character.Get());
			if (!Character)
				return false;

			InventoryComponent = Character->GetInventoryComponent();
			Test.TestNotNull(TEXT("Character has inventory component."), InventoryComponent.Get());
			return InventoryComponent != nullptr;
		}
	};

	bool TryRequestAmmoTag(FAutomationTestBase& Test, const TCHAR* TagName, FGameplayTag& OutTag)
	{
		OutTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		Test.TestTrue(FString::Printf(TEXT("GameplayTag exists: %s"), TagName), OutTag.IsValid());
		return OutTag.IsValid();
	}

	bool TryRequestInventoryTag(FAutomationTestBase& Test, const TCHAR* TagName, FGameplayTag& OutTag)
	{
		OutTag = FGameplayTag::RequestGameplayTag(FName(TagName), false);
		Test.TestTrue(FString::Printf(TEXT("GameplayTag exists: %s"), TagName), OutTag.IsValid());
		return OutTag.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAmmoFillsPartialStackBeforeCreatingNewStackTest,
	"PUBG_HotMode.GEONU.Inventory.Ammo.FillsPartialStackBeforeCreatingNewStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAmmoFillsPartialStackBeforeCreatingNewStackTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	constexpr int32 MaxStackSize = 100;
	FBG_InventoryList InventoryList;

	TestEqual(TEXT("Initial 70 ammo are added to one stack."),
	          InventoryList.AddQuantity(EBG_ItemType::Ammo, AmmoTag, 70, MaxStackSize), 70);
	TestEqual(TEXT("Adding 60 ammo reports the full requested quantity."),
	          InventoryList.AddQuantity(EBG_ItemType::Ammo, AmmoTag, 60, MaxStackSize), 60);

	const TArray<FBG_InventoryEntry>& Entries = InventoryList.GetEntries();
	TestEqual(TEXT("Partial stack is filled before a new stack is created."), Entries.Num(), 2);
	if (Entries.Num() != 2)
		return false;

	TestEqual(TEXT("First ammo stack is filled to max stack size."), Entries[0].Quantity, 100);
	TestEqual(TEXT("Remainder is placed in a new ammo stack."), Entries[1].Quantity, 30);
	TestEqual(TEXT("GetQuantity sums ammo across multiple stacks."),
	          InventoryList.GetQuantity(EBG_ItemType::Ammo, AmmoTag), 130);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAmmoSplitsLargeAddByMaxStackSizeTest,
	"PUBG_HotMode.GEONU.Inventory.Ammo.SplitsLargeAddByMaxStackSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAmmoSplitsLargeAddByMaxStackSizeTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	constexpr int32 MaxStackSize = 100;
	FBG_InventoryList InventoryList;

	TestEqual(TEXT("Large ammo add reports the full requested quantity."),
	          InventoryList.AddQuantity(EBG_ItemType::Ammo, AmmoTag, 230, MaxStackSize), 230);

	const TArray<FBG_InventoryEntry>& Entries = InventoryList.GetEntries();
	TestEqual(TEXT("230 ammo are split into three stacks."), Entries.Num(), 3);
	if (Entries.Num() != 3)
		return false;

	TestEqual(TEXT("First ammo stack has max stack size."), Entries[0].Quantity, 100);
	TestEqual(TEXT("Second ammo stack has max stack size."), Entries[1].Quantity, 100);
	TestEqual(TEXT("Third ammo stack has the remainder."), Entries[2].Quantity, 30);
	TestEqual(TEXT("GetQuantity returns the sum of all split ammo stacks."),
	          InventoryList.GetQuantity(EBG_ItemType::Ammo, AmmoTag), 230);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAuthAddItemRespectsWeightLimitWithoutWorldItemTest,
	"PUBG_HotMode.GEONU.Inventory.AddItem.RespectsWeightLimitWithoutWorldItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAuthAddItemRespectsWeightLimitWithoutWorldItemTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	BG::Tests::FInventoryComponentTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	TestTrue(TEXT("Auth_AddItem succeeds without a world item actor."),
	         Fixture.InventoryComponent->Auth_AddItem(
		         EBG_ItemType::Ammo, AmmoTag, 300, AddedQuantity));
	TestEqual(TEXT("Auth_AddItem accepts only the quantity allowed by carry weight."),
	          AddedQuantity, 100);
	TestEqual(TEXT("Inventory quantity matches the accepted quantity."),
	          Fixture.InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoTag), AddedQuantity);
	TestTrue(TEXT("Auth_AddItem keeps inventory within the normal carry weight."),
	         Fixture.InventoryComponent->GetCurrentWeight() <= Fixture.InventoryComponent->GetMaxWeight());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAuthAddItemRejectsInvalidQuantityTest,
	"PUBG_HotMode.GEONU.Inventory.AddItem.RejectsInvalidQuantity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAuthAddItemRejectsInvalidQuantityTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	BG::Tests::FInventoryComponentTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	AddExpectedError(TEXT("quantity 0 was not positive"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Auth_AddItem rejects zero quantity."),
	          Fixture.InventoryComponent->Auth_AddItem(
		          EBG_ItemType::Ammo, AmmoTag, 0, AddedQuantity));
	TestEqual(TEXT("Rejected add reports no added quantity."), AddedQuantity, 0);
	TestEqual(TEXT("Rejected add does not mutate inventory."),
	          Fixture.InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoTag), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAuthAddItemRejectsInvalidTagTest,
	"PUBG_HotMode.GEONU.Inventory.AddItem.RejectsInvalidTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAuthAddItemRejectsInvalidTagTest::RunTest(const FString& Parameters)
{
	BG::Tests::FInventoryComponentTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	AddExpectedError(TEXT("ItemTag was invalid"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Auth_AddItem rejects invalid item tags."),
	          Fixture.InventoryComponent->Auth_AddItem(
		          EBG_ItemType::Ammo, FGameplayTag(), 10, AddedQuantity));
	TestEqual(TEXT("Invalid tag rejection reports no added quantity."), AddedQuantity, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAuthAddItemRejectsEquipmentTypesTest,
	"PUBG_HotMode.GEONU.Inventory.AddItem.RejectsEquipmentTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAuthAddItemRejectsEquipmentTypesTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponTag;
	if (!BG::Tests::TryRequestInventoryTag(*this, TEXT("Item.Weapon.AR.M416"), WeaponTag))
		return false;

	BG::Tests::FInventoryComponentTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	AddExpectedError(TEXT("item type Weapon is not stored in regular inventory"),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Auth_AddItem rejects equipment item types."),
	          Fixture.InventoryComponent->Auth_AddItem(
		          EBG_ItemType::Weapon, WeaponTag, 1, AddedQuantity));
	TestEqual(TEXT("Equipment type rejection reports no added quantity."), AddedQuantity, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryAuthAddItemRequiresAuthorityTest,
	"PUBG_HotMode.GEONU.Inventory.AddItem.RequiresAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryAuthAddItemRequiresAuthorityTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	BG::Tests::FInventoryComponentTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	Fixture.Owner->SetRole(ROLE_SimulatedProxy);

	int32 AddedQuantity = 0;
	AddExpectedError(TEXT("had no authority"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Auth_AddItem fails without server authority."),
	          Fixture.InventoryComponent->Auth_AddItem(
		          EBG_ItemType::Ammo, AmmoTag, 10, AddedQuantity));
	TestEqual(TEXT("No-authority rejection reports no added quantity."), AddedQuantity, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryCharacterGiveItemUsesAuthAddItemTest,
	"PUBG_HotMode.GEONU.Inventory.GiveItem.UsesAuthAddItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryCharacterGiveItemUsesAuthAddItemTest::RunTest(const FString& Parameters)
{
	FGameplayTag AmmoTag;
	if (!BG::Tests::TryRequestAmmoTag(*this, TEXT("Item.Ammo.556"), AmmoTag))
		return false;

	BG::Tests::FCharacterInventoryTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 0;
	TestTrue(TEXT("GiveInventoryItem succeeds through character server-authority path."),
	         Fixture.Character->GiveInventoryItem(EBG_ItemType::Ammo, AmmoTag, 300, AddedQuantity));
	TestEqual(TEXT("GiveInventoryItem reports the weight-limited added quantity."),
	          AddedQuantity, 100);
	TestEqual(TEXT("GiveInventoryItem mutates the same regular inventory quantity."),
	          Fixture.InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoTag), AddedQuantity);
	TestTrue(TEXT("GiveInventoryItem keeps inventory within carry weight."),
	         Fixture.InventoryComponent->GetCurrentWeight() <= Fixture.InventoryComponent->GetMaxWeight());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGInventoryCharacterGiveItemRejectsInvalidRequestsTest,
	"PUBG_HotMode.GEONU.Inventory.GiveItem.RejectsInvalidRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGInventoryCharacterGiveItemRejectsInvalidRequestsTest::RunTest(const FString& Parameters)
{
	FGameplayTag WeaponTag;
	if (!BG::Tests::TryRequestInventoryTag(*this, TEXT("Item.Weapon.AR.M416"), WeaponTag))
		return false;

	BG::Tests::FCharacterInventoryTestFixture Fixture;
	if (!Fixture.Initialize(*this))
		return false;

	int32 AddedQuantity = 123;
	AddExpectedError(TEXT("ItemTag was invalid"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("quantity 0 was not positive"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("item type Weapon is not stored in regular inventory"),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("GiveInventoryItem failed"), EAutomationExpectedErrorFlags::Contains, 3);

	TestFalse(TEXT("GiveInventoryItem rejects invalid item tags."),
	          Fixture.Character->GiveInventoryItem(
		          EBG_ItemType::Ammo, FGameplayTag(), 10, AddedQuantity));
	TestEqual(TEXT("Invalid tag rejection reports no added quantity."), AddedQuantity, 0);

	AddedQuantity = 123;
	TestFalse(TEXT("GiveInventoryItem rejects non-positive counts."),
	          Fixture.Character->GiveInventoryItem(
		          EBG_ItemType::Ammo, WeaponTag, 0, AddedQuantity));
	TestEqual(TEXT("Invalid count rejection reports no added quantity."), AddedQuantity, 0);

	AddedQuantity = 123;
	TestFalse(TEXT("GiveInventoryItem rejects equipment item types."),
	          Fixture.Character->GiveInventoryItem(
		          EBG_ItemType::Weapon, WeaponTag, 1, AddedQuantity));
	TestEqual(TEXT("Equipment type rejection reports no added quantity."), AddedQuantity, 0);
	TestEqual(TEXT("Rejected give requests do not mutate equipment item quantity."),
	          Fixture.InventoryComponent->GetQuantity(EBG_ItemType::Weapon, WeaponTag), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
