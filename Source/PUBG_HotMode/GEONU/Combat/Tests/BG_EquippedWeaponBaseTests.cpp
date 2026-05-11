// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/BG_EquippedWeaponBase.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
