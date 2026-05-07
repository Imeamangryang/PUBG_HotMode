// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BG_InventoryViewModel.h"

#include "Engine/GameInstance.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemUseComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemDataRow.h"
#include "Inventory/BG_WorldItemBase.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

namespace
{
	constexpr EBG_EquipmentSlot EquipmentSlotRenderOrder[] =
	{
		EBG_EquipmentSlot::PrimaryA,
		EBG_EquipmentSlot::PrimaryB,
		EBG_EquipmentSlot::Pistol,
		EBG_EquipmentSlot::Melee,
		EBG_EquipmentSlot::Throwable,
		EBG_EquipmentSlot::Helmet,
		EBG_EquipmentSlot::Vest,
		EBG_EquipmentSlot::Backpack
	};
}

// --- Lifecycle ------------

UBG_InventoryViewModel::UBG_InventoryViewModel()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBG_InventoryViewModel::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by PlayerController."), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController())
		return;

	NotifyPossessedCharacterReady(Cast<ABG_Character>(PC->GetPawn()));
}

void UBG_InventoryViewModel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromCharacter();
	Super::EndPlay(EndPlayReason);
}

// --- Possession Binding ------------

void UBG_InventoryViewModel::NotifyPossessedCharacterReady(ABG_Character* InCharacter)
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to bind possessed character."), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController())
		return;

	if (!InCharacter)
	{
		LOGE(TEXT("%s received a null character in NotifyPossessedCharacterReady."),
			*GetNameSafe(this));
		UnbindFromCharacter();
		return;

	}

	UBG_InventoryComponent* InventoryComponent = InCharacter->GetInventoryComponent();
	if (!InventoryComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no InventoryComponent."),
			*GetNameSafe(this), *GetNameSafe(InCharacter));
		UnbindFromCharacter();
		return;

	}

	UBG_EquipmentComponent* EquipmentComponent = InCharacter->GetEquipmentComponent();
	if (!EquipmentComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no EquipmentComponent."),
			*GetNameSafe(this), *GetNameSafe(InCharacter));
		UnbindFromCharacter();
		return;

	}

	UBG_ItemUseComponent* ItemUseComponent = InCharacter->GetItemUseComponent();
	if (!ItemUseComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no ItemUseComponent."),
			*GetNameSafe(this), *GetNameSafe(InCharacter));
		UnbindFromCharacter();
		return;

	}

	BindToCharacter(InCharacter, InventoryComponent, EquipmentComponent, ItemUseComponent);
}

void UBG_InventoryViewModel::NotifyPossessedCharacterCleared()
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to clear possessed character binding."),
			*GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController())
		return;

	UnbindFromCharacter();
	InventoryItems.Reset();
	EquipmentSlots.Reset();
	NearbyWorldItems.Reset();
	ItemUseData = FBGItemUseRenderData();
	CurrentWeight = 0.f;
	MaxWeight = 0.f;
	ForceBroadcastAll();
}

// --- Commands ------------

bool UBG_InventoryViewModel::PickupWorldItem(ABG_WorldItemBase* WorldItem, int32 Quantity)
{
	ABG_Character* Character = GetBoundCharacter(TEXT("PickupWorldItem"));
	if (!Character)
		return false;

	if (!IsValid(WorldItem))
	{
		LOGE(TEXT("%s: PickupWorldItem failed because WorldItem was invalid."),
			*GetNameSafe(this));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::None, FGameplayTag());
		return false;

	}

	if (!(Quantity > 0))
	{
		LOGE(TEXT("%s: PickupWorldItem failed because Quantity=%d was not positive."),
			*GetNameSafe(this), Quantity);
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidQuantity, WorldItem->GetItemType(),
		                       WorldItem->GetItemTag());
		return false;

	}

	Character->PickupWorldItem(WorldItem, Quantity);
	return true;
}

bool UBG_InventoryViewModel::PickupNearbyWorldItem(int32 Quantity)
{
	ABG_WorldItemBase* WorldItem = GetBestNearbyWorldItem();
	if (!WorldItem)
	{
		LOGE(TEXT("%s: PickupNearbyWorldItem failed because no nearby WorldItem was available."),
			*GetNameSafe(this));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::None, FGameplayTag());
		return false;

	}

	return PickupWorldItem(WorldItem, Quantity > 0 ? Quantity : WorldItem->GetQuantity());
}

bool UBG_InventoryViewModel::DropInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity)
{
	UBG_InventoryComponent* InventoryComponent = GetBoundInventoryComponent(TEXT("DropInventoryItem"));
	if (!InventoryComponent)
		return false;

	if (!(Quantity > 0))
	{
		LOGE(TEXT("%s: DropInventoryItem failed because Quantity=%d was not positive."),
			*GetNameSafe(this), Quantity);
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidQuantity, ItemType, ItemTag);
		return false;

	}

	InventoryComponent->DropItem(ItemType, ItemTag, Quantity);
	return true;
}

bool UBG_InventoryViewModel::DropEquipment(EBG_EquipmentSlot EquipmentSlot)
{
	UBG_EquipmentComponent* EquipmentComponent = GetBoundEquipmentComponent(TEXT("DropEquipment"));
	if (!EquipmentComponent)
		return false;

	EquipmentComponent->DropEquipment(EquipmentSlot);
	return true;
}

bool UBG_InventoryViewModel::UseInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag)
{
	UBG_ItemUseComponent* ItemUseComponent = GetBoundItemUseComponent(TEXT("UseInventoryItem"));
	if (!ItemUseComponent)
		return false;

	if (!(ItemType == EBG_ItemType::Heal || ItemType == EBG_ItemType::Boost))
	{
		LOGE(TEXT("%s: UseInventoryItem failed because item type %d is not usable."),
			*GetNameSafe(this), static_cast<int32>(ItemType));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, ItemType, ItemTag);
		return false;

	}

	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: UseInventoryItem failed because ItemTag was invalid."),
			*GetNameSafe(this));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, ItemType, ItemTag);
		return false;

	}

	ItemUseComponent->UseItem(ItemType, ItemTag);
	return true;
}

bool UBG_InventoryViewModel::CancelItemUse()
{
	UBG_ItemUseComponent* ItemUseComponent = GetBoundItemUseComponent(TEXT("CancelItemUse"));
	if (!ItemUseComponent)
		return false;

	ItemUseComponent->CancelItemUse();
	return true;
}

// --- Failure Feedback ------------

void UBG_InventoryViewModel::NotifyInventoryFailure(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	FGameplayTag ItemTag)
{
	LastFailure = BuildFailureRenderData(FailReason, ItemType, ItemTag);
	OnInventoryFailureReceived.Broadcast(LastFailure);
}

// --- Refresh ------------

void UBG_InventoryViewModel::RefreshAllRenderData()
{
	RefreshInventoryRenderData();
	RefreshEquipmentRenderData();
	RefreshNearbyWorldItemRenderData();
	RefreshItemUseRenderData();
}

void UBG_InventoryViewModel::ForceBroadcastAll()
{
	OnInventoryItemsChanged.Broadcast();
	OnInventoryWeightChanged.Broadcast(CurrentWeight, MaxWeight);
	OnEquipmentSlotsChanged.Broadcast();
	OnNearbyWorldItemsChanged.Broadcast();
	OnItemUseChanged.Broadcast(ItemUseData);
}

// --- Binding ------------

void UBG_InventoryViewModel::BindToCharacter(
	ABG_Character* InCharacter,
	UBG_InventoryComponent* InInventoryComponent,
	UBG_EquipmentComponent* InEquipmentComponent,
	UBG_ItemUseComponent* InItemUseComponent)
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to bind inventory state."), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController())
		return;

	if (!(InCharacter && InInventoryComponent && InEquipmentComponent && InItemUseComponent))
	{
		LOGE(TEXT("%s: BindToCharacter failed. Character=%s, Inventory=%s, Equipment=%s, ItemUse=%s."),
			*GetNameSafe(this),
			*GetNameSafe(InCharacter),
			*GetNameSafe(InInventoryComponent),
			*GetNameSafe(InEquipmentComponent),
			*GetNameSafe(InItemUseComponent));
		return;
	}

	if (!(InInventoryComponent->GetOwner() == InCharacter
	      && InEquipmentComponent->GetOwner() == InCharacter
	      && InItemUseComponent->GetOwner() == InCharacter))
	{
		LOGE(TEXT(
			"%s tried to bind components that do not belong to %s. InventoryOwner=%s, EquipmentOwner=%s, ItemUseOwner=%s."
			),
			*GetNameSafe(this),
			*GetNameSafe(InCharacter),
			*GetNameSafe(InInventoryComponent->GetOwner()),
			*GetNameSafe(InEquipmentComponent->GetOwner()),
			*GetNameSafe(InItemUseComponent->GetOwner()));
		return;
	}

	if (BoundCharacter == InCharacter
		&& BoundInventoryComponent == InInventoryComponent
		&& BoundEquipmentComponent == InEquipmentComponent
		&& BoundItemUseComponent == InItemUseComponent)
	{
		RefreshAllRenderData();
		ForceBroadcastAll();
		return;
	}

	UnbindFromCharacter();

	BoundCharacter = InCharacter;
	BoundInventoryComponent = InInventoryComponent;
	BoundEquipmentComponent = InEquipmentComponent;
	BoundItemUseComponent = InItemUseComponent;

	InInventoryComponent->OnInventoryChanged.AddDynamic(this, &UBG_InventoryViewModel::HandleInventoryChanged);
	InInventoryComponent->OnInventoryWeightChanged.AddDynamic(
		this, &UBG_InventoryViewModel::HandleInventoryWeightChanged);
	InEquipmentComponent->OnEquipmentChanged.AddDynamic(this, &UBG_InventoryViewModel::HandleEquipmentChanged);
	InItemUseComponent->OnItemUseStateChanged.AddDynamic(
		this, &UBG_InventoryViewModel::HandleItemUseStateChanged);
	InItemUseComponent->OnItemUseProgressChanged.AddDynamic(
		this, &UBG_InventoryViewModel::HandleItemUseProgressChanged);
	InCharacter->OnNearbyWorldItemsChanged.AddDynamic(this, &UBG_InventoryViewModel::HandleNearbyWorldItemsChanged);

	RefreshAllRenderData();
	ForceBroadcastAll();
}

void UBG_InventoryViewModel::UnbindFromCharacter()
{
	UnbindNearbyWorldItemStateDelegates();

	if (UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get())
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UBG_InventoryViewModel::HandleInventoryChanged);
		InventoryComponent->OnInventoryWeightChanged.RemoveDynamic(
			this, &UBG_InventoryViewModel::HandleInventoryWeightChanged);
	}

	if (UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get())
	{
		EquipmentComponent->OnEquipmentChanged.RemoveDynamic(this, &UBG_InventoryViewModel::HandleEquipmentChanged);
	}

	if (UBG_ItemUseComponent* ItemUseComponent = BoundItemUseComponent.Get())
	{
		ItemUseComponent->OnItemUseStateChanged.RemoveDynamic(
			this, &UBG_InventoryViewModel::HandleItemUseStateChanged);
		ItemUseComponent->OnItemUseProgressChanged.RemoveDynamic(
			this, &UBG_InventoryViewModel::HandleItemUseProgressChanged);
	}

	if (ABG_Character* Character = BoundCharacter.Get())
	{
		Character->OnNearbyWorldItemsChanged.RemoveDynamic(
			this, &UBG_InventoryViewModel::HandleNearbyWorldItemsChanged);
	}

	BoundCharacter = nullptr;
	BoundInventoryComponent = nullptr;
	BoundEquipmentComponent = nullptr;
	BoundItemUseComponent = nullptr;
}

void UBG_InventoryViewModel::HandleInventoryChanged()
{
	RefreshInventoryRenderData();
	OnInventoryItemsChanged.Broadcast();
	OnInventoryWeightChanged.Broadcast(CurrentWeight, MaxWeight);
}

void UBG_InventoryViewModel::HandleInventoryWeightChanged(float NewCurrentWeight, float NewMaxWeight)
{
	CurrentWeight = NewCurrentWeight;
	MaxWeight = NewMaxWeight;
	OnInventoryWeightChanged.Broadcast(CurrentWeight, MaxWeight);

	RefreshInventoryRenderData();
	OnInventoryItemsChanged.Broadcast();
}

void UBG_InventoryViewModel::HandleEquipmentChanged()
{
	RefreshEquipmentRenderData();
	OnEquipmentSlotsChanged.Broadcast();
}

void UBG_InventoryViewModel::HandleNearbyWorldItemsChanged()
{
	RefreshNearbyWorldItemRenderData();
	OnNearbyWorldItemsChanged.Broadcast();
}

void UBG_InventoryViewModel::HandleNearbyWorldItemStateChanged()
{
	RefreshNearbyWorldItemRenderData();
	OnNearbyWorldItemsChanged.Broadcast();
}

void UBG_InventoryViewModel::HandleItemUseStateChanged(const FBG_ItemUseRepState& ItemUseState)
{
	RefreshItemUseRenderData();
	OnItemUseChanged.Broadcast(ItemUseData);
}

void UBG_InventoryViewModel::HandleItemUseProgressChanged(float Progress, float ElapsedTime, float RemainingTime)
{
	RefreshItemUseRenderData();
	OnItemUseChanged.Broadcast(ItemUseData);
}

// --- Render Data ------------

void UBG_InventoryViewModel::RefreshInventoryRenderData()
{
	InventoryItems.Reset();

	const UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
	if (!InventoryComponent)
	{
		LOGE(TEXT("%s could not refresh inventory render data because BoundInventoryComponent was null."),
			*GetNameSafe(this));
		CurrentWeight = 0.f;
		MaxWeight = 0.f;
		return;

	}

	for (const FBG_InventoryEntry& Entry : InventoryComponent->GetInventoryEntries())
	{
		if (!Entry.IsValidEntry())
			continue;

		InventoryItems.Add(BuildInventoryItemRenderData(Entry.ItemType, Entry.ItemTag, Entry.Quantity));
	}

	CurrentWeight = InventoryComponent->GetCurrentWeight();
	MaxWeight = InventoryComponent->GetMaxWeight();
}

void UBG_InventoryViewModel::RefreshEquipmentRenderData()
{
	EquipmentSlots.Reset();

	const UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get();
	if (!EquipmentComponent)
	{
		LOGE(TEXT("%s could not refresh equipment render data because BoundEquipmentComponent was null."),
			*GetNameSafe(this));
		return;
	}

	for (const EBG_EquipmentSlot Slot : EquipmentSlotRenderOrder)
	{
		EquipmentSlots.Add(BuildEquipmentSlotRenderData(Slot));
	}
}

void UBG_InventoryViewModel::RefreshNearbyWorldItemRenderData()
{
	NearbyWorldItems.Reset();

	const ABG_Character* Character = BoundCharacter.Get();
	if (!Character)
	{
		LOGE(TEXT("%s could not refresh nearby item render data because BoundCharacter was null."),
			*GetNameSafe(this));
		UnbindNearbyWorldItemStateDelegates();
		return;

	}

	for (ABG_WorldItemBase* WorldItem : Character->GetNearbyWorldItems())
	{
		if (!IsValid(WorldItem))
			continue;

		NearbyWorldItems.Add(BuildNearbyWorldItemRenderData(WorldItem));
	}

	RebindNearbyWorldItemStateDelegates();
}

void UBG_InventoryViewModel::RefreshItemUseRenderData()
{
	ItemUseData = BuildItemUseRenderData();
}

void UBG_InventoryViewModel::RebindNearbyWorldItemStateDelegates()
{
	UnbindNearbyWorldItemStateDelegates();

	for (const FBGNearbyWorldItemRenderData& NearbyWorldItem : NearbyWorldItems)
	{
		ABG_WorldItemBase* WorldItem = NearbyWorldItem.WorldItem;
		if (!IsValid(WorldItem))
			continue;

		WorldItem->OnWorldItemStateChanged.AddDynamic(
			this, &UBG_InventoryViewModel::HandleNearbyWorldItemStateChanged);
		BoundNearbyWorldItemActors.Add(WorldItem);
	}
}

void UBG_InventoryViewModel::UnbindNearbyWorldItemStateDelegates()
{
	for (const TWeakObjectPtr<ABG_WorldItemBase>& WorldItemPtr : BoundNearbyWorldItemActors)
	{
		if (ABG_WorldItemBase* WorldItem = WorldItemPtr.Get())
		{
			WorldItem->OnWorldItemStateChanged.RemoveDynamic(
				this, &UBG_InventoryViewModel::HandleNearbyWorldItemStateChanged);
		}
	}

	BoundNearbyWorldItemActors.Reset();
}

FBGInventoryItemRenderData UBG_InventoryViewModel::BuildInventoryItemRenderData(
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	int32 Quantity) const
{
	FBGInventoryItemRenderData RenderData;
	RenderData.ItemType = ItemType;
	RenderData.ItemTag = ItemTag;
	RenderData.Quantity = FMath::Max(Quantity, 0);
	RenderData.DisplayName = GetFallbackItemName(ItemTag);

	const FBG_ItemDataRow* ItemRow = FindItemRow(ItemType, ItemTag, TEXT("BuildInventoryItemRenderData"));
	if (!ItemRow)
		return RenderData;

	ApplyItemDisplayData(*ItemRow, RenderData.DisplayName, RenderData.Description, RenderData.Icon);
	RenderData.UnitWeight = ItemRow->UnitWeight;
	RenderData.TotalWeight = ItemRow->UnitWeight * RenderData.Quantity;
	RenderData.bStackable = ItemRow->bStackable;
	RenderData.MaxStackSize = ItemRow->MaxStackSize;
	RenderData.bHasDisplayRow = true;
	return RenderData;
}

FBGEquipmentSlotRenderData UBG_InventoryViewModel::BuildEquipmentSlotRenderData(EBG_EquipmentSlot Slot) const
{
	FBGEquipmentSlotRenderData RenderData;
	RenderData.Slot = Slot;
	RenderData.ItemType = GetItemTypeForEquipmentSlot(Slot);

	const UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get();
	if (!EquipmentComponent)
	{
		LOGE(TEXT("%s could not build equipment render data because BoundEquipmentComponent was null."),
			*GetNameSafe(this));
		return RenderData;
	}

	RenderData.ItemTag = EquipmentComponent->GetEquippedItemTag(Slot);
	RenderData.bEquipped = RenderData.ItemTag.IsValid();
	RenderData.bActive = EquipmentComponent->GetActiveWeaponSlot() == Slot;

	if (!RenderData.bEquipped)
		return RenderData;

	RenderData.DisplayName = GetFallbackItemName(RenderData.ItemTag);

	if (RenderData.ItemType == EBG_ItemType::Weapon)
	{
		RenderData.LoadedAmmo = EquipmentComponent->GetLoadedAmmo(Slot);
	}
	else if (RenderData.ItemType == EBG_ItemType::Armor)
	{
		RenderData.Durability = EquipmentComponent->GetArmorDurability(Slot);
	}
	else if (RenderData.ItemType == EBG_ItemType::Throwable)
	{
		const UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
				if (InventoryComponent){
			RenderData.Quantity = InventoryComponent->GetQuantity(EBG_ItemType::Throwable, RenderData.ItemTag);
		}
		else
		{
			LOGE(TEXT("%s could not read throwable quantity because BoundInventoryComponent was null."),
				*GetNameSafe(this));
		}
	}

	const FBG_ItemDataRow* ItemRow = FindItemRow(
		RenderData.ItemType, RenderData.ItemTag, TEXT("BuildEquipmentSlotRenderData"));
	if (!ItemRow)
		return RenderData;

	ApplyItemDisplayData(*ItemRow, RenderData.DisplayName, RenderData.Description, RenderData.Icon);
	RenderData.bHasDisplayRow = true;
	return RenderData;
}

FBGNearbyWorldItemRenderData UBG_InventoryViewModel::BuildNearbyWorldItemRenderData(
	ABG_WorldItemBase* WorldItem) const
{
	FBGNearbyWorldItemRenderData RenderData;
	RenderData.WorldItem = WorldItem;

	if (!IsValid(WorldItem))
	{
		LOGE(TEXT("%s could not build nearby world item render data because WorldItem was invalid."),
			*GetNameSafe(this));
		return RenderData;
	}

	RenderData.ItemType = WorldItem->GetItemType();
	RenderData.ItemTag = WorldItem->GetItemTag();
	RenderData.Quantity = WorldItem->GetQuantity();
	RenderData.DisplayName = GetFallbackItemName(RenderData.ItemTag);

	if (const ABG_Character* Character = BoundCharacter.Get())
	{
		RenderData.Distance = FVector::Dist(Character->GetActorLocation(), WorldItem->GetActorLocation());
	}

	const FBG_ItemDataRow* ItemRow = FindItemRow(
		RenderData.ItemType, RenderData.ItemTag, TEXT("BuildNearbyWorldItemRenderData"));
	if (!ItemRow)
		return RenderData;

	ApplyItemDisplayData(*ItemRow, RenderData.DisplayName, RenderData.Description, RenderData.Icon);
	RenderData.bHasDisplayRow = true;
	return RenderData;
}

FBGInventoryFailureRenderData UBG_InventoryViewModel::BuildFailureRenderData(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag) const
{
	FBGInventoryFailureRenderData RenderData;
	RenderData.FailReason = FailReason;
	RenderData.ItemType = ItemType;
	RenderData.ItemTag = ItemTag;
	RenderData.DisplayName = GetFallbackItemName(ItemTag);

	const FBG_ItemDataRow* ItemRow = FindItemRow(ItemType, ItemTag, TEXT("BuildFailureRenderData"));
	if (!ItemRow)
		return RenderData;

	if (!ItemRow->DisplayName.IsEmpty())
	{
		RenderData.DisplayName = ItemRow->DisplayName;
	}
	RenderData.bHasDisplayRow = true;
	return RenderData;
}

FBGItemUseRenderData UBG_InventoryViewModel::BuildItemUseRenderData() const
{
	FBGItemUseRenderData RenderData;

	const UBG_ItemUseComponent* ItemUseComponent = BoundItemUseComponent.Get();
	if (!ItemUseComponent)
		return RenderData;

	const FBG_ItemUseRepState ItemUseState = ItemUseComponent->GetItemUseState();
	RenderData.bIsUsingItem = ItemUseState.bIsUsingItem;
	RenderData.ItemType = ItemUseState.ItemType;
	RenderData.ItemTag = ItemUseState.ItemTag;
	RenderData.Progress = ItemUseComponent->GetItemUseProgress();
	RenderData.RemainingTime = ItemUseComponent->GetRemainingUseTime();
	RenderData.UseDuration = ItemUseState.UseDuration;
	RenderData.ElapsedTime = FMath::Max(0.f, RenderData.UseDuration - RenderData.RemainingTime);
	RenderData.DisplayName = GetFallbackItemName(RenderData.ItemTag);

	if (!RenderData.bIsUsingItem)
		return RenderData;

	const FBG_ItemDataRow* ItemRow = FindItemRow(
		RenderData.ItemType, RenderData.ItemTag, TEXT("BuildItemUseRenderData"));
	if (!ItemRow)
		return RenderData;

	ApplyItemDisplayData(*ItemRow, RenderData.DisplayName, RenderData.Description, RenderData.Icon);
	RenderData.bHasDisplayRow = true;
	return RenderData;
}

void UBG_InventoryViewModel::ApplyItemDisplayData(
	const FBG_ItemDataRow& ItemRow,
	FText& OutDisplayName,
	FText& OutDescription,
	TSoftObjectPtr<UTexture2D>& OutIcon) const
{
	if (!ItemRow.DisplayName.IsEmpty())
	{
		OutDisplayName = ItemRow.DisplayName;
	}

	OutDescription = ItemRow.Description;
	OutIcon = ItemRow.Icon;
}

// --- Item Data ------------

const FBG_ItemDataRow* UBG_InventoryViewModel::FindItemRow(
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	const TCHAR* OperationName) const
{
	if (ItemType == EBG_ItemType::None || !ItemTag.IsValid())
		return nullptr;

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	if (!RegistrySubsystem)
		return nullptr;

	FString FailureReason;
	const FBG_ItemDataRow* ItemRow = RegistrySubsystem->FindItemRow(ItemType, ItemTag, &FailureReason);
	if (!ItemRow)
	{
		LOGE(TEXT("%s: %s failed to find item row. ItemType=%d, ItemTag=%s, Reason=%s."),
			*GetNameSafe(this), OperationName, static_cast<int32>(ItemType),
			*ItemTag.ToString(), *FailureReason);
	}
	return ItemRow;
}

UBG_ItemDataRegistrySubsystem* UBG_InventoryViewModel::GetItemDataRegistrySubsystem(
	const TCHAR* OperationName) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: %s failed because World was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	const UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		LOGE(TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(this),
			OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem =
		GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		LOGE(TEXT("%s: %s failed because ItemDataRegistrySubsystem was null."),
			*GetNameSafe(this), OperationName);
	}
	return RegistrySubsystem;
}

// --- Component Access ------------

ABG_Character* UBG_InventoryViewModel::GetBoundCharacter(const TCHAR* OperationName) const
{
	ABG_Character* Character = BoundCharacter.Get();
	if (!Character)
	{
		LOGE(TEXT("%s: %s failed because BoundCharacter was null."), *GetNameSafe(this), OperationName);
	}
	return Character;
}

UBG_InventoryComponent* UBG_InventoryViewModel::GetBoundInventoryComponent(const TCHAR* OperationName) const
{
	UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
	if (!InventoryComponent)
	{
		LOGE(TEXT("%s: %s failed because BoundInventoryComponent was null."), *GetNameSafe(this),
			OperationName);
	}
	return InventoryComponent;
}

UBG_EquipmentComponent* UBG_InventoryViewModel::GetBoundEquipmentComponent(const TCHAR* OperationName) const
{
	UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get();
	if (!EquipmentComponent)
	{
		LOGE(TEXT("%s: %s failed because BoundEquipmentComponent was null."), *GetNameSafe(this),
			OperationName);
	}
	return EquipmentComponent;
}

UBG_ItemUseComponent* UBG_InventoryViewModel::GetBoundItemUseComponent(const TCHAR* OperationName) const
{
	UBG_ItemUseComponent* ItemUseComponent = BoundItemUseComponent.Get();
	if (!ItemUseComponent)
	{
		LOGE(TEXT("%s: %s failed because BoundItemUseComponent was null."), *GetNameSafe(this),
			OperationName);
	}
	return ItemUseComponent;
}

ABG_WorldItemBase* UBG_InventoryViewModel::GetBestNearbyWorldItem() const
{
	if (ABG_Character* Character = BoundCharacter.Get())
	{
		if (ABG_WorldItemBase* CurrentWorldItem = Character->GetCurrentWorldItem())
		{
			return CurrentWorldItem;
		}
	}

	for (const FBGNearbyWorldItemRenderData& NearbyWorldItem : NearbyWorldItems)
	{
		if (IsValid(NearbyWorldItem.WorldItem))
		{
			return NearbyWorldItem.WorldItem;
		}
	}

	return nullptr;
}

// --- Utility ------------

EBG_ItemType UBG_InventoryViewModel::GetItemTypeForEquipmentSlot(EBG_EquipmentSlot Slot)
{
	switch (Slot)
	{
	case EBG_EquipmentSlot::PrimaryA:
	case EBG_EquipmentSlot::PrimaryB:
	case EBG_EquipmentSlot::Pistol:
	case EBG_EquipmentSlot::Melee:
		return EBG_ItemType::Weapon;
	case EBG_EquipmentSlot::Throwable:
		return EBG_ItemType::Throwable;
	case EBG_EquipmentSlot::Helmet:
	case EBG_EquipmentSlot::Vest:
		return EBG_ItemType::Armor;
	case EBG_EquipmentSlot::Backpack:
		return EBG_ItemType::Backpack;
	default:
		return EBG_ItemType::None;
	}
}

FText UBG_InventoryViewModel::GetFallbackItemName(const FGameplayTag& ItemTag)
{
	if (!ItemTag.IsValid())
		return FText::GetEmpty();

	return FText::FromString(ItemTag.ToString());
}
