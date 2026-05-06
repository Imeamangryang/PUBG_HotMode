// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_WorldItemBase.h"

#include "BG_EquipmentComponent.h"
#include "BG_InventoryComponent.h"
#include "BG_ItemDataRegistrySubsystem.h"
#include "BG_ItemDataRow.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

ABG_WorldItemBase::ABG_WorldItemBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	WorldMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMeshComponent);
	WorldMeshComponent->SetMobility(EComponentMobility::Movable);
	WorldMeshComponent->SetCollisionProfileName(TEXT("Weapon"));
	WorldMeshComponent->SetGenerateOverlapEvents(true);
	WorldMeshComponent->SetSimulatePhysics(true);
}

void ABG_WorldItemBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && !ensureMsgf(Quantity > 0,
	                                  TEXT("%s: BeginPlay found invalid Quantity %d. Destroying world item."),
	                                  *GetNameSafe(this), Quantity))
	{
		Destroy();
		return;
	}

	BroadcastWorldItemStateChanged();
}

void ABG_WorldItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_WorldItemBase, ItemType);
	DOREPLIFETIME(ABG_WorldItemBase, ItemTag);
	DOREPLIFETIME(ABG_WorldItemBase, Quantity);
	DOREPLIFETIME(ABG_WorldItemBase, WeaponLoadedAmmo);
}

ABG_WorldItemBase* ABG_WorldItemBase::SpawnDroppedWorldItem(
	UWorld* World,
	const FTransform& SpawnTransform,
	TSubclassOf<ABG_WorldItemBase> WorldItemClass,
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity,
	int32 LoadedAmmo)
{
	if (!ensureMsgf(World, TEXT("SpawnDroppedWorldItem failed because World was null.")))
		return nullptr;

	if (!ensureMsgf(ItemType != EBG_ItemType::None && ItemTag.IsValid(),
	                TEXT("SpawnDroppedWorldItem failed because item identity was invalid. ItemType=%s, ItemTag=%s."),
	                *GetItemTypeName(ItemType), *ItemTag.ToString()))
		return nullptr;

	if (!ensureMsgf(Quantity > 0, TEXT("SpawnDroppedWorldItem failed because Quantity %d was not positive."), Quantity))
		return nullptr;

	UClass* SpawnClass = ResolveWorldItemClass(World, WorldItemClass, ItemType, ItemTag);
	if (!ensureMsgf(SpawnClass, TEXT("SpawnDroppedWorldItem failed because SpawnClass was null.")))
		return nullptr;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ABG_WorldItemBase* WorldItem = World->SpawnActor<ABG_WorldItemBase>(
		SpawnClass,
		SpawnTransform,
		SpawnParameters);
	if (!ensureMsgf(WorldItem, TEXT("SpawnDroppedWorldItem failed because SpawnActor returned null for %s %s."),
	                *GetItemTypeName(ItemType), *ItemTag.ToString()))
		return nullptr;

	WorldItem->InitializeWorldItem(ItemType, ItemTag, Quantity, LoadedAmmo);
	return WorldItem;
}

UClass* ABG_WorldItemBase::ResolveWorldItemClass(
	UWorld* World,
	TSubclassOf<ABG_WorldItemBase> FallbackWorldItemClass,
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag)
{
	UClass* FallbackClass = FallbackWorldItemClass ? FallbackWorldItemClass.Get() : ABG_WorldItemBase::StaticClass();
	if (!ensureMsgf(FallbackClass, TEXT("ResolveWorldItemClass failed because fallback class was null for %s %s."),
	                *GetItemTypeName(ItemType), *ItemTag.ToString()))
		return nullptr;

	if (!ensureMsgf(FallbackClass->IsChildOf(ABG_WorldItemBase::StaticClass()),
	                TEXT("ResolveWorldItemClass found invalid fallback class %s for %s %s. Using ABG_WorldItemBase."),
	                *GetNameSafe(FallbackClass), *GetItemTypeName(ItemType), *ItemTag.ToString()))
		FallbackClass = ABG_WorldItemBase::StaticClass();

	const FBG_ItemDataRow* ItemRow = FindWorldItemRow(World, ItemType, ItemTag, TEXT("ResolveWorldItemClass"));
	if (!ItemRow || ItemRow->WorldItemClass.IsNull())
		return FallbackClass;

	UClass* RowWorldItemClass = ItemRow->WorldItemClass.LoadSynchronous();
	if (!ensureMsgf(RowWorldItemClass,
	                TEXT("ResolveWorldItemClass failed to load row world item class %s for %s %s. Using fallback %s."),
	                *ItemRow->WorldItemClass.ToSoftObjectPath().ToString(),
	                *GetItemTypeName(ItemType), *ItemTag.ToString(), *GetNameSafe(FallbackClass)))
		return FallbackClass;

	if (!ensureMsgf(RowWorldItemClass->IsChildOf(ABG_WorldItemBase::StaticClass()),
	                TEXT(
		                "ResolveWorldItemClass rejected %s because it is not a child of ABG_WorldItemBase. Using fallback %s."
	                ),
	                *GetNameSafe(RowWorldItemClass), *GetNameSafe(FallbackClass)))
		return FallbackClass;

	return RowWorldItemClass;
}

const FBG_ItemDataRow* ABG_WorldItemBase::FindWorldItemRow(
	UWorld* World,
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	const TCHAR* OperationName)
{
	if (!ensureMsgf(World, TEXT("%s failed because World was null."), OperationName))
		return nullptr;

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!ensureMsgf(GameInstance, TEXT("%s failed because GameInstance was null."), OperationName))
		return nullptr;

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!ensureMsgf(RegistrySubsystem, TEXT("%s failed because UBG_ItemDataRegistrySubsystem was null."),
	                OperationName))
		return nullptr;

	return RegistrySubsystem->FindItemRow(ItemType, ItemTag);
}

void ABG_WorldItemBase::InitializeWorldItem(
	EBG_ItemType NewItemType,
	FGameplayTag NewItemTag,
	int32 NewQuantity,
	int32 NewWeaponLoadedAmmo)
{
	if (!ensureMsgf(HasAuthority(), TEXT("%s: InitializeWorldItem failed because actor had no authority."),
	                *GetNameSafe(this)))
		return;

	if (!ensureMsgf(NewItemType != EBG_ItemType::None && NewItemTag.IsValid(),
	                TEXT("%s: InitializeWorldItem failed because item identity was invalid. ItemType=%s, ItemTag=%s."),
	                *GetNameSafe(this), *GetItemTypeName(NewItemType), *NewItemTag.ToString()))
		return;

	if (!ensureMsgf(NewQuantity > 0, TEXT("%s: InitializeWorldItem failed because Quantity %d was not positive."),
	                *GetNameSafe(this), NewQuantity))
		return;

	ItemType = NewItemType;
	ItemTag = NewItemTag;
	Quantity = NewQuantity;
	WeaponLoadedAmmo = FMath::Max(0, NewWeaponLoadedAmmo);

	BroadcastWorldItemStateChanged();
	ForceNetUpdate();
}

bool ABG_WorldItemBase::TryPickup(
	ABG_Character* RequestingCharacter,
	int32 RequestedQuantity,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	int32 PickupQuantity = 0;
	if (!ValidatePickupRequest(RequestingCharacter, RequestedQuantity, PickupQuantity, OutFailReason))
		return false;

	switch (ItemType)
	{
	case EBG_ItemType::Ammo:
	case EBG_ItemType::Heal:
	case EBG_ItemType::Boost:
	case EBG_ItemType::Throwable:
		return TryPickupStackItem(RequestingCharacter, PickupQuantity, OutFailReason);
	case EBG_ItemType::Weapon:
		return TryPickupWeapon(RequestingCharacter, PickupQuantity, OutFailReason);
	case EBG_ItemType::Armor:
		return TryPickupArmor(RequestingCharacter, PickupQuantity, OutFailReason);
	case EBG_ItemType::Backpack:
		return TryPickupBackpack(RequestingCharacter, PickupQuantity, OutFailReason);
	default:
		ensureMsgf(false, TEXT("%s: TryPickup failed because item type %s is unsupported."),
		           *GetNameSafe(this), *GetItemTypeName(ItemType));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}
}

void ABG_WorldItemBase::OnRep_ItemState()
{
	BroadcastWorldItemStateChanged();
}

bool ABG_WorldItemBase::ValidatePickupRequest(
	ABG_Character* RequestingCharacter,
	int32 RequestedQuantity,
	int32& OutPickupQuantity,
	EBGInventoryFailReason& OutFailReason) const
{
	OutPickupQuantity = 0;

	if (!ensureMsgf(HasAuthority(), TEXT("%s: ValidatePickupRequest failed because actor had no authority."),
	                *GetNameSafe(this)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (!ensureMsgf(IsValid(RequestingCharacter),
	                TEXT("%s: ValidatePickupRequest failed because RequestingCharacter was invalid."),
	                *GetNameSafe(this)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (!ensureMsgf(RequestingCharacter->GetController(),
	                TEXT("%s: ValidatePickupRequest failed because %s had no controller."),
	                *GetNameSafe(this), *GetNameSafe(RequestingCharacter)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (!ensureMsgf(RequestedQuantity > 0,
	                TEXT("%s: ValidatePickupRequest failed because RequestedQuantity %d was not positive."),
	                *GetNameSafe(this), RequestedQuantity))
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (!ensureMsgf(Quantity > 0, TEXT("%s: ValidatePickupRequest failed because world item quantity %d was invalid."),
	                *GetNameSafe(this), Quantity))
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (!ensureMsgf(ItemType != EBG_ItemType::None && ItemTag.IsValid(),
	                TEXT("%s: ValidatePickupRequest failed because item identity was invalid. ItemType=%s, ItemTag=%s."
	                ),
	                *GetNameSafe(this), *GetItemTypeName(ItemType), *ItemTag.ToString()))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!FindItemRow(TEXT("ValidatePickupRequest")))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!IsInPickupRange(RequestingCharacter))
	{
		LOGW(TEXT("%s: ValidatePickupRequest rejected pickup because %s was too far."),
		     *GetNameSafe(this),
		     *GetNameSafe(RequestingCharacter));
		OutFailReason = EBGInventoryFailReason::TooFar;
		return false;
	}

	OutPickupQuantity = FMath::Min(RequestedQuantity, Quantity);
	return true;
}

bool ABG_WorldItemBase::TryPickupStackItem(
	ABG_Character* RequestingCharacter,
	int32 PickupQuantity,
	EBGInventoryFailReason& OutFailReason)
{
	if (!IsStackInventoryItemType())
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	UBG_InventoryComponent* InventoryComponent = RequestingCharacter->GetInventoryComponent();
	if (!ensureMsgf(InventoryComponent, TEXT("%s: TryPickupStackItem failed because %s had no InventoryComponent."),
	                *GetNameSafe(this), *GetNameSafe(RequestingCharacter)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	int32 AddedQuantity = 0;
	if (!InventoryComponent->TryAddItem(ItemType, ItemTag, PickupQuantity, AddedQuantity) || AddedQuantity <= 0)
	{
		OutFailReason = EBGInventoryFailReason::Overweight;
		return false;
	}

	ConsumePickedUpQuantity(AddedQuantity);
	OutFailReason = EBGInventoryFailReason::None;
	RequestingCharacter->NotifySuccessfulPickup(ItemType);
	return true;
}

bool ABG_WorldItemBase::TryPickupWeapon(
	ABG_Character* RequestingCharacter,
	int32 PickupQuantity,
	EBGInventoryFailReason& OutFailReason)
{
	if (PickupQuantity != 1)
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	UBG_EquipmentComponent* EquipmentComponent = RequestingCharacter->GetEquipmentComponent();
	if (!ensureMsgf(EquipmentComponent, TEXT("%s: TryPickupWeapon failed because %s had no EquipmentComponent."),
	                *GetNameSafe(this), *GetNameSafe(RequestingCharacter)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(TEXT("TryPickupWeapon"));
	if (!WeaponRow)
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	const EBG_EquipmentSlot WeaponSlot = SelectWeaponSlotForPickup(*EquipmentComponent, *WeaponRow);
	if (WeaponSlot == EBG_EquipmentSlot::None)
	{
		OutFailReason = EBGInventoryFailReason::SlotMismatch;
		return false;
	}

	FGameplayTag ReplacedWeaponItemTag;
	int32 ReplacedLoadedAmmo = 0;
	if (!EquipmentComponent->TryEquipWeapon(WeaponSlot, ItemTag, WeaponLoadedAmmo, ReplacedWeaponItemTag,
	                                        ReplacedLoadedAmmo))
	{
		OutFailReason = EBGInventoryFailReason::SlotMismatch;
		return false;
	}

	if (ReplacedWeaponItemTag.IsValid())
	{
		if (!TrySpawnReplacementItem(EBG_ItemType::Weapon, ReplacedWeaponItemTag, 1, ReplacedLoadedAmmo))
		{
			FGameplayTag RollbackRemovedWeaponItemTag;
			int32 RollbackRemovedLoadedAmmo = 0;
			EquipmentComponent->TryUnequipWeapon(WeaponSlot, RollbackRemovedWeaponItemTag, RollbackRemovedLoadedAmmo);

			FGameplayTag RollbackReplacedWeaponItemTag;
			int32 RollbackReplacedLoadedAmmo = 0;
			EquipmentComponent->TryEquipWeapon(WeaponSlot, ReplacedWeaponItemTag, ReplacedLoadedAmmo,
			                                   RollbackReplacedWeaponItemTag, RollbackReplacedLoadedAmmo);

			OutFailReason = EBGInventoryFailReason::ServerRejected;
			return false;
		}
	}

	ConsumePickedUpQuantity(1);
	OutFailReason = EBGInventoryFailReason::None;
	RequestingCharacter->NotifySuccessfulPickup(ItemType);
	return true;
}

bool ABG_WorldItemBase::TryPickupArmor(
	ABG_Character* RequestingCharacter,
	int32 PickupQuantity,
	EBGInventoryFailReason& OutFailReason)
{
	if (PickupQuantity != 1)
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	UBG_EquipmentComponent* EquipmentComponent = RequestingCharacter->GetEquipmentComponent();
	if (!ensureMsgf(EquipmentComponent, TEXT("%s: TryPickupArmor failed because %s had no EquipmentComponent."),
	                *GetNameSafe(this), *GetNameSafe(RequestingCharacter)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	const FBG_ArmorItemDataRow* ArmorRow = FindArmorItemRow(TEXT("TryPickupArmor"));
	if (!ArmorRow)
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	const EBG_EquipmentSlot ArmorSlot = SelectArmorSlotForPickup(*ArmorRow);
	if (ArmorSlot == EBG_EquipmentSlot::None)
	{
		OutFailReason = EBGInventoryFailReason::SlotMismatch;
		return false;
	}

	FGameplayTag ReplacedArmorItemTag;
	float ReplacedArmorDurability = 0.f;
	if (!EquipmentComponent->TryEquipArmor(ArmorSlot, ItemTag, ReplacedArmorItemTag, ReplacedArmorDurability))
	{
		OutFailReason = EBGInventoryFailReason::SlotMismatch;
		return false;
	}

	if (ReplacedArmorItemTag.IsValid())
	{
		TrySpawnReplacementItem(EBG_ItemType::Armor, ReplacedArmorItemTag, 1, 0);
	}

	ConsumePickedUpQuantity(1);
	OutFailReason = EBGInventoryFailReason::None;
	RequestingCharacter->NotifySuccessfulPickup(ItemType);
	return true;
}

bool ABG_WorldItemBase::TryPickupBackpack(
	ABG_Character* RequestingCharacter,
	int32 PickupQuantity,
	EBGInventoryFailReason& OutFailReason)
{
	if (PickupQuantity != 1)
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	UBG_EquipmentComponent* EquipmentComponent = RequestingCharacter->GetEquipmentComponent();
	if (!ensureMsgf(EquipmentComponent, TEXT("%s: TryPickupBackpack failed because %s had no EquipmentComponent."),
	                *GetNameSafe(this), *GetNameSafe(RequestingCharacter)))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (!FindBackpackItemRow(TEXT("TryPickupBackpack")))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	FGameplayTag ReplacedBackpackItemTag;
	if (!EquipmentComponent->TryEquipBackpack(ItemTag, ReplacedBackpackItemTag))
	{
		OutFailReason = EBGInventoryFailReason::Overweight;
		return false;
	}

	if (ReplacedBackpackItemTag.IsValid())
	{
		TrySpawnReplacementItem(EBG_ItemType::Backpack, ReplacedBackpackItemTag, 1, 0);
	}

	ConsumePickedUpQuantity(1);
	OutFailReason = EBGInventoryFailReason::None;
	RequestingCharacter->NotifySuccessfulPickup(ItemType);
	return true;
}

bool ABG_WorldItemBase::TrySpawnReplacementItem(
	EBG_ItemType ReplacedItemType,
	const FGameplayTag& ReplacedItemTag,
	int32 ReplacedQuantity,
	int32 ReplacedLoadedAmmo) const
{
	const FVector SpawnLocation = GetActorLocation() + GetActorRightVector() * 40.f;
	const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
	ABG_WorldItemBase* SpawnedItem = SpawnDroppedWorldItem(
		GetWorld(),
		SpawnTransform,
		GetClass(),
		ReplacedItemType,
		ReplacedItemTag,
		ReplacedQuantity,
		ReplacedLoadedAmmo);

	if (!ensureMsgf(SpawnedItem, TEXT("%s: TrySpawnReplacementItem failed for replaced item %s %s."),
	                *GetNameSafe(this), *GetItemTypeName(ReplacedItemType), *ReplacedItemTag.ToString()))
		return false;

	return true;
}

void ABG_WorldItemBase::ConsumePickedUpQuantity(int32 PickedUpQuantity)
{
	if (!ensureMsgf(HasAuthority(), TEXT("%s: ConsumePickedUpQuantity failed because actor had no authority."),
	                *GetNameSafe(this)))
		return;

	if (!ensureMsgf(PickedUpQuantity > 0,
	                TEXT("%s: ConsumePickedUpQuantity failed because PickedUpQuantity %d was not positive."),
	                *GetNameSafe(this), PickedUpQuantity))
		return;

	Quantity = FMath::Max(0, Quantity - PickedUpQuantity);
	if (Quantity <= 0)
	{
		Destroy();
		return;
	}

	BroadcastWorldItemStateChanged();
	ForceNetUpdate();
}

bool ABG_WorldItemBase::IsStackInventoryItemType() const
{
	return ItemType == EBG_ItemType::Ammo
		|| ItemType == EBG_ItemType::Heal
		|| ItemType == EBG_ItemType::Boost
		|| ItemType == EBG_ItemType::Throwable;
}

bool ABG_WorldItemBase::IsInPickupRange(const ABG_Character* RequestingCharacter) const
{
	if (!IsValid(RequestingCharacter))
		return false;

	const float Range = FMath::Max(0.f, PickupInteractionRange);
	return FVector::DistSquared(GetActorLocation(), RequestingCharacter->GetActorLocation()) <= FMath::Square(Range);
}

UBG_ItemDataRegistrySubsystem* ABG_WorldItemBase::GetItemDataRegistrySubsystem(
	const TCHAR* OperationName) const
{
	const UWorld* World = GetWorld();
	if (!ensureMsgf(World, TEXT("%s: %s failed because World was null."), *GetNameSafe(this), OperationName))
		return nullptr;

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!ensureMsgf(GameInstance, TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(this),
	                OperationName))
		return nullptr;

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!ensureMsgf(RegistrySubsystem, TEXT("%s: %s failed because UBG_ItemDataRegistrySubsystem was null."),
	                *GetNameSafe(this), OperationName))
		return nullptr;

	return RegistrySubsystem;
}

const FBG_ItemDataRow* ABG_WorldItemBase::FindItemRow(const TCHAR* OperationName) const
{
	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem ? RegistrySubsystem->FindItemRow(ItemType, ItemTag) : nullptr;
}

const FBG_WeaponItemDataRow* ABG_WorldItemBase::FindWeaponItemRow(const TCHAR* OperationName) const
{
	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_WeaponItemDataRow>(EBG_ItemType::Weapon, ItemTag)
		       : nullptr;
}

const FBG_ArmorItemDataRow* ABG_WorldItemBase::FindArmorItemRow(const TCHAR* OperationName) const
{
	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_ArmorItemDataRow>(EBG_ItemType::Armor, ItemTag)
		       : nullptr;
}

const FBG_BackpackItemDataRow* ABG_WorldItemBase::FindBackpackItemRow(const TCHAR* OperationName) const
{
	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		       ? RegistrySubsystem->FindTypedItemRow<FBG_BackpackItemDataRow>(EBG_ItemType::Backpack, ItemTag)
		       : nullptr;
}

EBG_EquipmentSlot ABG_WorldItemBase::SelectWeaponSlotForPickup(
	const UBG_EquipmentComponent& EquipmentComponent,
	const FBG_WeaponItemDataRow& WeaponRow)
{
	switch (WeaponRow.EquipSlot)
	{
	case EBG_WeaponEquipSlot::Primary:
		if (!EquipmentComponent.HasEquippedItem(EBG_EquipmentSlot::PrimaryA))
		{
			return EBG_EquipmentSlot::PrimaryA;
		}
		if (!EquipmentComponent.HasEquippedItem(EBG_EquipmentSlot::PrimaryB))
		{
			return EBG_EquipmentSlot::PrimaryB;
		}
		if (EquipmentComponent.GetActiveWeaponSlot() == EBG_EquipmentSlot::PrimaryA
			|| EquipmentComponent.GetActiveWeaponSlot() == EBG_EquipmentSlot::PrimaryB)
		{
			return EquipmentComponent.GetActiveWeaponSlot();
		}
		return EBG_EquipmentSlot::PrimaryA;
	case EBG_WeaponEquipSlot::Pistol:
		return EBG_EquipmentSlot::Pistol;
	case EBG_WeaponEquipSlot::Melee:
		return EBG_EquipmentSlot::Melee;
	default:
		return EBG_EquipmentSlot::None;
	}
}

EBG_EquipmentSlot ABG_WorldItemBase::SelectArmorSlotForPickup(const FBG_ArmorItemDataRow& ArmorRow)
{
	switch (ArmorRow.EquipSlot)
	{
	case EBG_ArmorEquipSlot::Helmet:
		return EBG_EquipmentSlot::Helmet;
	case EBG_ArmorEquipSlot::Vest:
		return EBG_EquipmentSlot::Vest;
	default:
		return EBG_EquipmentSlot::None;
	}
}

void ABG_WorldItemBase::BroadcastWorldItemStateChanged()
{
	ReceiveWorldItemStateChanged();
	OnWorldItemStateChanged.Broadcast();
}

FString ABG_WorldItemBase::GetItemTypeName(EBG_ItemType InItemType)
{
	const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
	return ItemTypeEnum
		       ? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(InItemType))
		       : TEXT("<Unknown>");
}
