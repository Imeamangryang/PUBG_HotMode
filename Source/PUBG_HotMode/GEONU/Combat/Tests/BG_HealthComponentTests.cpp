// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Combat/BG_HealthComponent.h"
#include "Engine/World.h"
#include "Player/BG_Character.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGHealthComponentSetHealthPercentTest,
	"PUBG_HotMode.GEONU.Combat.HealthComponent.SetHealthPercentUpdatesHealthAndDeathState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGHealthComponentSetHealthPercentTest::RunTest(const FString& Parameters)
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
	ABG_Character* Character = World->SpawnActor<ABG_Character>(
		ABG_Character::StaticClass(),
		FTransform::Identity,
		SpawnParameters);
	TestNotNull(TEXT("Character is spawned."), Character);
	if (!Character)
		return false;

	UBG_HealthComponent* HealthComponent = Character->GetHealthComponent();
	TestNotNull(TEXT("Character has health component."), HealthComponent);
	if (!HealthComponent)
		return false;

	TestTrue(TEXT("Health percent can be set below max."), HealthComponent->SetHealthPercent(0.4f));
	TestEqual(TEXT("Current health is set from requested percent."), HealthComponent->GetCurrentHP(), 40.f);
	TestEqual(TEXT("Health percent getter reflects requested percent."), HealthComponent->GetHealthPercent(), 0.4f);
	TestFalse(TEXT("Positive health keeps character alive."), HealthComponent->IsDead());

	TestTrue(TEXT("Health percent can be set to one."), HealthComponent->SetHealthPercent(1.f));
	TestEqual(TEXT("Current health is set to max by full percent."), HealthComponent->GetCurrentHP(), HealthComponent->GetMaxHP());

	TestTrue(TEXT("Health percent can be set to zero."), HealthComponent->SetHealthPercent(0.f));
	TestEqual(TEXT("Zero health is stored."), HealthComponent->GetCurrentHP(), 0.f);
	TestTrue(TEXT("Zero health marks character dead."), HealthComponent->IsDead());

	TestTrue(TEXT("Debug health percent set can revive from dead state."), HealthComponent->SetHealthPercent(0.25f));
	TestEqual(TEXT("Revived health is set from requested percent."), HealthComponent->GetCurrentHP(), 25.f);
	TestFalse(TEXT("Positive debug health percent clears dead state."), HealthComponent->IsDead());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
