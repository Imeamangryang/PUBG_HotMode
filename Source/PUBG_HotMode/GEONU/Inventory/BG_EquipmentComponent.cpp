// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_EquipmentComponent.h"

#include "BG_InventoryComponent.h"
#include "BG_ItemDataRegistrySubsystem.h"
#include "BG_ItemDataRow.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

UBG_EquipmentComponent::UBG_EquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
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
		LOGE(TEXT("FindEquipmentComponent failed because %s has no equipment component."), *GetNameSafe(TargetActor));
		return nullptr;
	}

	return EquipmentComponent;
}

bool UBG_EquipmentComponent::TryEquipWeapon(EBG_EquipmentSlot WeaponSlot, FGameplayTag WeaponItemTag,
                                            int32 LoadedAmmo, FGameplayTag& OutReplacedWeaponItemTag,
                                            int32& OutReplacedLoadedAmmo)
{
	OutReplacedWeaponItemTag = FGameplayTag();
	OutReplacedLoadedAmmo = 0;

	if (!CanMutateEquipmentState(TEXT("TryEquipWeapon")))
	{
		return false;
	}

	if (!IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: TryEquipWeapon failed because %s is not a weapon slot."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(WeaponItemTag, TEXT("TryEquipWeapon"));
	if (!WeaponRow)
	{
		return false;
	}

	if (!IsWeaponSlotCompatible(WeaponSlot, *WeaponRow))
	{
		LOGE(TEXT("%s: TryEquipWeapon failed because weapon %s is not compatible with slot %s."),
		     *GetNameSafe(this),
		     *WeaponItemTag.ToString(),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	OutReplacedWeaponItemTag = GetEquippedItemTag(WeaponSlot);
	OutReplacedLoadedAmmo = GetLoadedAmmo(WeaponSlot);

	SetEquippedItemTag(WeaponSlot, WeaponItemTag);
	SetLoadedAmmo(WeaponSlot, FMath::Clamp(LoadedAmmo, 0, FMath::Max(0, WeaponRow->MagazineSize)));

	if (PublicState.ActiveWeaponSlot == EBG_EquipmentSlot::None || !GetActiveWeaponItemTag().IsValid())
	{
		PublicState.ActiveWeaponSlot = WeaponSlot;
	}

	NormalizeActiveWeaponSlot();
	ApplyActiveWeaponStateToCharacter();
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::TryUnequipWeapon(EBG_EquipmentSlot WeaponSlot,
                                              FGameplayTag& OutRemovedWeaponItemTag,
                                              int32& OutRemovedLoadedAmmo)
{
	OutRemovedWeaponItemTag = FGameplayTag();
	OutRemovedLoadedAmmo = 0;

	if (!CanMutateEquipmentState(TEXT("TryUnequipWeapon")))
	{
		return false;
	}

	if (!IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: TryUnequipWeapon failed because %s is not a weapon slot."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	if (!HasEquippedItem(WeaponSlot))
	{
		LOGE(TEXT("%s: TryUnequipWeapon failed because slot %s is empty."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	OutRemovedWeaponItemTag = GetEquippedItemTag(WeaponSlot);
	OutRemovedLoadedAmmo = GetLoadedAmmo(WeaponSlot);

	SetEquippedItemTag(WeaponSlot, FGameplayTag());
	SetLoadedAmmo(WeaponSlot, 0);

	if (PublicState.ActiveWeaponSlot == WeaponSlot)
	{
		PublicState.ActiveWeaponSlot = EBG_EquipmentSlot::None;
	}

	NormalizeActiveWeaponSlot();
	ApplyActiveWeaponStateToCharacter();
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::TryActivateWeapon(EBG_EquipmentSlot WeaponSlot)
{
	if (!CanMutateEquipmentState(TEXT("TryActivateWeapon")))
	{
		return false;
	}

	if (WeaponSlot != EBG_EquipmentSlot::None && !IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: TryActivateWeapon failed because %s is not a weapon slot."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	if (WeaponSlot != EBG_EquipmentSlot::None && !HasEquippedItem(WeaponSlot))
	{
		LOGE(TEXT("%s: TryActivateWeapon failed because slot %s is empty."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	PublicState.ActiveWeaponSlot = WeaponSlot;
	NormalizeActiveWeaponSlot();
	ApplyActiveWeaponStateToCharacter();
	BroadcastActiveWeaponSlotChanged();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::TryLoadAmmo(EBG_EquipmentSlot WeaponSlot, int32 LoadedAmmo)
{
	if (!CanMutateEquipmentState(TEXT("TryLoadAmmo")))
	{
		return false;
	}

	if (!IsWeaponSlot(WeaponSlot))
	{
		LOGE(TEXT("%s: TryLoadAmmo failed because %s is not a weapon slot."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FGameplayTag WeaponItemTag = GetEquippedItemTag(WeaponSlot);
	if (!WeaponItemTag.IsValid())
	{
		LOGE(TEXT("%s: TryLoadAmmo failed because slot %s is empty."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(WeaponSlot));
		return false;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(WeaponItemTag, TEXT("TryLoadAmmo"));
	if (!WeaponRow)
	{
		return false;
	}

	SetLoadedAmmo(WeaponSlot, FMath::Clamp(LoadedAmmo, 0, FMath::Max(0, WeaponRow->MagazineSize)));
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::TryEquipArmor(EBG_EquipmentSlot ArmorSlot, FGameplayTag ArmorItemTag,
                                           FGameplayTag& OutReplacedArmorItemTag,
                                           float& OutReplacedArmorDurability)
{
	OutReplacedArmorItemTag = FGameplayTag();
	OutReplacedArmorDurability = 0.f;

	if (!CanMutateEquipmentState(TEXT("TryEquipArmor")))
	{
		return false;
	}

	if (!IsArmorSlot(ArmorSlot))
	{
		LOGE(TEXT("%s: TryEquipArmor failed because %s is not an armor slot."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(ArmorSlot));
		return false;
	}

	const FBG_ArmorItemDataRow* ArmorRow = FindArmorItemRow(ArmorItemTag, TEXT("TryEquipArmor"));
	if (!ArmorRow)
	{
		return false;
	}

	if (!IsArmorSlotCompatible(ArmorSlot, *ArmorRow))
	{
		LOGE(TEXT("%s: TryEquipArmor failed because armor %s is not compatible with slot %s."),
		     *GetNameSafe(this),
		     *ArmorItemTag.ToString(),
		     *GetEquipmentSlotName(ArmorSlot));
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

bool UBG_EquipmentComponent::TryUnequipArmor(EBG_EquipmentSlot ArmorSlot,
                                             FGameplayTag& OutRemovedArmorItemTag,
                                             float& OutRemovedArmorDurability)
{
	OutRemovedArmorItemTag = FGameplayTag();
	OutRemovedArmorDurability = 0.f;

	if (!CanMutateEquipmentState(TEXT("TryUnequipArmor")))
	{
		return false;
	}

	if (!IsArmorSlot(ArmorSlot))
	{
		LOGE(TEXT("%s: TryUnequipArmor failed because %s is not an armor slot."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(ArmorSlot));
		return false;
	}

	if (!HasEquippedItem(ArmorSlot))
	{
		LOGE(TEXT("%s: TryUnequipArmor failed because slot %s is empty."),
		     *GetNameSafe(this),
		     *GetEquipmentSlotName(ArmorSlot));
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

bool UBG_EquipmentComponent::TryEquipBackpack(FGameplayTag BackpackItemTag,
                                              FGameplayTag& OutReplacedBackpackItemTag)
{
	OutReplacedBackpackItemTag = FGameplayTag();

	if (!CanMutateEquipmentState(TEXT("TryEquipBackpack")))
	{
		return false;
	}

	const FBG_BackpackItemDataRow* BackpackRow = FindBackpackItemRow(BackpackItemTag, TEXT("TryEquipBackpack"));
	if (!BackpackRow)
	{
		return false;
	}

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("TryEquipBackpack"));
	if (!InventoryComponent)
	{
		return false;
	}

	if (!InventoryComponent->SetBackpackWeightBonus(BackpackRow->WeightBonus))
	{
		LOGW(TEXT("%s: TryEquipBackpack rejected backpack %s because inventory weight limit update failed."),
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

bool UBG_EquipmentComponent::TryUnequipBackpack(FGameplayTag& OutRemovedBackpackItemTag)
{
	OutRemovedBackpackItemTag = FGameplayTag();

	if (!CanMutateEquipmentState(TEXT("TryUnequipBackpack")))
	{
		return false;
	}

	if (!PublicState.BackpackItemTag.IsValid())
	{
		LOGE(TEXT("%s: TryUnequipBackpack failed because backpack slot is empty."), *GetNameSafe(this));
		return false;
	}

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("TryUnequipBackpack"));
	if (!InventoryComponent)
	{
		return false;
	}

	if (!InventoryComponent->SetBackpackWeightBonus(0.f))
	{
		LOGW(TEXT("%s: TryUnequipBackpack rejected because current inventory weight exceeds base capacity."),
		     *GetNameSafe(this));
		return false;
	}

	OutRemovedBackpackItemTag = PublicState.BackpackItemTag;
	PublicState.BackpackItemTag = FGameplayTag();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::TrySetThrowable(FGameplayTag ThrowableItemTag)
{
	if (!CanMutateEquipmentState(TEXT("TrySetThrowable")))
	{
		return false;
	}

	if (!FindThrowableItemRow(ThrowableItemTag, TEXT("TrySetThrowable")))
	{
		return false;
	}

	UBG_InventoryComponent* InventoryComponent = GetInventoryComponentForEquipment(TEXT("TrySetThrowable"));
	if (!InventoryComponent)
	{
		return false;
	}

	const int32 ThrowableQuantity = InventoryComponent->GetQuantity(EBG_ItemType::Throwable, ThrowableItemTag);
	if (ThrowableQuantity <= 0)
	{
		LOGE(TEXT("%s: TrySetThrowable failed because inventory has no throwable %s."),
		     *GetNameSafe(this),
		     *ThrowableItemTag.ToString());
		return false;
	}

	PublicState.ThrowableItemTag = ThrowableItemTag;
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
}

bool UBG_EquipmentComponent::TryClearThrowable()
{
	if (!CanMutateEquipmentState(TEXT("TryClearThrowable")))
	{
		return false;
	}

	if (!PublicState.ThrowableItemTag.IsValid())
	{
		return true;
	}

	PublicState.ThrowableItemTag = FGameplayTag();
	BroadcastEquipmentChanged();
	ForceEquipmentNetUpdate();
	return true;
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
	BroadcastEquipmentChanged();
	BroadcastActiveWeaponSlotChanged();
}

void UBG_EquipmentComponent::OnRep_OwnerState()
{
	BroadcastEquipmentChanged();
}

void UBG_EquipmentComponent::HandleInventoryItemQuantityChanged(
	EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (ItemType != EBG_ItemType::Throwable || Quantity > 0 || PublicState.ThrowableItemTag != ItemTag)
	{
		return;
	}

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
		     *GetNameSafe(this),
		     *GetNameSafe(Owner));
	}

	CachedInventory = Owner->FindComponentByClass<UBG_InventoryComponent>();
	if (!CachedInventory)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s had no inventory component."),
		     *GetNameSafe(this),
		     *GetNameSafe(Owner));
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
		     *GetNameSafe(this),
		     OperationName,
		     *GetNameSafe(Owner));
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
		     *GetNameSafe(this),
		     OperationName,
		     *GetNameSafe(Owner));
		return false;
	}

	return true;
}

bool UBG_EquipmentComponent::IsWeaponSlot(EBG_EquipmentSlot Slot) const
{
	return Slot == EBG_EquipmentSlot::PrimaryA
		|| Slot == EBG_EquipmentSlot::PrimaryB
		|| Slot == EBG_EquipmentSlot::Pistol
		|| Slot == EBG_EquipmentSlot::Melee;
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
		     *GetNameSafe(this),
		     OperationName);
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

void UBG_EquipmentComponent::ApplyActiveWeaponStateToCharacter()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!CachedCharacter)
	{
		LOGE(TEXT("%s: ApplyActiveWeaponStateToCharacter failed because CachedCharacter was null."),
		     *GetNameSafe(this));
		return;
	}

	const FGameplayTag ActiveWeaponTag = GetActiveWeaponItemTag();
	if (!ActiveWeaponTag.IsValid())
	{
		CachedCharacter->SetWeaponState(EBGWeaponPoseType::None, false);
		return;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(
		ActiveWeaponTag, TEXT("ApplyActiveWeaponStateToCharacter"));
	if (!WeaponRow)
	{
		CachedCharacter->SetWeaponState(EBGWeaponPoseType::None, false);
		return;
	}

	EBGWeaponPoseType WeaponPoseType = EBGWeaponPoseType::None;
	switch (WeaponRow->WeaponPoseCategory)
	{
	case EBG_WeaponPoseCategory::Pistol:
		WeaponPoseType = EBGWeaponPoseType::Pistol;
		break;
	case EBG_WeaponPoseCategory::Rifle:
		WeaponPoseType = EBGWeaponPoseType::Rifle;
		break;
	case EBG_WeaponPoseCategory::Shotgun:
		WeaponPoseType = EBGWeaponPoseType::Shotgun;
		break;
	default:
		WeaponPoseType = EBGWeaponPoseType::None;
		break;
	}

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
