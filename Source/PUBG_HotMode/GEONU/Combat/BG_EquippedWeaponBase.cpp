// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BG_EquippedWeaponBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
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

	WeaponPoseType = DefaultWeaponPoseType;
	AmmoItemTag = DefaultAmmoItemTag;
	MagazineCapacity = FMath::Max(0, DefaultMagazineCapacity);
	LoadedAmmo = MagazineCapacity;
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
	if (!IsValid(NewOwningCharacter))
	{
		LOGE(TEXT("%s: SetOwningCharacter failed because NewOwningCharacter was null or invalid."),
			*GetNameSafe(this));
		return false;
	}

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

void ABG_EquippedWeaponBase::ApplyWeaponRuntimeDefinition(
	EBGWeaponPoseType NewWeaponPoseType,
	const FGameplayTag& NewAmmoItemTag,
	int32 NewMagazineCapacity,
	int32 NewLoadedAmmo)
{
	WeaponPoseType = NewWeaponPoseType;
	AmmoItemTag = NewAmmoItemTag;
	MagazineCapacity = FMath::Max(0, NewMagazineCapacity);
	LoadedAmmo = FMath::Clamp(NewLoadedAmmo, 0, MagazineCapacity);
	bIsReloadingWeapon = false;
}

bool ABG_EquippedWeaponBase::CanFire(int32 AmmoCost) const
{
	return AmmoCost > 0
		&& !bIsReloadingWeapon
		&& LoadedAmmo >= AmmoCost;
}

bool ABG_EquippedWeaponBase::ConsumeLoadedAmmo(int32 AmmoCost)
{
	if (!CanFire(AmmoCost))
	{
		LOGE(TEXT("%s: ConsumeLoadedAmmo failed because AmmoCost=%d, LoadedAmmo=%d, bIsReloadingWeapon=%d."),
			*GetNameSafe(this),
			AmmoCost,
			LoadedAmmo,
			bIsReloadingWeapon ? 1 : 0);
		return false;
	}

	LoadedAmmo = FMath::Max(0, LoadedAmmo - AmmoCost);
	return true;
}

int32 ABG_EquippedWeaponBase::GetMissingAmmoCount() const
{
	return FMath::Max(0, MagazineCapacity - LoadedAmmo);
}

bool ABG_EquippedWeaponBase::CanReloadWithInventoryAmmo(int32 InventoryAmmoCount) const
{
	if (bIsReloadingWeapon || MagazineCapacity <= 0 || LoadedAmmo >= MagazineCapacity)
	{
		return false;
	}

	return bUseInfiniteDebugAmmo || InventoryAmmoCount > 0;
}

bool ABG_EquippedWeaponBase::BeginWeaponReload()
{
	if (bIsReloadingWeapon)
	{
		LOGE(TEXT("%s: BeginWeaponReload failed because a reload was already in progress."), *GetNameSafe(this));
		return false;
	}

	if (MagazineCapacity <= 0)
	{
		LOGE(TEXT("%s: BeginWeaponReload failed because MagazineCapacity was %d."), *GetNameSafe(this), MagazineCapacity);
		return false;
	}

	bIsReloadingWeapon = true;
	return true;
}

int32 ABG_EquippedWeaponBase::ResolveReloadAmount(int32 InventoryAmmoCount) const
{
	const int32 MissingAmmo = GetMissingAmmoCount();
	if (MissingAmmo <= 0)
	{
		return 0;
	}

	if (bUseInfiniteDebugAmmo)
	{
		return MissingAmmo;
	}

	return FMath::Clamp(InventoryAmmoCount, 0, MissingAmmo);
}

void ABG_EquippedWeaponBase::FinishWeaponReload(int32 ReloadedAmmo)
{
	if (!bIsReloadingWeapon)
	{
		LOGE(TEXT("%s: FinishWeaponReload failed because no reload was active."), *GetNameSafe(this));
		return;
	}

	LoadedAmmo = FMath::Clamp(LoadedAmmo + FMath::Max(0, ReloadedAmmo), 0, MagazineCapacity);
	bIsReloadingWeapon = false;
}

void ABG_EquippedWeaponBase::CancelWeaponReload()
{
	bIsReloadingWeapon = false;
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

void ABG_EquippedWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_EquippedWeaponBase, WeaponPoseType);
	DOREPLIFETIME(ABG_EquippedWeaponBase, AmmoItemTag);
	DOREPLIFETIME(ABG_EquippedWeaponBase, MagazineCapacity);
	DOREPLIFETIME(ABG_EquippedWeaponBase, LoadedAmmo);
	DOREPLIFETIME(ABG_EquippedWeaponBase, bIsReloadingWeapon);
}
