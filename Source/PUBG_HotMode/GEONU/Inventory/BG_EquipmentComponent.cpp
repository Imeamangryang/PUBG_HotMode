// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_EquipmentComponent.h"

#include "BG_InventoryComponent.h"
#include "BG_ItemDataRegistrySubsystem.h"
#include "BG_ItemDataRow.h"
#include "BG_WorldItemBase.h"
#include "Combat/BG_EquippedWeaponBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

namespace
{
EBGWeaponPoseType ResolveCharacterWeaponPoseType(const FBG_WeaponItemDataRow& WeaponRow)
{
	switch (WeaponRow.WeaponPoseCategory)
	{
	case EBG_WeaponPoseCategory::Pistol:
		return EBGWeaponPoseType::Pistol;
	case EBG_WeaponPoseCategory::Rifle:
		return EBGWeaponPoseType::Rifle;
	case EBG_WeaponPoseCategory::Shotgun:
		return EBGWeaponPoseType::Shotgun;
	case EBG_WeaponPoseCategory::Sniper:
		return EBGWeaponPoseType::Sniper;
	default:
		return EBGWeaponPoseType::None;
	}
}
}

UBG_EquipmentComponent::UBG_EquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WorldItemClass = ABG_WorldItemBase::StaticClass();
}

void UBG_EquipmentComponent::OnRegister()
{
	Super::OnRegister();
	CacheOwnerComponents();
}

void UBG_EquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheOwnerComponents();

	if (CachedInventory)
	{
		CachedInventory->OnInventoryItemQuantityChanged.AddUniqueDynamic(
			this, &UBG_EquipmentComponent::HandleInventoryItemQuantityChanged);
	}

	ApplyActiveWeaponStateToCharacter();
}

void UBG_EquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBG_EquipmentComponent, PublicState);
	DOREPLIFETIME_CONDITION(UBG_EquipmentComponent, OwnerState, COND_OwnerOnly);
	DOREPLIFETIME(UBG_EquipmentComponent, EquippedWeaponActors);
}

UBG_EquipmentComponent* UBG_EquipmentComponent::FindEquipmentComponent(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		LOGE(TEXT("FindEquipmentComponent failed because target actor was null or invalid."));
		return nullptr;
	}

	UBG_EquipmentComponent* EquipmentComponent = TargetActor->FindComponentByClass<UBG_EquipmentComponent>();
	if (!EquipmentComponent)
	{
		LOGE(TEXT("FindEquipmentComponent failed because %s has no equipment component."),
		     *GetNameSafe(TargetActor));
		return nullptr;
	}

	return EquipmentComponent;
}

bool UBG_EquipmentComponent::Auth_EquipWeapon(EBG_EquipmentSlot WeaponSlot, FGameplayTag WeaponItemTag,
                                            int32 LoadedAmmo, FGameplayTag& OutReplacedWeaponItemTag,
                                            int32& OutReplacedLoadedAmmo)
{
	OutReplacedWeaponItemTag = FGameplayTag();
	OutReplacedLoadedAmmo = 0;

	if (!CanMutateEquipmentState(TEXT("Auth_EquipWeapon")))
		return false;

	if (!IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: Auth_EquipWeapon failed because %s is not a weapon slot."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(WeaponItemTag, TEXT("Auth_EquipWeapon"));
	if (!WeaponRow)
		return false;

	if (!IsWeaponSlotCompatible(WeaponSlot, *WeaponRow))
	{
		LOGE(TEXT("%s: Auth_EquipWeapon failed because weapon %s is not compatible with slot %s."),
		     *GetNameSafe(this), *WeaponItemTag.ToString(), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	ABG_EquippedWeaponBase* NewWeaponActor = nullptr;
	if (!SpawnAndAttachEquippedWeaponActor(WeaponSlot, *WeaponRow, NewWeaponActor))
		return false;

	NewWeaponActor->ApplyWeaponRuntimeDefinition(
		ResolveCharacterWeaponPoseType(*WeaponRow),
		WeaponRow->AmmoItemTag,
		WeaponRow->MagazineSize,
		FMath::Clamp(LoadedAmmo, 0, FMath::Max(0, WeaponRow->MagazineSize)));

	OutReplacedWeaponItemTag = GetEquippedItemTag(WeaponSlot);
	OutReplacedLoadedAmmo = GetLoadedAmmo(WeaponSlot);
	ABG_EquippedWeaponBase* ReplacedWeaponActor = GetEquippedWeaponActor(WeaponSlot);

	SetEquippedItemTag(WeaponSlot, WeaponItemTag);
	SetLoadedAmmo(WeaponSlot, FMath::Clamp(LoadedAmmo, 0, FMath::Max(0, WeaponRow->MagazineSize)));
	SetEquippedWeaponActor(WeaponSlot, NewWeaponActor);

	if (PublicState.ActiveWeaponSlot == EBG_EquipmentSlot::None || !GetActiveWeaponItemTag().IsValid())
	{
		PublicState.ActiveWeaponSlot = WeaponSlot;
	}

	NormalizeActiveWeaponSlot();
	RefreshEquippedWeaponAttachments();
	DestroyEquippedWeaponActorInstance(ReplacedWeaponActor);
	ApplyActiveWeaponStateToCharacter();
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::Auth_UnequipWeapon(EBG_EquipmentSlot WeaponSlot,
                                              FGameplayTag& OutRemovedWeaponItemTag,
                                              int32& OutRemovedLoadedAmmo)
{
	OutRemovedWeaponItemTag = FGameplayTag();
	OutRemovedLoadedAmmo = 0;

	if (!CanMutateEquipmentState(TEXT("Auth_UnequipWeapon")))
		return false;

	if (!IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: Auth_UnequipWeapon failed because %s is not a weapon slot."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	if (!HasEquippedItem(WeaponSlot))
	{
		LOGE(TEXT("%s: Auth_UnequipWeapon failed because slot %s is empty."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	OutRemovedWeaponItemTag = GetEquippedItemTag(WeaponSlot);
	OutRemovedLoadedAmmo = GetLoadedAmmo(WeaponSlot);
	ABG_EquippedWeaponBase* RemovedWeaponActor = GetEquippedWeaponActor(WeaponSlot);

	SetEquippedItemTag(WeaponSlot, FGameplayTag());
	SetLoadedAmmo(WeaponSlot, 0);
	SetEquippedWeaponActor(WeaponSlot, nullptr);

	if (PublicState.ActiveWeaponSlot == WeaponSlot)
	{
		PublicState.ActiveWeaponSlot = EBG_EquipmentSlot::None;
	}

	NormalizeActiveWeaponSlot();
	RefreshEquippedWeaponAttachments();
	DestroyEquippedWeaponActorInstance(RemovedWeaponActor);
	ApplyActiveWeaponStateToCharacter();
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
	ForceEquipmentNetUpdate();
	return true;
}

void UBG_EquipmentComponent::ActivateWeapon(EBG_EquipmentSlot WeaponSlot)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Auth_ActivateWeapon(WeaponSlot);
		return;
	}

	Server_ActivateWeapon(WeaponSlot);
}

void UBG_EquipmentComponent::SwapWeaponSlots(EBG_EquipmentSlot SourceSlot, EBG_EquipmentSlot TargetSlot)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
		if (!Auth_SwapWeaponSlots(SourceSlot, TargetSlot, FailReason))
		{
			NotifyEquipmentFailure(FailReason, SourceSlot);
		}
		return;
	}

	Server_SwapWeaponSlots(SourceSlot, TargetSlot);
}

bool UBG_EquipmentComponent::Auth_ActivateWeapon(EBG_EquipmentSlot WeaponSlot)
{
	if (!CanMutateEquipmentState(TEXT("Auth_ActivateWeapon")))
		return false;

	if (WeaponSlot != EBG_EquipmentSlot::None && !IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: Auth_ActivateWeapon failed because %s is not a weapon slot."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	if (WeaponSlot != EBG_EquipmentSlot::None && !HasEquippedItem(WeaponSlot))
	{
		LOGE(TEXT("%s: Auth_ActivateWeapon failed because slot %s is empty."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	PublicState.ActiveWeaponSlot = WeaponSlot;
	NormalizeActiveWeaponSlot();
	RefreshEquippedWeaponAttachments();
	ApplyActiveWeaponStateToCharacter();
	BroadcastActiveWeaponSlotChanged();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::Auth_SwapWeaponSlots(
	EBG_EquipmentSlot SourceSlot,
	EBG_EquipmentSlot TargetSlot,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (!CanMutateEquipmentState(TEXT("Auth_SwapWeaponSlots")))
		return false;

	if (!IsPrimaryWeaponSlot(SourceSlot) || !IsPrimaryWeaponSlot(TargetSlot) || SourceSlot == TargetSlot)
	{
		LOGE(TEXT("%s: Auth_SwapWeaponSlots failed because SourceSlot=%s and TargetSlot=%s are not a valid PrimaryA/B pair."),
		     *GetNameSafe(this), *GetEquipmentSlotName(SourceSlot), *GetEquipmentSlotName(TargetSlot));
		OutFailReason = EBGInventoryFailReason::SlotMismatch;
		return false;
	}

	if (!HasEquippedItem(SourceSlot) || !HasEquippedItem(TargetSlot))
	{
		LOGE(TEXT("%s: Auth_SwapWeaponSlots failed because SourceSlot=%s or TargetSlot=%s was empty."),
		     *GetNameSafe(this), *GetEquipmentSlotName(SourceSlot), *GetEquipmentSlotName(TargetSlot));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	ABG_EquippedWeaponBase* SourceWeaponActor = GetEquippedWeaponActor(SourceSlot);
	ABG_EquippedWeaponBase* TargetWeaponActor = GetEquippedWeaponActor(TargetSlot);
	if (!IsValid(SourceWeaponActor) || !IsValid(TargetWeaponActor))
	{
		LOGE(TEXT("%s: Auth_SwapWeaponSlots failed because SourceActor=%s or TargetActor=%s was invalid."),
		     *GetNameSafe(this), *GetNameSafe(SourceWeaponActor), *GetNameSafe(TargetWeaponActor));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	const FGameplayTag SourceItemTag = GetEquippedItemTag(SourceSlot);
	const FGameplayTag TargetItemTag = GetEquippedItemTag(TargetSlot);
	const int32 SourceLoadedAmmo = SourceWeaponActor->GetLoadedAmmo();
	const int32 TargetLoadedAmmo = TargetWeaponActor->GetLoadedAmmo();

	SetEquippedItemTag(SourceSlot, TargetItemTag);
	SetEquippedItemTag(TargetSlot, SourceItemTag);
	SetLoadedAmmo(SourceSlot, TargetLoadedAmmo);
	SetLoadedAmmo(TargetSlot, SourceLoadedAmmo);
	SetEquippedWeaponActor(SourceSlot, TargetWeaponActor);
	SetEquippedWeaponActor(TargetSlot, SourceWeaponActor);

	if (PublicState.ActiveWeaponSlot == SourceSlot)
	{
		PublicState.ActiveWeaponSlot = TargetSlot;
	}
	else if (PublicState.ActiveWeaponSlot == TargetSlot)
	{
		PublicState.ActiveWeaponSlot = SourceSlot;
	}

	NormalizeActiveWeaponSlot();
	RefreshEquippedWeaponAttachments();
	ApplyActiveWeaponStateToCharacter();
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
	ForceEquipmentNetUpdate();
	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_EquipmentComponent::Auth_LoadAmmo(EBG_EquipmentSlot WeaponSlot, int32 LoadedAmmo)
{
	if (!CanMutateEquipmentState(TEXT("Auth_LoadAmmo")))
		return false;

	if (!IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: Auth_LoadAmmo failed because %s is not a weapon slot."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FGameplayTag WeaponItemTag = GetEquippedItemTag(WeaponSlot);
	if (!WeaponItemTag.IsValid())
	{
		LOGE(TEXT("%s: Auth_LoadAmmo failed because slot %s is empty."),
		     *GetNameSafe(this), *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(WeaponItemTag, TEXT("Auth_LoadAmmo"));
	if (!WeaponRow)
		return false;

	// Equipment keeps the replicated slot snapshot, but mirrors the same value back into the live weapon actor.
	const int32 SafeLoadedAmmo = FMath::Clamp(LoadedAmmo, 0, FMath::Max(0, WeaponRow->MagazineSize));
	SetLoadedAmmo(WeaponSlot, SafeLoadedAmmo);

	if (ABG_EquippedWeaponBase* WeaponActor = GetEquippedWeaponActor(WeaponSlot))
	{
		WeaponActor->ApplyWeaponRuntimeDefinition(
			ResolveCharacterWeaponPoseType(*WeaponRow),
			WeaponRow->AmmoItemTag,
			WeaponRow->MagazineSize,
			SafeLoadedAmmo);
	}
	else
	{
		LOGE(TEXT("%s: Auth_LoadAmmo could not mirror loaded ammo because WeaponActor was null for slot %s."),
			*GetNameSafe(this),
			*GetEquipmentSlotName(WeaponSlot));
	}

	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::Auth_EquipArmor(EBG_EquipmentSlot ArmorSlot, FGameplayTag ArmorItemTag,
                                           FGameplayTag& OutReplacedArmorItemTag,
                                           float& OutReplacedArmorDurability)
{
	OutReplacedArmorItemTag = FGameplayTag();
	OutReplacedArmorDurability = 0.f;

	if (!CanMutateEquipmentState(TEXT("Auth_EquipArmor")))
		return false;

	if (!IsArmorSlot(ArmorSlot))
	{
		LOGE(TEXT("%s: Auth_EquipArmor failed because %s is not an armor slot."),
		     *GetNameSafe(this), *GetEquipmentSlotName(ArmorSlot));
		return false;
	}

	const FBG_ArmorItemDataRow* ArmorRow = FindArmorItemRow(ArmorItemTag, TEXT("Auth_EquipArmor"));
	if (!ArmorRow)
		return false;

	if (!IsArmorSlotCompatible(ArmorSlot, *ArmorRow))
	{
		LOGE(TEXT("%s: Auth_EquipArmor failed because armor %s is not compatible with slot %s."),
		     *GetNameSafe(this), *ArmorItemTag.ToString(), *GetEquipmentSlotName(ArmorSlot));
		return false;
	}

	OutReplacedArmorItemTag = GetEquippedItemTag(ArmorSlot);
	OutReplacedArmorDurability = GetArmorDurability(ArmorSlot);

	SetEquippedItemTag(ArmorSlot, ArmorItemTag);
	SetArmorDurability(ArmorSlot, FMath::Max(0.f, ArmorRow->MaxDurability));
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::Auth_UnequipArmor(EBG_EquipmentSlot ArmorSlot,
                                             FGameplayTag& OutRemovedArmorItemTag,
                                             float& OutRemovedArmorDurability)
{
	OutRemovedArmorItemTag = FGameplayTag();
	OutRemovedArmorDurability = 0.f;

	if (!CanMutateEquipmentState(TEXT("Auth_UnequipArmor")))
		return false;

	if (!IsArmorSlot(ArmorSlot))
	{
		LOGE(TEXT("%s: Auth_UnequipArmor failed because %s is not an armor slot."),
		     *GetNameSafe(this), *GetEquipmentSlotName(ArmorSlot));
		return false;
	}

	if (!HasEquippedItem(ArmorSlot))
	{
		LOGE(TEXT("%s: Auth_UnequipArmor failed because slot %s is empty."),
		     *GetNameSafe(this), *GetEquipmentSlotName(ArmorSlot));
		return false;
	}

	OutRemovedArmorItemTag = GetEquippedItemTag(ArmorSlot);
	OutRemovedArmorDurability = GetArmorDurability(ArmorSlot);

	SetEquippedItemTag(ArmorSlot, FGameplayTag());
	SetArmorDurability(ArmorSlot, 0.f);
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::Auth_EquipBackpack(FGameplayTag BackpackItemTag,
                                              FGameplayTag& OutReplacedBackpackItemTag)
{
	OutReplacedBackpackItemTag = FGameplayTag();

	if (!CanMutateEquipmentState(TEXT("Auth_EquipBackpack")))
		return false;

	const FBG_BackpackItemDataRow* BackpackRow = FindBackpackItemRow(BackpackItemTag, TEXT("Auth_EquipBackpack"));
	if (!BackpackRow)
		return false;

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("Auth_EquipBackpack"));
	if (!InventoryComponent)
		return false;

	bool bSuccess = InventoryComponent->Auth_SetBackpackBonus(BackpackRow->WeightBonus);
	if (!bSuccess)
	{
		LOGW(TEXT("%s: Auth_EquipBackpack rejected backpack %s because inventory weight limit update failed."),
		     *GetNameSafe(this),
		     *BackpackItemTag.ToString());
		return false;
	}

	OutReplacedBackpackItemTag = PublicState.BackpackItemTag;
	PublicState.BackpackItemTag = BackpackItemTag;
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::Auth_UnequipBackpack(FGameplayTag& OutRemovedBackpackItemTag)
{
	OutRemovedBackpackItemTag = FGameplayTag();

	if (!CanMutateEquipmentState(TEXT("Auth_UnequipBackpack")))
		return false;

	if (!PublicState.BackpackItemTag.IsValid())
	{
		LOGE(TEXT("%s: Auth_UnequipBackpack failed because backpack slot is empty."), *GetNameSafe(this));
		return false;
	}

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("Auth_UnequipBackpack"));
	if (!InventoryComponent)
		return false;

	bool bSuccess = InventoryComponent->Auth_SetBackpackBonus(0.f);
	if (!bSuccess)
	{
		LOGW(TEXT("%s: Auth_UnequipBackpack rejected because current inventory weight exceeds base capacity."),
		     *GetNameSafe(this));
		return false;
	}

	OutRemovedBackpackItemTag = PublicState.BackpackItemTag;
	PublicState.BackpackItemTag = FGameplayTag();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

void UBG_EquipmentComponent::SetThrowable(FGameplayTag ThrowableItemTag)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Auth_SetThrowable(ThrowableItemTag);
		return;
	}

	Server_SetThrowable(ThrowableItemTag);
}

bool UBG_EquipmentComponent::Auth_SetThrowable(FGameplayTag ThrowableItemTag)
{
	if (!CanMutateEquipmentState(TEXT("Auth_SetThrowable")))
		return false;

	if (!FindThrowableItemRow(ThrowableItemTag, TEXT("Auth_SetThrowable")))
		return false;

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("Auth_SetThrowable"));
	if (!InventoryComponent)
		return false;

	const int32 ThrowableQuantity = InventoryComponent->GetQuantity(EBG_ItemType::Throwable, ThrowableItemTag);
	if (ThrowableQuantity <= 0)
	{
		LOGE(TEXT("%s: Auth_SetThrowable failed because inventory has no throwable %s."),
		     *GetNameSafe(this), *ThrowableItemTag.ToString());
		return false;
	}

	PublicState.ThrowableItemTag = ThrowableItemTag;
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

void UBG_EquipmentComponent::ClearThrowable()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Auth_ClearThrowable();
		return;
	}

	Server_ClearThrowable();
}

bool UBG_EquipmentComponent::Auth_ClearThrowable()
{
	if (!CanMutateEquipmentState(TEXT("Auth_ClearThrowable")))
		return false;

	if (!PublicState.ThrowableItemTag.IsValid())
		return true;

	PublicState.ThrowableItemTag = FGameplayTag();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

void UBG_EquipmentComponent::DropEquipment(EBG_EquipmentSlot EquipmentSlot)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
		if (!Auth_DropEquipment(EquipmentSlot, FailReason))
		{
			NotifyEquipmentFailure(FailReason, EquipmentSlot);
		}
		return;
	}

	Server_DropEquipment(EquipmentSlot);
}

bool UBG_EquipmentComponent::Auth_DropEquipment(
	EBG_EquipmentSlot EquipmentSlot,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (!CanMutateEquipmentState(TEXT("Auth_DropEquipment")))
		return false;

	if (IsWeaponSlot(EquipmentSlot))
	{
		return Auth_DropWeapon(EquipmentSlot, OutFailReason);
	}

	if (IsArmorSlot(EquipmentSlot))
	{
		return Auth_DropArmor(EquipmentSlot, OutFailReason);
	}

	if (EquipmentSlot == EBG_EquipmentSlot::Backpack)
	{
		return Auth_DropBackpack(OutFailReason);
	}

	if (EquipmentSlot == EBG_EquipmentSlot::Throwable)
	{
		return Auth_DropThrowable(OutFailReason);
	}

	LOGE(TEXT("%s: Auth_DropEquipment failed because slot %s is not droppable."),
	     *GetNameSafe(this), *GetEquipmentSlotName(EquipmentSlot));
	OutFailReason = EBGInventoryFailReason::SlotMismatch;
	return false;
}

bool UBG_EquipmentComponent::Auth_DropWeapon(
	EBG_EquipmentSlot WeaponSlot,
	EBGInventoryFailReason& OutFailReason)
{
	FGameplayTag RemovedWeaponItemTag;
	int32 RemovedLoadedAmmo = 0;
	if (!Auth_UnequipWeapon(WeaponSlot, RemovedWeaponItemTag, RemovedLoadedAmmo))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!SpawnDroppedEquipmentItem(EBG_ItemType::Weapon, RemovedWeaponItemTag, RemovedLoadedAmmo))
	{
		FGameplayTag RollbackReplacedWeaponItemTag;
		int32 RollbackReplacedLoadedAmmo = 0;
		Auth_EquipWeapon(WeaponSlot, RemovedWeaponItemTag, RemovedLoadedAmmo,
		                 RollbackReplacedWeaponItemTag, RollbackReplacedLoadedAmmo);
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_EquipmentComponent::Auth_DropArmor(
	EBG_EquipmentSlot ArmorSlot,
	EBGInventoryFailReason& OutFailReason)
{
	FGameplayTag RemovedArmorItemTag;
	float RemovedArmorDurability = 0.f;
	if (!Auth_UnequipArmor(ArmorSlot, RemovedArmorItemTag, RemovedArmorDurability))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!SpawnDroppedEquipmentItem(EBG_ItemType::Armor, RemovedArmorItemTag, 0))
	{
		FGameplayTag RollbackReplacedArmorItemTag;
		float RollbackReplacedArmorDurability = 0.f;
		if (Auth_EquipArmor(ArmorSlot, RemovedArmorItemTag, RollbackReplacedArmorItemTag,
		                    RollbackReplacedArmorDurability))
		{
			SetArmorDurability(ArmorSlot, RemovedArmorDurability);
		}
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_EquipmentComponent::Auth_DropBackpack(EBGInventoryFailReason& OutFailReason)
{
	FGameplayTag RemovedBackpackItemTag;
	if (!Auth_UnequipBackpack(RemovedBackpackItemTag))
	{
		OutFailReason = EBGInventoryFailReason::Overweight;
		return false;
	}

	if (!SpawnDroppedEquipmentItem(EBG_ItemType::Backpack, RemovedBackpackItemTag, 0))
	{
		FGameplayTag RollbackReplacedBackpackItemTag;
		Auth_EquipBackpack(RemovedBackpackItemTag, RollbackReplacedBackpackItemTag);
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_EquipmentComponent::Auth_DropThrowable(EBGInventoryFailReason& OutFailReason)
{
	if (!PublicState.ThrowableItemTag.IsValid())
	{
		LOGE(TEXT("%s: Auth_DropThrowable failed because throwable slot is empty."), *GetNameSafe(this));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("Auth_DropThrowable"));
	if (!InventoryComponent)
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	return InventoryComponent->Auth_DropItem(
		EBG_ItemType::Throwable,
		PublicState.ThrowableItemTag,
		1,
		OutFailReason);
}

FGameplayTag UBG_EquipmentComponent::GetEquippedItemTag(EBG_EquipmentSlot Slot) const
{
	switch (Slot)
	{
	case EBG_EquipmentSlot::PrimaryA:
		return PublicState.PrimaryAWeaponTag;
	case EBG_EquipmentSlot::PrimaryB:
		return PublicState.PrimaryBWeaponTag;
	case EBG_EquipmentSlot::Pistol:
		return PublicState.PistolWeaponTag;
	case EBG_EquipmentSlot::Melee:
		return PublicState.MeleeWeaponTag;
	case EBG_EquipmentSlot::Throwable:
		return PublicState.ThrowableItemTag;
	case EBG_EquipmentSlot::Helmet:
		return PublicState.HelmetItemTag;
	case EBG_EquipmentSlot::Vest:
		return PublicState.VestItemTag;
	case EBG_EquipmentSlot::Backpack:
		return PublicState.BackpackItemTag;
	default:
		return FGameplayTag();
	}
}

bool UBG_EquipmentComponent::HasEquippedItem(EBG_EquipmentSlot Slot) const
{
	return GetEquippedItemTag(Slot).IsValid();
}

FGameplayTag UBG_EquipmentComponent::GetActiveWeaponItemTag() const
{
	return GetEquippedItemTag(PublicState.ActiveWeaponSlot);
}

EBG_EquipmentSlot UBG_EquipmentComponent::GetSlotForEquippedWeaponTag(FGameplayTag WeaponItemTag) const
{
	if (!WeaponItemTag.IsValid())
	{
		return EBG_EquipmentSlot::None;
	}

	for (const EBG_EquipmentSlot Slot : {
		     EBG_EquipmentSlot::PrimaryA, EBG_EquipmentSlot::PrimaryB, EBG_EquipmentSlot::Pistol,
		     EBG_EquipmentSlot::Melee
	     })
	{
		if (GetEquippedItemTag(Slot) == WeaponItemTag)
		{
			return Slot;
		}
	}

	return EBG_EquipmentSlot::None;
}

int32 UBG_EquipmentComponent::GetLoadedAmmo(EBG_EquipmentSlot WeaponSlot) const
{
	switch (WeaponSlot)
	{
	case EBG_EquipmentSlot::PrimaryA:
		return OwnerState.PrimaryALoadedAmmo;
	case EBG_EquipmentSlot::PrimaryB:
		return OwnerState.PrimaryBLoadedAmmo;
	case EBG_EquipmentSlot::Pistol:
		return OwnerState.PistolLoadedAmmo;
	case EBG_EquipmentSlot::Melee:
		return OwnerState.MeleeLoadedAmmo;
	default:
		return 0;
	}
}

ABG_EquippedWeaponBase* UBG_EquipmentComponent::GetEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot) const
{
	switch (WeaponSlot)
	{
	case EBG_EquipmentSlot::PrimaryA:
		return EquippedWeaponActors.PrimaryAWeaponActor;
	case EBG_EquipmentSlot::PrimaryB:
		return EquippedWeaponActors.PrimaryBWeaponActor;
	case EBG_EquipmentSlot::Pistol:
		return EquippedWeaponActors.PistolWeaponActor;
	case EBG_EquipmentSlot::Melee:
		return EquippedWeaponActors.MeleeWeaponActor;
	default:
		return nullptr;
	}
}

ABG_EquippedWeaponBase* UBG_EquipmentComponent::GetActiveEquippedWeaponActor() const
{
	return GetEquippedWeaponActor(PublicState.ActiveWeaponSlot);
}

float UBG_EquipmentComponent::GetArmorDurability(EBG_EquipmentSlot ArmorSlot) const
{
	switch (ArmorSlot)
	{
	case EBG_EquipmentSlot::Helmet:
		return OwnerState.HelmetDurability;
	case EBG_EquipmentSlot::Vest:
		return OwnerState.VestDurability;
	default:
		return 0.f;
	}
}

void UBG_EquipmentComponent::OnRep_PublicState()
{
	RefreshEquippedWeaponAttachments();
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
}

void UBG_EquipmentComponent::OnRep_OwnerState()
{
	BroadcastEquipmentChanged();
}

void UBG_EquipmentComponent::OnRep_EquippedWeaponActors()
{
	RefreshEquippedWeaponAttachments();
	BroadcastEquipmentChanged();
}

void UBG_EquipmentComponent::HandleInventoryItemQuantityChanged(
	EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return;

	if (ItemType != EBG_ItemType::Throwable || Quantity > 0 || PublicState.ThrowableItemTag != ItemTag)
		return;

	PublicState.ThrowableItemTag = FGameplayTag();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
}

void UBG_EquipmentComponent::CacheOwnerComponents()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner was null."), *GetNameSafe(this));
		return;
	}

	CachedCharacter = Cast<ABG_Character>(Owner);
	if (!CachedCharacter)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s was not ABG_Character."),
		     *GetNameSafe(this), *GetNameSafe(Owner));
	}

	CachedInventory = Owner->FindComponentByClass<UBG_InventoryComponent>();
	if (!CachedInventory)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s had no inventory component."),
		     *GetNameSafe(this), *GetNameSafe(Owner));
	}
}

UBG_InventoryComponent* UBG_EquipmentComponent::GetInventoryComponentForEquipment(const TCHAR* OperationName) const
{
	if (CachedInventory)
	{
		return CachedInventory.Get();
	}

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: %s failed because owner was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_InventoryComponent* InventoryComponent = Owner->FindComponentByClass<UBG_InventoryComponent>();
	if (!InventoryComponent)
	{
		LOGE(TEXT("%s: %s failed because owner %s had no inventory component."),
		     *GetNameSafe(this), OperationName, *GetNameSafe(Owner));
		return nullptr;
	}

	return InventoryComponent;
}

bool UBG_EquipmentComponent::CanMutateEquipmentState(const TCHAR* OperationName) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: %s failed because owner was null."), *GetNameSafe(this), OperationName);
		return false;
	}

	if (!Owner->HasAuthority())
	{
		LOGE(TEXT("%s: %s failed because owner %s had no authority."),
		     *GetNameSafe(this), OperationName, *GetNameSafe(Owner));
		return false;
	}

	return true;
}

void UBG_EquipmentComponent::Server_ActivateWeapon_Implementation(EBG_EquipmentSlot WeaponSlot)
{
	Auth_ActivateWeapon(WeaponSlot);
}

void UBG_EquipmentComponent::Server_SwapWeaponSlots_Implementation(
	EBG_EquipmentSlot SourceSlot,
	EBG_EquipmentSlot TargetSlot)
{
	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	if (!Auth_SwapWeaponSlots(SourceSlot, TargetSlot, FailReason))
	{
		NotifyEquipmentFailure(FailReason, SourceSlot);
	}
}

void UBG_EquipmentComponent::Server_SetThrowable_Implementation(FGameplayTag ThrowableItemTag)
{
	Auth_SetThrowable(ThrowableItemTag);
}

void UBG_EquipmentComponent::Server_ClearThrowable_Implementation()
{
	Auth_ClearThrowable();
}

void UBG_EquipmentComponent::Server_DropEquipment_Implementation(EBG_EquipmentSlot EquipmentSlot)
{
	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	if (!Auth_DropEquipment(EquipmentSlot, FailReason))
	{
		NotifyEquipmentFailure(FailReason, EquipmentSlot);
	}
}

bool UBG_EquipmentComponent::IsWeaponSlot(EBG_EquipmentSlot Slot) const
{
	return Slot == EBG_EquipmentSlot::PrimaryA
		|| Slot == EBG_EquipmentSlot::PrimaryB
		|| Slot == EBG_EquipmentSlot::Pistol
		|| Slot == EBG_EquipmentSlot::Melee;
}

bool UBG_EquipmentComponent::IsPrimaryWeaponSlot(EBG_EquipmentSlot Slot) const
{
	return Slot == EBG_EquipmentSlot::PrimaryA || Slot == EBG_EquipmentSlot::PrimaryB;
}

bool UBG_EquipmentComponent::IsArmorSlot(EBG_EquipmentSlot Slot) const
{
	return Slot == EBG_EquipmentSlot::Helmet || Slot == EBG_EquipmentSlot::Vest;
}

bool UBG_EquipmentComponent::IsWeaponSlotCompatible(EBG_EquipmentSlot Slot,
                                                    const FBG_WeaponItemDataRow& WeaponRow) const
{
	switch (WeaponRow.EquipSlot)
	{
	case EBG_WeaponEquipSlot::Primary:
		return Slot == EBG_EquipmentSlot::PrimaryA || Slot == EBG_EquipmentSlot::PrimaryB;
	case EBG_WeaponEquipSlot::Pistol:
		return Slot == EBG_EquipmentSlot::Pistol;
	case EBG_WeaponEquipSlot::Melee:
		return Slot == EBG_EquipmentSlot::Melee;
	default:
		return false;
	}
}

bool UBG_EquipmentComponent::IsArmorSlotCompatible(EBG_EquipmentSlot Slot,
                                                   const FBG_ArmorItemDataRow& ArmorRow) const
{
	switch (ArmorRow.EquipSlot)
	{
	case EBG_ArmorEquipSlot::Helmet:
		return Slot == EBG_EquipmentSlot::Helmet;
	case EBG_ArmorEquipSlot::Vest:
		return Slot == EBG_EquipmentSlot::Vest;
	default:
		return false;
	}
}

UBG_ItemDataRegistrySubsystem* UBG_EquipmentComponent::GetItemDataRegistrySubsystem(
	const TCHAR* OperationName) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: %s failed because World was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		LOGE(TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		LOGE(TEXT("%s: %s failed because UBG_ItemDataRegistrySubsystem was null."),
		     *GetNameSafe(this), OperationName);
		return nullptr;
	}

	return RegistrySubsystem;
}

const FBG_WeaponItemDataRow* UBG_EquipmentComponent::FindWeaponItemRow(
	const FGameplayTag& ItemTag, const TCHAR* OperationName) const
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because weapon ItemTag was invalid."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_WeaponItemDataRow>(EBG_ItemType::Weapon, ItemTag)
		       : nullptr;
}

const FBG_ArmorItemDataRow* UBG_EquipmentComponent::FindArmorItemRow(
	const FGameplayTag& ItemTag, const TCHAR* OperationName) const
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because armor ItemTag was invalid."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_ArmorItemDataRow>(EBG_ItemType::Armor, ItemTag)
		       : nullptr;
}

const FBG_BackpackItemDataRow* UBG_EquipmentComponent::FindBackpackItemRow(
	const FGameplayTag& ItemTag, const TCHAR* OperationName) const
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because backpack ItemTag was invalid."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_BackpackItemDataRow>(EBG_ItemType::Backpack, ItemTag)
		       : nullptr;
}

const FBG_ThrowableItemDataRow* UBG_EquipmentComponent::FindThrowableItemRow(
	const FGameplayTag& ItemTag, const TCHAR* OperationName) const
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because throwable ItemTag was invalid."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_ThrowableItemDataRow>(EBG_ItemType::Throwable, ItemTag)
		       : nullptr;
}

void UBG_EquipmentComponent::SetEquippedItemTag(EBG_EquipmentSlot Slot, const FGameplayTag& ItemTag)
{
	switch (Slot)
	{
	case EBG_EquipmentSlot::PrimaryA:
		PublicState.PrimaryAWeaponTag = ItemTag;
		break;
	case EBG_EquipmentSlot::PrimaryB:
		PublicState.PrimaryBWeaponTag = ItemTag;
		break;
	case EBG_EquipmentSlot::Pistol:
		PublicState.PistolWeaponTag = ItemTag;
		break;
	case EBG_EquipmentSlot::Melee:
		PublicState.MeleeWeaponTag = ItemTag;
		break;
	case EBG_EquipmentSlot::Throwable:
		PublicState.ThrowableItemTag = ItemTag;
		break;
	case EBG_EquipmentSlot::Helmet:
		PublicState.HelmetItemTag = ItemTag;
		break;
	case EBG_EquipmentSlot::Vest:
		PublicState.VestItemTag = ItemTag;
		break;
	case EBG_EquipmentSlot::Backpack:
		PublicState.BackpackItemTag = ItemTag;
		break;
	default:
		break;
	}
}

void UBG_EquipmentComponent::SetLoadedAmmo(EBG_EquipmentSlot WeaponSlot, int32 LoadedAmmo)
{
	const int32 SafeLoadedAmmo = FMath::Max(0, LoadedAmmo);
	switch (WeaponSlot)
	{
	case EBG_EquipmentSlot::PrimaryA:
		OwnerState.PrimaryALoadedAmmo = SafeLoadedAmmo;
		break;
	case EBG_EquipmentSlot::PrimaryB:
		OwnerState.PrimaryBLoadedAmmo = SafeLoadedAmmo;
		break;
	case EBG_EquipmentSlot::Pistol:
		OwnerState.PistolLoadedAmmo = SafeLoadedAmmo;
		break;
	case EBG_EquipmentSlot::Melee:
		OwnerState.MeleeLoadedAmmo = SafeLoadedAmmo;
		break;
	default:
		break;
	}
}

void UBG_EquipmentComponent::SetArmorDurability(EBG_EquipmentSlot ArmorSlot, float Durability)
{
	const float SafeDurability = FMath::Max(0.f, Durability);
	switch (ArmorSlot)
	{
	case EBG_EquipmentSlot::Helmet:
		OwnerState.HelmetDurability = SafeDurability;
		break;
	case EBG_EquipmentSlot::Vest:
		OwnerState.VestDurability = SafeDurability;
		break;
	default:
		break;
	}
}

void UBG_EquipmentComponent::SetEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot, ABG_EquippedWeaponBase* WeaponActor)
{
	switch (WeaponSlot)
	{
	case EBG_EquipmentSlot::PrimaryA:
		EquippedWeaponActors.PrimaryAWeaponActor = WeaponActor;
		break;
	case EBG_EquipmentSlot::PrimaryB:
		EquippedWeaponActors.PrimaryBWeaponActor = WeaponActor;
		break;
	case EBG_EquipmentSlot::Pistol:
		EquippedWeaponActors.PistolWeaponActor = WeaponActor;
		break;
	case EBG_EquipmentSlot::Melee:
		EquippedWeaponActors.MeleeWeaponActor = WeaponActor;
		break;
	default:
		break;
	}
}

void UBG_EquipmentComponent::NormalizeActiveWeaponSlot()
{
	if (PublicState.ActiveWeaponSlot != EBG_EquipmentSlot::None && !GetActiveWeaponItemTag().IsValid())
	{
		PublicState.ActiveWeaponSlot = EBG_EquipmentSlot::None;
	}
}

void UBG_EquipmentComponent::ForceEquipmentNetUpdate() const
{
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

bool UBG_EquipmentComponent::SpawnAndAttachEquippedWeaponActor(
	EBG_EquipmentSlot WeaponSlot,
	const FBG_WeaponItemDataRow& WeaponRow,
	ABG_EquippedWeaponBase*& OutWeaponActor)
{
	OutWeaponActor = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: SpawnAndAttachEquippedWeaponActor failed because World was null."),
		     *GetNameSafe(this));
		return false;
	}

	UClass* EquippedWeaponClass = ResolveEquippedWeaponClass(
		WeaponRow,
		WeaponSlot,
		TEXT("SpawnAndAttachEquippedWeaponActor"));
	if (!EquippedWeaponClass)
		return false;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = CachedCharacter;
	SpawnParameters.Instigator = CachedCharacter;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABG_EquippedWeaponBase* SpawnedWeaponActor = World->SpawnActor<ABG_EquippedWeaponBase>(
		EquippedWeaponClass,
		FTransform::Identity,
		SpawnParameters);
	if (!SpawnedWeaponActor)
	{
		LOGE(TEXT("%s: SpawnAndAttachEquippedWeaponActor failed because SpawnActor returned null for slot %s."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	if (!SpawnedWeaponActor->InitializeEquippedWeapon(CachedCharacter))
	{
		LOGE(TEXT("%s: SpawnAndAttachEquippedWeaponActor failed because %s rejected owner initialization."),
		     *GetNameSafe(this),
		     *GetNameSafe(SpawnedWeaponActor));
		SpawnedWeaponActor->Destroy();
		return false;
	}

	if (!AttachEquippedWeaponActor(WeaponSlot, SpawnedWeaponActor))
	{
		SpawnedWeaponActor->NotifyUnequipped();
		SpawnedWeaponActor->Destroy();
		return false;
	}

	OutWeaponActor = SpawnedWeaponActor;
	return true;
}

UClass* UBG_EquipmentComponent::ResolveEquippedWeaponClass(
	const FBG_WeaponItemDataRow& WeaponRow,
	EBG_EquipmentSlot WeaponSlot,
	const TCHAR* OperationName) const
{
	if (WeaponRow.EquippedWeaponClass.IsNull())
	{
		LOGE(TEXT("%s: %s failed because weapon row %s has no EquippedWeaponClass for slot %s."),
		     *GetNameSafe(this),
		     OperationName,
		     *WeaponRow.ItemTag.ToString(),
		     *GetEquipmentSlotName(WeaponSlot));
		return nullptr;
	}

	UClass* EquippedWeaponClass = WeaponRow.EquippedWeaponClass.LoadSynchronous();
	if (!EquippedWeaponClass)
	{
		LOGE(TEXT("%s: %s failed to load EquippedWeaponClass %s for weapon %s."),
		     *GetNameSafe(this),
		     OperationName,
		     *WeaponRow.EquippedWeaponClass.ToSoftObjectPath().ToString(),
		     *WeaponRow.ItemTag.ToString());
		return nullptr;
	}

	if (!EquippedWeaponClass->IsChildOf(ABG_EquippedWeaponBase::StaticClass()))
	{
		LOGE(TEXT("%s: %s rejected class %s because it is not ABG_EquippedWeaponBase for weapon %s."),
		     *GetNameSafe(this),
		     OperationName,
		     *GetNameSafe(EquippedWeaponClass),
		     *WeaponRow.ItemTag.ToString());
		return nullptr;
	}

	return EquippedWeaponClass;
}

bool UBG_EquipmentComponent::AttachEquippedWeaponActor(
	EBG_EquipmentSlot WeaponSlot,
	ABG_EquippedWeaponBase* WeaponActor) const
{
	if (!IsValid(WeaponActor))
	{
		LOGE(TEXT("%s: AttachEquippedWeaponActor failed because WeaponActor was null for slot %s."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	USceneComponent* AttachParent = GetWeaponAttachParent(TEXT("AttachEquippedWeaponActor"));
	if (!AttachParent)
		return false;

	if (!CachedCharacter)
	{
		LOGE(TEXT("%s: AttachEquippedWeaponActor failed because CachedCharacter was null for slot %s."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FName DesiredSocketName = CachedCharacter->GetDesiredWeaponSocketName(WeaponSlot);
	if (!DesiredSocketName.IsNone() && !AttachParent->DoesSocketExist(DesiredSocketName))
	{
		LOGE(TEXT("%s: AttachEquippedWeaponActor failed because socket %s does not exist on %s for slot %s."),
		     *GetNameSafe(this),
		     *DesiredSocketName.ToString(),
		     *GetNameSafe(AttachParent),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const bool bAttached = WeaponActor->AttachToComponent(
		AttachParent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		DesiredSocketName);
	if (!bAttached)
	{
		LOGE(TEXT("%s: AttachEquippedWeaponActor failed to attach %s to %s socket %s."),
		     *GetNameSafe(this),
		     *GetNameSafe(WeaponActor),
		     *GetNameSafe(AttachParent),
		     *DesiredSocketName.ToString());
		return false;
	}

	const bool bIsActiveSlot = PublicState.ActiveWeaponSlot == WeaponSlot;
	FTransform AttachTransform = bIsActiveSlot
		                             ? WeaponActor->GetHandAttachTransform()
		                             : WeaponActor->GetBackAttachTransform();

	// Reserve weapons stay alive for ammo persistence and are hidden until they become active again.
	WeaponActor->SetActorHiddenInGame(!bIsActiveSlot);

	if (bIsActiveSlot && !WeaponActor->GetGripSocketName().IsNone())
	{
		FTransform GripWorldTransform;
		if (WeaponActor->GetGripTransform(GripWorldTransform))
		{
			FTransform GripActorTransform = GripWorldTransform.GetRelativeTransform(WeaponActor->GetActorTransform());
			GripActorTransform.SetScale3D(FVector::OneVector);
			AttachTransform = GripActorTransform.Inverse() * WeaponActor->GetHandAttachTransform();
		}
		else
		{
			LOGE(TEXT("%s: AttachEquippedWeaponActor fell back to HandAttachTransform because %s could not resolve grip socket %s."),
			     *GetNameSafe(this),
			     *GetNameSafe(WeaponActor),
			     *WeaponActor->GetGripSocketName().ToString());
		}
	}

	WeaponActor->SetActorRelativeTransform(AttachTransform);
	WeaponActor->SetActorEnableCollision(false);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	WeaponActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			LOGE(TEXT("%s: AttachEquippedWeaponActor found a null PrimitiveComponent on %s for slot %s."),
				*GetNameSafe(this),
				*GetNameSafe(WeaponActor),
				*GetEquipmentSlotName(WeaponSlot));
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
	}

	return true;
}

USceneComponent* UBG_EquipmentComponent::GetWeaponAttachParent(const TCHAR* OperationName) const
{
	if (!CachedCharacter)
	{
		LOGE(TEXT("%s: %s failed because CachedCharacter was null."),
		     *GetNameSafe(this),
		     OperationName);
		return nullptr;
	}

	USceneComponent* AttachParent = CachedCharacter->GetBodyAnimationMesh();
	if (!AttachParent)
	{
		LOGE(TEXT("%s: %s failed because owning character %s had no body mesh."),
		     *GetNameSafe(this),
		     OperationName,
		     *GetNameSafe(CachedCharacter));
		return nullptr;
	}

	return AttachParent;
}

void UBG_EquipmentComponent::RefreshEquippedWeaponAttachments()
{
	for (const EBG_EquipmentSlot WeaponSlot : {
		     EBG_EquipmentSlot::PrimaryA,
		     EBG_EquipmentSlot::PrimaryB,
		     EBG_EquipmentSlot::Pistol,
		     EBG_EquipmentSlot::Melee
	     })
	{
		ABG_EquippedWeaponBase* WeaponActor = GetEquippedWeaponActor(WeaponSlot);
		if (!WeaponActor)
		{
			continue;
		}

		if (!AttachEquippedWeaponActor(WeaponSlot, WeaponActor))
		{
			LOGE(TEXT("%s: RefreshEquippedWeaponAttachments failed for slot %s actor %s."),
			     *GetNameSafe(this),
			     *GetEquipmentSlotName(WeaponSlot),
			     *GetNameSafe(WeaponActor));
		}
	}
}

void UBG_EquipmentComponent::DestroyEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot)
{
	ABG_EquippedWeaponBase* WeaponActor = GetEquippedWeaponActor(WeaponSlot);
	SetEquippedWeaponActor(WeaponSlot, nullptr);
	DestroyEquippedWeaponActorInstance(WeaponActor);
}

void UBG_EquipmentComponent::DestroyEquippedWeaponActorInstance(ABG_EquippedWeaponBase* WeaponActor) const
{
	if (!WeaponActor)
	{
		return;
	}

	WeaponActor->NotifyUnequipped();
	WeaponActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	WeaponActor->Destroy();
}

bool UBG_EquipmentComponent::SpawnDroppedEquipmentItem(
	EBG_ItemType DroppedItemType,
	const FGameplayTag& DroppedItemTag,
	int32 DroppedLoadedAmmo) const
{
	ABG_WorldItemBase* SpawnedWorldItem = ABG_WorldItemBase::SpawnDroppedWorldItem(
		GetWorld(),
		BuildDropTransform(),
		WorldItemClass,
		DroppedItemType,
		DroppedItemTag,
		1,
		DroppedLoadedAmmo);

	if (!SpawnedWorldItem)
	{
		LOGE(TEXT("%s: SpawnDroppedEquipmentItem failed for %s %s."),
		     *GetNameSafe(this), *GetItemTypeName(DroppedItemType), *DroppedItemTag.ToString());
		return false;
	}

	return true;
}

FTransform UBG_EquipmentComponent::BuildDropTransform() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: BuildDropTransform failed because owner was null."), *GetNameSafe(this));
		return FTransform::Identity;
	}

	const FVector DropLocation = Owner->GetActorLocation()
		+ Owner->GetActorForwardVector() * 120.f
		+ FVector(0.f, 0.f, 30.f);
	const FRotator DropRotation(0.f, Owner->GetActorRotation().Yaw, 0.f);
	return FTransform(DropRotation, DropLocation);
}

void UBG_EquipmentComponent::NotifyEquipmentFailure(
	EBGInventoryFailReason FailReason,
	EBG_EquipmentSlot EquipmentSlot) const
{
	ABG_Character* OwnerCharacter = Cast<ABG_Character>(GetOwner());
	if (!OwnerCharacter)
	{
		LOGE(TEXT("%s: NotifyEquipmentFailure failed because owner was not ABG_Character. FailReason=%d"),
		     *GetNameSafe(this), static_cast<int32>(FailReason));
		return;
	}

	EBG_ItemType FailureItemType = EBG_ItemType::None;
	if (IsWeaponSlot(EquipmentSlot))
	{
		FailureItemType = EBG_ItemType::Weapon;
	}
	else if (IsArmorSlot(EquipmentSlot))
	{
		FailureItemType = EBG_ItemType::Armor;
	}
	else if (EquipmentSlot == EBG_EquipmentSlot::Backpack)
	{
		FailureItemType = EBG_ItemType::Backpack;
	}
	else if (EquipmentSlot == EBG_EquipmentSlot::Throwable)
	{
		FailureItemType = EBG_ItemType::Throwable;
	}

	OwnerCharacter->Client_ReceiveInventoryFailure(FailReason, FailureItemType, GetEquippedItemTag(EquipmentSlot));
}

void UBG_EquipmentComponent::ApplyActiveWeaponStateToCharacter()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
		return;

	if (!CachedCharacter)
	{
		LOGE(TEXT("%s: ApplyActiveWeaponStateToCharacter failed because CachedCharacter was null."),
		     *GetNameSafe(this));
		return;
	}

	const FGameplayTag ActiveWeaponTag = GetActiveWeaponItemTag();
	if (!ActiveWeaponTag.IsValid())
	{
		CachedCharacter->SetEquippedWeapon(nullptr);
		CachedCharacter->SetWeaponState(EBGWeaponPoseType::None, false);
		return;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(
		ActiveWeaponTag, TEXT("ApplyActiveWeaponStateToCharacter"));
	if (!WeaponRow)
	{
		CachedCharacter->SetEquippedWeapon(nullptr);
		CachedCharacter->SetWeaponState(EBGWeaponPoseType::None, false);
		return;
	}

	ABG_EquippedWeaponBase* ActiveWeaponActor = GetActiveEquippedWeaponActor();
	if (!ActiveWeaponActor)
	{
		LOGE(TEXT("%s: ApplyActiveWeaponStateToCharacter failed because ActiveWeaponActor was null for tag %s."),
			*GetNameSafe(this),
			*ActiveWeaponTag.ToString());
		CachedCharacter->SetEquippedWeapon(nullptr);
		CachedCharacter->SetWeaponState(EBGWeaponPoseType::None, false);
		return;
	}

	const EBGWeaponPoseType WeaponPoseType = ResolveCharacterWeaponPoseType(*WeaponRow);
	CachedCharacter->SetEquippedWeapon(ActiveWeaponActor);
	CachedCharacter->SetWeaponState(WeaponPoseType, WeaponPoseType != EBGWeaponPoseType::None);
}

void UBG_EquipmentComponent::BroadcastEquipmentChanged()
{
	OnEquipmentChanged.Broadcast();
}

void UBG_EquipmentComponent::BroadcastActiveWeaponSlotChanged()
{
	OnActiveWeaponSlotChanged.Broadcast(PublicState.ActiveWeaponSlot);
}

FString UBG_EquipmentComponent::GetEquipmentSlotName(EBG_EquipmentSlot Slot)
{
	const UEnum* SlotEnum = StaticEnum<EBG_EquipmentSlot>();
	return SlotEnum ? SlotEnum->GetNameStringByValue(static_cast<int64>(Slot)) : TEXT("<Unknown>");
}

FString UBG_EquipmentComponent::GetItemTypeName(EBG_ItemType ItemType)
{
	const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
	return ItemTypeEnum ? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(ItemType)) : TEXT("<Unknown>");
}
