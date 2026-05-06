// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BG_EquippedWeaponBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

ABG_EquippedWeaponBase::ABG_EquippedWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	WeaponRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	SetRootComponent(WeaponRootComponent);

	EquippedMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedMesh"));
	EquippedMeshComponent->SetupAttachment(WeaponRootComponent);
	EquippedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedMeshComponent->SetGenerateOverlapEvents(false);
}

namespace
{
bool ResolveEquippedMeshSocketTransform(
	const ABG_EquippedWeaponBase* WeaponActor,
	const UStaticMeshComponent* EquippedMeshComponent,
	FName SocketName,
	const TCHAR* OperationName,
	FTransform& OutSocketTransform)
{
	if (SocketName.IsNone())
	{
		LOGE(TEXT("%s: %s failed because SocketName was none."), *GetNameSafe(WeaponActor), OperationName);
		return false;
	}

	if (!EquippedMeshComponent)
	{
		LOGE(TEXT("%s: %s failed because EquippedMeshComponent was null for socket %s."),
		     *GetNameSafe(WeaponActor),
		     OperationName,
		     *SocketName.ToString());
		return false;
	}

	if (!EquippedMeshComponent->DoesSocketExist(SocketName))
	{
		LOGE(TEXT("%s: %s failed because socket %s does not exist on %s."),
		     *GetNameSafe(WeaponActor),
		     OperationName,
		     *SocketName.ToString(),
		     *GetNameSafe(EquippedMeshComponent));
		return false;
	}

	OutSocketTransform = EquippedMeshComponent->GetSocketTransform(SocketName);
	return true;
}
}

bool ABG_EquippedWeaponBase::InitializeEquippedWeapon(ABG_Character* NewOwningCharacter)
{
	if (!SetOwningCharacter(NewOwningCharacter))
		return false;

	NotifyEquipped();
	return true;
}

bool ABG_EquippedWeaponBase::SetOwningCharacter(ABG_Character* NewOwningCharacter)
{
	if (!ensureMsgf(IsValid(NewOwningCharacter),
	                TEXT("%s: SetOwningCharacter failed because NewOwningCharacter was null or invalid."),
	                *GetNameSafe(this)))
		return false;

	OwningCharacter = NewOwningCharacter;
	SetOwner(NewOwningCharacter);
	SetInstigator(NewOwningCharacter);

	OnOwningCharacterChanged(NewOwningCharacter);
	return true;
}

void ABG_EquippedWeaponBase::ClearOwningCharacter()
{
	OwningCharacter = nullptr;
	SetOwner(nullptr);
	SetInstigator(nullptr);

	OnOwningCharacterChanged(nullptr);
}

void ABG_EquippedWeaponBase::SetMuzzleSocketName(FName NewMuzzleSocketName)
{
	MuzzleSocketName = NewMuzzleSocketName;
}

bool ABG_EquippedWeaponBase::GetMuzzleTransform(FTransform& OutMuzzleTransform) const
{
	return ResolveEquippedMeshSocketTransform(
		this,
		EquippedMeshComponent,
		MuzzleSocketName,
		TEXT("GetMuzzleTransform"),
		OutMuzzleTransform);
}

void ABG_EquippedWeaponBase::SetGripSocketName(FName NewGripSocketName)
{
	GripSocketName = NewGripSocketName;
}

bool ABG_EquippedWeaponBase::GetGripTransform(FTransform& OutGripTransform) const
{
	return ResolveEquippedMeshSocketTransform(
		this,
		EquippedMeshComponent,
		GripSocketName,
		TEXT("GetGripTransform"),
		OutGripTransform);
}

void ABG_EquippedWeaponBase::SetSupportHandSocketName(FName NewSupportHandSocketName)
{
	SupportHandSocketName = NewSupportHandSocketName;
}

bool ABG_EquippedWeaponBase::GetSupportHandTransform(FTransform& OutSupportHandTransform) const
{
	return ResolveEquippedMeshSocketTransform(
		this,
		EquippedMeshComponent,
		SupportHandSocketName,
		TEXT("GetSupportHandTransform"),
		OutSupportHandTransform);
}

void ABG_EquippedWeaponBase::SetHandAttachTransform(const FTransform& NewHandAttachTransform)
{
	HandAttachTransform = NewHandAttachTransform;
}

void ABG_EquippedWeaponBase::SetBackAttachTransform(const FTransform& NewBackAttachTransform)
{
	BackAttachTransform = NewBackAttachTransform;
}

void ABG_EquippedWeaponBase::NotifyEquipped()
{
	OnEquipped();
}

void ABG_EquippedWeaponBase::NotifyUnequipped()
{
	OnUnequipped();
}

void ABG_EquippedWeaponBase::NotifyFireTriggered(const FTransform& MuzzleTransform, int32 ShotPredictionId)
{
	OnFireTriggered(MuzzleTransform, ShotPredictionId);
}

void ABG_EquippedWeaponBase::NotifyReloadStarted(float ReloadDuration)
{
	OnReloadStarted(ReloadDuration);
}

void ABG_EquippedWeaponBase::NotifyReloadFinished(bool bReloadSucceeded)
{
	OnReloadFinished(bReloadSucceeded);
}

void ABG_EquippedWeaponBase::OnOwningCharacterChanged_Implementation(ABG_Character* NewOwningCharacter)
{
}

void ABG_EquippedWeaponBase::OnEquipped_Implementation()
{
}

void ABG_EquippedWeaponBase::OnUnequipped_Implementation()
{
}

void ABG_EquippedWeaponBase::OnFireTriggered_Implementation(const FTransform& MuzzleTransform, int32 ShotPredictionId)
{
}

void ABG_EquippedWeaponBase::OnReloadStarted_Implementation(float ReloadDuration)
{
}

void ABG_EquippedWeaponBase::OnReloadFinished_Implementation(bool bReloadSucceeded)
{
}
