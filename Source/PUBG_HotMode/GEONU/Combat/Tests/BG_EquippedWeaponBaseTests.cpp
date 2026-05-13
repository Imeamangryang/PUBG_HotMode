// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGEquippedWeaponBaseDefaultComponentsTest,
	"PUBG_HotMode.GEONU.Combat.EquippedWeaponBase.ProvidesDefaultAuthoringComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGEquippedWeaponBaseDefaultComponentsTest::RunTest(const FString& Parameters)
{
	const ABG_EquippedWeaponBase* DefaultWeapon = GetDefault<ABG_EquippedWeaponBase>();
	TestNotNull(TEXT("Equipped mesh component is available."), DefaultWeapon->GetEquippedMeshComponent());
	TestEqual(TEXT("Default muzzle source is the static mesh socket named Muzzle."),
	          DefaultWeapon->GetMuzzleSocketName(), FName(TEXT("Muzzle")));
	TestEqual(TEXT("Default grip source is the static mesh socket named Grip."),
	          DefaultWeapon->GetGripSocketName(), FName(TEXT("Grip")));
	TestEqual(TEXT("Default support hand source is the static mesh socket named SupportHand."),
	          DefaultWeapon->GetSupportHandSocketName(), FName(TEXT("SupportHand")));

	FTransform MuzzleTransform;
	AddExpectedError(TEXT("socket Muzzle does not exist"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Default weapon cannot resolve muzzle without a mesh socket."),
	          DefaultWeapon->GetMuzzleTransform(MuzzleTransform));
	TestTrue(TEXT("Default hand attach transform starts as identity."),
	         DefaultWeapon->GetHandAttachTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Default back attach transform starts as identity."),
	         DefaultWeapon->GetBackAttachTransform().Equals(FTransform::Identity));
	TestTrue(TEXT("Default inventory preview transform starts as identity."),
	         DefaultWeapon->GetPreviewTransform().Equals(FTransform::Identity));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGEquippedWeaponBaseStrictReloadUsesInventoryAmmoTest,
	"PUBG_HotMode.GEONU.Combat.EquippedWeaponBase.StrictReloadUsesInventoryAmmo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGEquippedWeaponBaseStrictReloadUsesInventoryAmmoTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
		return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABG_EquippedWeaponBase* Weapon = World->SpawnActor<ABG_EquippedWeaponBase>(
		ABG_EquippedWeaponBase::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("Equipped weapon actor is spawned."), Weapon);
	if (!Weapon)
		return false;

	Weapon->ApplyWeaponRuntimeDefinition(EBGWeaponPoseType::Rifle, FGameplayTag(), 30, 0);

	TestFalse(TEXT("Infinite debug ammo starts disabled by default."),
	          Weapon->UsesInfiniteDebugAmmo());
	TestFalse(TEXT("Strict reload cannot start with no inventory ammo."),
	          Weapon->CanReloadWithInventoryAmmo(0));
	TestEqual(TEXT("Strict reload resolves no ammo when inventory ammo is empty."),
	          Weapon->ResolveReloadAmount(0), 0);
	TestTrue(TEXT("Strict reload can start when inventory ammo is available."),
	         Weapon->CanReloadWithInventoryAmmo(1));
	TestEqual(TEXT("Strict reload loads only available inventory ammo."),
	          Weapon->ResolveReloadAmount(12), 12);
	TestEqual(TEXT("Strict reload clamps inventory ammo to missing magazine space."),
	          Weapon->ResolveReloadAmount(300), 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGEquippedWeaponBaseInfiniteDebugAmmoBypassesInventoryAmmoTest,
	"PUBG_HotMode.GEONU.Combat.EquippedWeaponBase.InfiniteDebugAmmoBypassesInventoryAmmo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGEquippedWeaponBaseInfiniteDebugAmmoBypassesInventoryAmmoTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
		return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABG_EquippedWeaponBase* Weapon = World->SpawnActor<ABG_EquippedWeaponBase>(
		ABG_EquippedWeaponBase::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("Equipped weapon actor is spawned."), Weapon);
	if (!Weapon)
		return false;

	Weapon->ApplyWeaponRuntimeDefinition(EBGWeaponPoseType::Rifle, FGameplayTag(), 30, 0);
	Weapon->SetUseInfiniteDebugAmmo(true);

	TestTrue(TEXT("Infinite debug ammo can be enabled."),
	         Weapon->UsesInfiniteDebugAmmo());
	TestTrue(TEXT("Infinite debug ammo can reload with no inventory ammo."),
	         Weapon->CanReloadWithInventoryAmmo(0));
	TestEqual(TEXT("Infinite debug ammo resolves missing magazine ammo without inventory ammo."),
	          Weapon->ResolveReloadAmount(0), 30);
	TestTrue(TEXT("Reload can begin with infinite debug ammo."),
	         Weapon->BeginWeaponReload());
	Weapon->FinishWeaponReload(Weapon->ResolveReloadAmount(0));
	TestEqual(TEXT("Infinite debug ammo fills loaded ammo without inventory ammo."),
	          Weapon->GetLoadedAmmo(), 30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGEquippedWeaponBaseStrictReloadConsumesInventoryAmmoTest,
	"PUBG_HotMode.GEONU.Combat.EquippedWeaponBase.StrictReloadConsumesInventoryAmmo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGEquippedWeaponBaseStrictReloadConsumesInventoryAmmoTest::RunTest(const FString& Parameters)
{
	const FGameplayTag AmmoTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Ammo.556")), false);
	TestTrue(TEXT("GameplayTag exists: Item.Ammo.556"), AmmoTag.IsValid());
	if (!AmmoTag.IsValid())
		return false;

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = WorldWrapper.GetTestWorld();
	TestNotNull(TEXT("Test world is created."), World);
	if (!World)
		return false;

	UGameInstance* GameInstance = World->GetGameInstance();
	TestNotNull(TEXT("Test world has a game instance."), GameInstance);
	if (!GameInstance)
		return false;

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem =
		GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	TestNotNull(TEXT("Item data registry subsystem is available."), RegistrySubsystem);
	if (!RegistrySubsystem)
		return false;

	if (!TestTrue(TEXT("Item data registry is loaded."),
	              RegistrySubsystem->IsRegistryLoaded() || RegistrySubsystem->LoadRegistry()))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* InventoryOwner = World->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Inventory owner actor is spawned."), InventoryOwner);
	if (!InventoryOwner)
		return false;

	UBG_InventoryComponent* InventoryComponent = NewObject<UBG_InventoryComponent>(InventoryOwner);
	TestNotNull(TEXT("Inventory component is created."), InventoryComponent);
	if (!InventoryComponent)
		return false;

	InventoryOwner->AddInstanceComponent(InventoryComponent);
	InventoryComponent->RegisterComponent();

	ABG_EquippedWeaponBase* Weapon = World->SpawnActor<ABG_EquippedWeaponBase>(
		ABG_EquippedWeaponBase::StaticClass(), FTransform::Identity, SpawnParameters);
	TestNotNull(TEXT("Equipped weapon actor is spawned."), Weapon);
	if (!Weapon)
		return false;

	int32 AddedQuantity = 0;
	TestTrue(TEXT("Inventory reserve ammo can be added for strict reload."),
	         InventoryComponent->Auth_AddItem(EBG_ItemType::Ammo, AmmoTag, 12, AddedQuantity));
	TestEqual(TEXT("Reserve ammo setup adds the requested ammo quantity."), AddedQuantity, 12);

	Weapon->ApplyWeaponRuntimeDefinition(EBGWeaponPoseType::Rifle, AmmoTag, 30, 0);
	TestTrue(TEXT("Strict reload can start with inventory reserve ammo."),
	         Weapon->CanReloadWithInventoryAmmo(InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoTag)));
	TestTrue(TEXT("Weapon reload enters reload state."), Weapon->BeginWeaponReload());

	const int32 AmmoToLoad = Weapon->ResolveReloadAmount(
		InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoTag));
	TestEqual(TEXT("Strict reload resolves the available reserve ammo quantity."), AmmoToLoad, 12);

	int32 RemovedQuantity = 0;
	TestTrue(TEXT("Strict reload consumes reserve ammo from inventory."),
	         InventoryComponent->Auth_RemoveItem(
		         EBG_ItemType::Ammo, AmmoTag, AmmoToLoad, RemovedQuantity));
	TestEqual(TEXT("Strict reload removes exactly the loaded ammo quantity."), RemovedQuantity, AmmoToLoad);

	Weapon->FinishWeaponReload(RemovedQuantity);
	TestEqual(TEXT("Strict reload increases loaded ammo by consumed reserve ammo."),
	          Weapon->GetLoadedAmmo(), 12);
	TestEqual(TEXT("Strict reload leaves no reserve ammo after consuming all available ammo."),
	          InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoTag), 0);
	TestFalse(TEXT("Strict reload finishes the weapon reload state."), Weapon->IsReloadingWeapon());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
