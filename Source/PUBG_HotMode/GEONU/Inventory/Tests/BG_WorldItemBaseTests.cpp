// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Inventory/BG_WorldItemBase.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBGWorldItemBaseDefaultPhysicsTest,
	"PUBG_HotMode.GEONU.Inventory.WorldItemBase.EnablesDefaultPhysics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBGWorldItemBaseDefaultPhysicsTest::RunTest(const FString& Parameters)
{
	const ABG_WorldItemBase* DefaultWorldItem = GetDefault<ABG_WorldItemBase>();
	const UStaticMeshComponent* WorldMesh = DefaultWorldItem->GetStaticMeshComponent();

	TestNotNull(TEXT("World mesh component is available."), WorldMesh);
	if (!WorldMesh)
		return false;

	TestEqual(TEXT("World mesh defaults to movable for physics simulation."),
	          WorldMesh->Mobility, EComponentMobility::Movable);
	TestTrue(TEXT("World mesh enables physics simulation by default."),
	         WorldMesh->BodyInstance.bSimulatePhysics);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
