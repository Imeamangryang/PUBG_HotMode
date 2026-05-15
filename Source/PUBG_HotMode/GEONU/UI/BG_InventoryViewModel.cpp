// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BG_InventoryViewModel.h"

#include "Combat/BG_EquippedWeaponBase.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemUseComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemDataRow.h"
#include "Inventory/BG_WorldItemBase.h"
#include "Materials/MaterialInterface.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"
#include "UI/BG_WeaponIconCaptureComponent.h"

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

	bool IsCapacityFailure(EBGInventoryFailReason FailReason)
	{
		return FailReason == EBGInventoryFailReason::Overweight;
	}

	EBG_EquipmentSlot ResolveFailureHighlightEquipmentSlot(EBGInventoryFailReason FailReason)
	{
		return IsCapacityFailure(FailReason) ? EBG_EquipmentSlot::Backpack : EBG_EquipmentSlot::None;
	}

	FText BuildFailureUserMessage(EBGInventoryFailReason FailReason, EBG_ItemType ItemType)
	{
		switch (FailReason)
		{
		case EBGInventoryFailReason::InvalidItem:
			return NSLOCTEXT("BGInventoryFailure", "InvalidItem", "오류: 아이템을 사용할 수 없습니다.");
		case EBGInventoryFailReason::InvalidQuantity:
			return NSLOCTEXT("BGInventoryFailure", "InvalidQuantity", "오류: 아이템 수량이 올바르지 않습니다.");
		case EBGInventoryFailReason::TooFar:
			return NSLOCTEXT("BGInventoryFailure", "TooFar", "아이템이 너무 멀리 있습니다.");
		case EBGInventoryFailReason::Overweight:
			return ItemType == EBG_ItemType::Backpack
				       ? NSLOCTEXT("BGInventoryFailure", "BackpackCapacityRejected", "너무 많은 아이템을 가지고 있습니다.")
				       : NSLOCTEXT("BGInventoryFailure", "InventoryCapacityExceeded", "인벤토리 용량이 부족합니다.");
		case EBGInventoryFailReason::SlotMismatch:
			return NSLOCTEXT("BGInventoryFailure", "SlotMismatch", "해당 위치에 장착할 수 없습니다.");
		case EBGInventoryFailReason::MissingAmmo:
			return NSLOCTEXT("BGInventoryFailure", "MissingAmmo", "탄약이 부족합니다.");
		case EBGInventoryFailReason::AlreadyUsingItem:
			return NSLOCTEXT("BGInventoryFailure", "AlreadyUsingItem", "사용중인 아이템입니다.");
		case EBGInventoryFailReason::HealthCapReached:
			return NSLOCTEXT("BGInventoryFailure", "HealthCapReached", "더 이상 회복할 수 없습니다.");
		case EBGInventoryFailReason::ServerRejected:
			return NSLOCTEXT("BGInventoryFailure", "ServerRejected", "오류: 서버가 요청을 거부했습니다.");
		case EBGInventoryFailReason::None:
		default:
			return FText::GetEmpty();
		}
	}

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
	PickupPromptData = FBGPickupPromptRenderData();
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
	if (EquipmentSlot == EBG_EquipmentSlot::None)
	{
		LOGE(TEXT("%s: DropEquipment failed because EquipmentSlot was None."), *GetNameSafe(this));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::None, FGameplayTag());
		return false;
	}

	UBG_EquipmentComponent* EquipmentComponent = GetBoundEquipmentComponent(TEXT("DropEquipment"));
	if (!EquipmentComponent)
		return false;

	const FGameplayTag EquippedItemTag = EquipmentComponent->GetEquippedItemTag(EquipmentSlot);
	if (!EquippedItemTag.IsValid())
	{
		LOGE(TEXT("%s: DropEquipment failed because slot %d was empty."),
			*GetNameSafe(this), static_cast<int32>(EquipmentSlot));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, GetItemTypeForEquipmentSlot(EquipmentSlot),
		                       EquippedItemTag);
		return false;
	}

	EquipmentComponent->DropEquipment(EquipmentSlot);
	return true;
}

bool UBG_InventoryViewModel::SwapWeaponSlots(EBG_EquipmentSlot SourceSlot, EBG_EquipmentSlot TargetSlot)
{
	if (!IsPrimaryWeaponSlot(SourceSlot) || !IsPrimaryWeaponSlot(TargetSlot) || SourceSlot == TargetSlot)
	{
		LOGE(TEXT("%s: SwapWeaponSlots failed because SourceSlot=%d and TargetSlot=%d are not a valid PrimaryA/B pair."),
			*GetNameSafe(this), static_cast<int32>(SourceSlot), static_cast<int32>(TargetSlot));
		NotifyInventoryFailure(EBGInventoryFailReason::SlotMismatch, EBG_ItemType::Weapon, FGameplayTag());
		return false;
	}

	UBG_EquipmentComponent* EquipmentComponent = GetBoundEquipmentComponent(TEXT("SwapWeaponSlots"));
	if (!EquipmentComponent)
		return false;

	const FGameplayTag SourceItemTag = EquipmentComponent->GetEquippedItemTag(SourceSlot);
	const FGameplayTag TargetItemTag = EquipmentComponent->GetEquippedItemTag(TargetSlot);
	if (!SourceItemTag.IsValid() || !TargetItemTag.IsValid())
	{
		LOGE(TEXT("%s: SwapWeaponSlots failed because SourceSlot=%d or TargetSlot=%d was empty."),
			*GetNameSafe(this), static_cast<int32>(SourceSlot), static_cast<int32>(TargetSlot));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::Weapon,
		                       SourceItemTag.IsValid() ? TargetItemTag : SourceItemTag);
		return false;
	}

	EquipmentComponent->SwapWeaponSlots(SourceSlot, TargetSlot);
	return true;
}

bool UBG_InventoryViewModel::MoveEquippedWeaponSlot(
	EBG_EquipmentSlot SourceSlot,
	EBG_EquipmentSlot TargetSlot)
{
	const bool bIsAllowedPrimarySwap =
		(SourceSlot == EBG_EquipmentSlot::PrimaryA && TargetSlot == EBG_EquipmentSlot::PrimaryB)
		|| (SourceSlot == EBG_EquipmentSlot::PrimaryB && TargetSlot == EBG_EquipmentSlot::PrimaryA);
	if (!bIsAllowedPrimarySwap)
	{
		LOGE(TEXT("%s: MoveEquippedWeaponSlot failed because SourceSlot=%d and TargetSlot=%d are not a PrimaryA/B swap pair."),
			*GetNameSafe(this), static_cast<int32>(SourceSlot), static_cast<int32>(TargetSlot));
		NotifyInventoryFailure(EBGInventoryFailReason::SlotMismatch, EBG_ItemType::Weapon, FGameplayTag());
		return false;
	}

	return SwapWeaponSlots(SourceSlot, TargetSlot);
}

bool UBG_InventoryViewModel::SelectThrowable(FGameplayTag ThrowableItemTag)
{
	if (!ThrowableItemTag.IsValid())
	{
		LOGE(TEXT("%s: SelectThrowable failed because ThrowableItemTag was invalid."), *GetNameSafe(this));
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::Throwable, ThrowableItemTag);
		return false;
	}

	UBG_EquipmentComponent* EquipmentComponent = GetBoundEquipmentComponent(TEXT("SelectThrowable"));
	if (!EquipmentComponent)
		return false;

	UBG_InventoryComponent* InventoryComponent = GetBoundInventoryComponent(TEXT("SelectThrowable"));
	if (!InventoryComponent)
		return false;

	if (!FindItemRow(EBG_ItemType::Throwable, ThrowableItemTag, TEXT("SelectThrowable")))
	{
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::Throwable, ThrowableItemTag);
		return false;
	}

	if (InventoryComponent->GetQuantity(EBG_ItemType::Throwable, ThrowableItemTag) <= 0)
	{
		LOGE(TEXT("%s: SelectThrowable failed because inventory has no throwable %s."),
			*GetNameSafe(this), *ThrowableItemTag.ToString());
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidQuantity, EBG_ItemType::Throwable, ThrowableItemTag);
		return false;
	}

	EquipmentComponent->SetThrowable(ThrowableItemTag);
	return true;
}

bool UBG_InventoryViewModel::ClearThrowable()
{
	UBG_EquipmentComponent* EquipmentComponent = GetBoundEquipmentComponent(TEXT("ClearThrowable"));
	if (!EquipmentComponent)
		return false;

	EquipmentComponent->ClearThrowable();
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
	RefreshPickupPromptRenderData();
	RefreshItemUseRenderData();
}

void UBG_InventoryViewModel::ForceBroadcastAll()
{
	OnInventoryItemsChanged.Broadcast();
	OnInventoryWeightChanged.Broadcast(CurrentWeight, MaxWeight);
	OnEquipmentSlotsChanged.Broadcast();
	OnNearbyWorldItemsChanged.Broadcast();
	OnPickupPromptChanged.Broadcast(PickupPromptData);
	OnItemUseChanged.Broadcast(ItemUseData);
}

bool UBG_InventoryViewModel::GetEquipmentSlotData(
	EBG_EquipmentSlot Slot,
	FBGEquipmentSlotRenderData& OutSlotData) const
{
	OutSlotData = FBGEquipmentSlotRenderData();

	if (Slot == EBG_EquipmentSlot::None)
		return false;

	if (const FBGEquipmentSlotRenderData* SlotData = EquipmentSlots.Find(Slot))
	{
		OutSlotData = *SlotData;
		return true;
	}

	OutSlotData.Slot = Slot;
	OutSlotData.ItemType = GetItemTypeForEquipmentSlot(Slot);
	return false;
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
	RefreshEquipmentRenderData();
	OnInventoryItemsChanged.Broadcast();
	OnInventoryWeightChanged.Broadcast(CurrentWeight, MaxWeight);
	OnEquipmentSlotsChanged.Broadcast();
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
	RefreshPickupPromptRenderData();
	OnNearbyWorldItemsChanged.Broadcast();
	OnPickupPromptChanged.Broadcast(PickupPromptData);
}

void UBG_InventoryViewModel::HandleNearbyWorldItemStateChanged()
{
	RefreshNearbyWorldItemRenderData();
	RefreshPickupPromptRenderData();
	OnNearbyWorldItemsChanged.Broadcast();
	OnPickupPromptChanged.Broadcast(PickupPromptData);
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
		EquipmentSlots.Add(Slot, BuildEquipmentSlotRenderData(Slot));
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

void UBG_InventoryViewModel::RefreshPickupPromptRenderData()
{
	PickupPromptData = BuildPickupPromptRenderData(GetBestNearbyWorldItem());
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
		RenderData.bCanDrag = IsPrimaryWeaponSlot(Slot);
		RenderData.bCanDropToGround = IsWeaponSlot(Slot);
		RenderData.bCanSwapWithPrimarySlot = IsPrimaryWeaponSlot(Slot);
		RenderData.LoadedAmmo = FMath::Max(0, EquipmentComponent->GetLoadedAmmo(Slot));

		if (const ABG_EquippedWeaponBase* WeaponActor = EquipmentComponent->GetEquippedWeaponActor(Slot))
		{
			RenderData.LoadedAmmo = FMath::Max(0, WeaponActor->GetLoadedAmmo());
			RenderData.MagazineSize = FMath::Max(0, WeaponActor->GetMagazineCapacity());
		}
		else
		{
			LOGE(TEXT("%s could not read runtime weapon ammo because slot %d has no equipped weapon actor."),
				*GetNameSafe(this), static_cast<int32>(Slot));
		}
	}
	else if (RenderData.ItemType == EBG_ItemType::Armor)
	{
		RenderData.Durability = EquipmentComponent->GetArmorDurability(Slot);
	}
	else if (RenderData.ItemType == EBG_ItemType::Throwable)
	{
		const UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
		if (InventoryComponent)
		{
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

	if (RenderData.ItemType == EBG_ItemType::Weapon)
	{
		const FBG_WeaponItemDataRow* WeaponRow = static_cast<const FBG_WeaponItemDataRow*>(ItemRow);
		FGameplayTag AmmoItemTag = WeaponRow->AmmoItemTag;
		if (const ABG_EquippedWeaponBase* WeaponActor = EquipmentComponent->GetEquippedWeaponActor(Slot))
		{
			if (WeaponActor->GetAmmoItemTag().IsValid())
			{
				AmmoItemTag = WeaponActor->GetAmmoItemTag();
			}

			if (WeaponActor->GetMagazineCapacity() > 0)
			{
				RenderData.MagazineSize = WeaponActor->GetMagazineCapacity();
			}
		}

		if (RenderData.MagazineSize <= 0)
		{
			RenderData.MagazineSize = FMath::Max(0, WeaponRow->MagazineSize);
		}

		RenderData.LoadedAmmo = RenderData.MagazineSize > 0
			? FMath::Clamp(RenderData.LoadedAmmo, 0, RenderData.MagazineSize)
			: 0;

		if (AmmoItemTag.IsValid() && RenderData.MagazineSize > 0)
		{
			const UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
			if (InventoryComponent)
			{
				RenderData.ReserveAmmo = FMath::Max(
					0,
					InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoItemTag));
			}
			else
			{
				LOGE(TEXT("%s could not read reserve ammo because BoundInventoryComponent was null."),
					*GetNameSafe(this));
			}
		}

		if (!WeaponRow->EquippedWeaponClass.IsNull())
		{
			RenderData.PreviewIconKey = UBG_WeaponIconCaptureComponent::MakePreviewIconKey(
				RenderData.ItemTag,
				RenderData.Slot,
				WeaponRow->EquippedWeaponClass);
			RenderData.bUseRuntimePreviewIcon = !RenderData.PreviewIconKey.IsNone();
		}
		else
		{
			LOGE(TEXT("%s could not enable runtime preview because weapon row %s had no EquippedWeaponClass."),
				*GetNameSafe(this),
				*RenderData.ItemTag.ToString());
		}
	}
	else if (RenderData.ItemType == EBG_ItemType::Armor)
	{
		const FBG_ArmorItemDataRow* ArmorRow = static_cast<const FBG_ArmorItemDataRow*>(ItemRow);
		RenderData.MaxDurability = FMath::Max(0.f, ArmorRow->MaxDurability);
		RenderData.EquipmentLevel = FMath::Max(0, ArmorRow->EquipmentLevel);
	}
	else if (RenderData.ItemType == EBG_ItemType::Backpack)
	{
		const FBG_BackpackItemDataRow* BackpackRow = static_cast<const FBG_BackpackItemDataRow*>(ItemRow);
		RenderData.EquipmentLevel = FMath::Max(0, BackpackRow->EquipmentLevel);
		RenderData.BackpackWeightBonus = FMath::Max(0.f, BackpackRow->WeightBonus);
	}

	RenderData.bHasDisplayRow = true;
	RenderData.PreviewIconResource = ResolveEquipmentPreviewIconResource(RenderData);
	RenderData.PreviewIconDisplaySize = ResolveEquipmentPreviewIconDisplaySize(
		RenderData,
		RenderData.PreviewIconResource);
	RenderData.bHasPreviewIconBrush = BuildPreviewIconBrush(
		RenderData.PreviewIconResource,
		RenderData.PreviewIconDisplaySize,
		RenderData.PreviewIconBrush);
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

FBGPickupPromptRenderData UBG_InventoryViewModel::BuildPickupPromptRenderData(
	ABG_WorldItemBase* WorldItem) const
{
	FBGPickupPromptRenderData RenderData;

	if (!IsValid(WorldItem))
	{
		return RenderData;
	}

	const EBG_ItemType ItemType = WorldItem->GetItemType();
	const FGameplayTag ItemTag = WorldItem->GetItemTag();
	RenderData.DisplayName = GetFallbackItemName(ItemTag);

	const FBG_ItemDataRow* ItemRow = FindItemRow(
		ItemType, ItemTag, TEXT("BuildPickupPromptRenderData"));
	if (ItemRow && !ItemRow->DisplayName.IsEmpty())
	{
		RenderData.DisplayName = ItemRow->DisplayName;
	}

	RenderData.bVisible = !RenderData.DisplayName.IsEmpty();
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
	RenderData.UserMessage = BuildFailureUserMessage(FailReason, ItemType);
	RenderData.HighlightEquipmentSlot = ResolveFailureHighlightEquipmentSlot(FailReason);
	RenderData.bIsCapacityFailure = IsCapacityFailure(FailReason);
	RenderData.bShouldFlashBackpackIcon = RenderData.HighlightEquipmentSlot == EBG_EquipmentSlot::Backpack;

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

UObject* UBG_InventoryViewModel::ResolveEquipmentPreviewIconResource(
	const FBGEquipmentSlotRenderData& RenderData) const
{
	if (!RenderData.bEquipped || !RenderData.bUseRuntimePreviewIcon)
	{
		return nullptr;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		LOGE(TEXT("%s could not resolve equipment preview icon resource because owner was null."),
			*GetNameSafe(this));
		return nullptr;
	}

	UBG_WeaponIconCaptureComponent* WeaponIconCaptureComponent =
		OwnerActor->FindComponentByClass<UBG_WeaponIconCaptureComponent>();
	if (!WeaponIconCaptureComponent)
	{
		LOGE(TEXT("%s could not resolve equipment preview icon resource because owner %s had no WeaponIconCaptureComponent."),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor));
		return nullptr;
	}

	return WeaponIconCaptureComponent->GetOrCreateWeaponSlotIconResource(RenderData);
}

FVector2D UBG_InventoryViewModel::ResolveEquipmentPreviewIconDisplaySize(
	const FBGEquipmentSlotRenderData& RenderData,
	UObject* PreviewIconResource) const
{
	if (!RenderData.bEquipped || !RenderData.bUseRuntimePreviewIcon || RenderData.PreviewIconKey.IsNone())
	{
		return FVector2D::ZeroVector;
	}

	if (!PreviewIconResource)
	{
		return FVector2D::ZeroVector;
	}

	if (!Cast<UTextureRenderTarget2D>(PreviewIconResource) && !Cast<UMaterialInterface>(PreviewIconResource))
	{
		return FVector2D::ZeroVector;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		LOGE(TEXT("%s could not resolve equipment preview icon display size because owner was null."),
			*GetNameSafe(this));
		return FVector2D::ZeroVector;
	}

	UBG_WeaponIconCaptureComponent* WeaponIconCaptureComponent =
		OwnerActor->FindComponentByClass<UBG_WeaponIconCaptureComponent>();
	if (!WeaponIconCaptureComponent)
	{
		LOGE(TEXT("%s could not resolve equipment preview icon display size because owner %s had no WeaponIconCaptureComponent."),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor));
		return FVector2D::ZeroVector;
	}

	FBGWeaponIconPreviewMetrics PreviewMetrics;
	if (!WeaponIconCaptureComponent->GetPreviewCaptureMetrics(RenderData.PreviewIconKey, PreviewMetrics))
	{
		return WeaponIconCaptureComponent->GetPreviewRenderTargetSize();
	}

	const FVector2D ReferenceOrthoSize = WeaponIconCaptureComponent->GetPreviewReferenceOrthoSize();
	if (ReferenceOrthoSize.X <= KINDA_SMALL_NUMBER || ReferenceOrthoSize.Y <= KINDA_SMALL_NUMBER)
	{
		LOGE(TEXT("%s could not resolve equipment preview icon display size because reference ortho size was invalid. Size=%s"),
			*GetNameSafe(this),
			*ReferenceOrthoSize.ToString());
		return WeaponIconCaptureComponent->GetPreviewRenderTargetSize();
	}

	const FVector2D BaseDisplaySize = WeaponIconCaptureComponent->GetPreviewRenderTargetSize();
	return FVector2D(
		FMath::Max(1.f, BaseDisplaySize.X * PreviewMetrics.OrthoWidth / ReferenceOrthoSize.X),
		FMath::Max(1.f, BaseDisplaySize.Y * PreviewMetrics.OrthoHeight / ReferenceOrthoSize.Y));
}

bool UBG_InventoryViewModel::BuildPreviewIconBrush(
	UObject* PreviewIconResource,
	const FVector2D& PreviewIconDisplaySize,
	FSlateBrush& OutBrush) const
{
	OutBrush = FSlateBrush();
	if (!PreviewIconResource)
		return false;

	OutBrush.SetResourceObject(PreviewIconResource);
	OutBrush.DrawAs = ESlateBrushDrawType::Image;
	const bool bHasDisplaySizeOverride =
		PreviewIconDisplaySize.X > KINDA_SMALL_NUMBER && PreviewIconDisplaySize.Y > KINDA_SMALL_NUMBER;

	if (const UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(PreviewIconResource))
	{
		OutBrush.ImageSize = FVector2D(
			static_cast<float>(RenderTarget->SizeX),
			static_cast<float>(RenderTarget->SizeY));
		if (bHasDisplaySizeOverride)
		{
			OutBrush.ImageSize = PreviewIconDisplaySize;
		}
		return true;
	}

	if (const UTexture2D* Texture = Cast<UTexture2D>(PreviewIconResource))
	{
		OutBrush.ImageSize = FVector2D(
			static_cast<float>(Texture->GetSizeX()),
			static_cast<float>(Texture->GetSizeY()));
		if (bHasDisplaySizeOverride)
		{
			OutBrush.ImageSize = PreviewIconDisplaySize;
		}
		return true;
	}

	if (Cast<UMaterialInterface>(PreviewIconResource))
	{
		OutBrush.ImageSize = FVector2D(512.f, 256.f);

		if (const AActor* OwnerActor = GetOwner())
		{
			if (const UBG_WeaponIconCaptureComponent* WeaponIconCaptureComponent =
				OwnerActor->FindComponentByClass<UBG_WeaponIconCaptureComponent>())
			{
				OutBrush.ImageSize = WeaponIconCaptureComponent->GetPreviewRenderTargetSize();
			}
		}

		if (bHasDisplaySizeOverride)
		{
			OutBrush.ImageSize = PreviewIconDisplaySize;
		}
		return true;
	}

	LOGE(TEXT("%s could not build preview icon brush because resource %s was not a supported texture or material type."),
		*GetNameSafe(this),
		*GetNameSafe(PreviewIconResource));
	OutBrush = FSlateBrush();
	return false;
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

bool UBG_InventoryViewModel::IsWeaponSlot(EBG_EquipmentSlot Slot)
{
	return Slot == EBG_EquipmentSlot::PrimaryA
		|| Slot == EBG_EquipmentSlot::PrimaryB
		|| Slot == EBG_EquipmentSlot::Pistol
		|| Slot == EBG_EquipmentSlot::Melee;
}

bool UBG_InventoryViewModel::IsPrimaryWeaponSlot(EBG_EquipmentSlot Slot)
{
	return Slot == EBG_EquipmentSlot::PrimaryA || Slot == EBG_EquipmentSlot::PrimaryB;
}

FText UBG_InventoryViewModel::GetFallbackItemName(const FGameplayTag& ItemTag)
{
	if (!ItemTag.IsValid())
		return FText::GetEmpty();

	return FText::FromString(ItemTag.ToString());
}
