// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Combat/BG_HealthComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemUseComponent.h"
#include "Player/BG_Character.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGItemUseMovementDoesNotCancelAndSlowsCharacterTest,
	"PUBG_HotMode.GEONU.Inventory.ItemUse.MovementDoesNotCancelAndSlowsCharacter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGItemUseMovementDoesNotCancelAndSlowsCharacterTest::RunTest(const FString& Parameters)
{
	const FGameplayTag BandageTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Item.Heal.Bandage")), false);
	TestTrue(TEXT("GameplayTag exists: Item.Heal.Bandage"), BandageTag.IsValid());
	if (!BandageTag.IsValid())
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
	ABG_Character* Character = World->SpawnActor<ABG_Character>(
		ABG_Character::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("Character is spawned."), Character);
	if (!Character)
		return false;

	UBG_HealthComponent* HealthComponent = Character->GetHealthComponent();
	UBG_InventoryComponent* InventoryComponent = Character->GetInventoryComponent();
	UBG_ItemUseComponent* ItemUseComponent = Character->GetItemUseComponent();
	UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	TestNotNull(TEXT("Character has health component."), HealthComponent);
	TestNotNull(TEXT("Character has inventory component."), InventoryComponent);
	TestNotNull(TEXT("Character has item use component."), ItemUseComponent);
	TestNotNull(TEXT("Character has movement component."), MovementComponent);
	if (!(HealthComponent && InventoryComponent && ItemUseComponent && MovementComponent))
		return false;

	TestTrue(TEXT("Health can be lowered below bandage heal cap."), HealthComponent->SetHealthPercent(0.5f));

	int32 AddedQuantity = 0;
	TestTrue(TEXT("Bandage can be added to inventory."),
	         InventoryComponent->Auth_AddItem(EBG_ItemType::Heal, BandageTag, 1, AddedQuantity));
	TestEqual(TEXT("One bandage is added."), AddedQuantity, 1);

	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	TestTrue(TEXT("Bandage use can start."), ItemUseComponent->Auth_StartUseItem(
		         EBG_ItemType::Heal,
		         BandageTag,
		         FailReason));
	TestEqual(TEXT("Bandage use has no failure reason."), FailReason, EBGInventoryFailReason::None);
	TestTrue(TEXT("Item use remains active after start."), ItemUseComponent->IsUsingItem());
	TestEqual(TEXT("Item use applies standing movement speed penalty."), MovementComponent->MaxWalkSpeed, 120.f);

	ItemUseComponent->NotifyMovementInput();
	TestTrue(TEXT("Movement input does not cancel item use."), ItemUseComponent->IsUsingItem());
	TestEqual(TEXT("Movement input keeps item-use movement speed penalty."), MovementComponent->MaxWalkSpeed, 120.f);

	ItemUseComponent->CancelItemUse();
	TestFalse(TEXT("Explicit cancel stops item use."), ItemUseComponent->IsUsingItem());
	TestEqual(TEXT("Cancel restores standing movement speed."), MovementComponent->MaxWalkSpeed, 600.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
