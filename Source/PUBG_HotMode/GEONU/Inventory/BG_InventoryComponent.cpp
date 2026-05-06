// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_InventoryComponent.h"

#include "BG_ItemDataRegistrySubsystem.h"
#include "BG_ItemDataRow.h"
#include "BG_WorldItemBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"


UBG_InventoryComponent::UBG_InventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	InventoryList.SetOwnerComponent(this);
	WorldItemClass = ABG_WorldItemBase::StaticClass();
}

void UBG_InventoryComponent::OnRegister()
{
	Super::OnRegister();
	InventoryList.SetOwnerComponent(this);
}

void UBG_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	InventoryList.SetOwnerComponent(this);

	const AActor* Owner = GetOwner();
	if (!ensureMsgf(IsValid(Owner), TEXT("%s: BeginPlay failed because owner was null."), *GetNameSafe(this)))
		return;

	if (!Owner->HasAuthority())
	{
		return;
	}

	if (!ensureMsgf(BaseMaxWeight >= 0.f, TEXT("%s: BeginPlay found invalid BaseMaxWeight %.2f. Resetting to 50."),
	                *GetNameSafe(this), BaseMaxWeight))
		BaseMaxWeight = 50.f;

	BackpackWeightBonus = FMath::Max(0.f, BackpackWeightBonus);
	RefreshMaxWeight();
	RecalculateCurrentWeight();
	BroadcastInventoryChanged();
	BroadcastWeightChanged();
}

void UBG_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UBG_InventoryComponent, InventoryList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBG_InventoryComponent, CurrentWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBG_InventoryComponent, MaxWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBG_InventoryComponent, BackpackWeightBonus, COND_OwnerOnly);
}

UBG_InventoryComponent* UBG_InventoryComponent::FindInventoryComponent(AActor* TargetActor)
{
	if (!ensureMsgf(IsValid(TargetActor),
	                TEXT("FindInventoryComponent failed because target actor was null or invalid.")))
		return nullptr;

	UBG_InventoryComponent* InventoryComponent = TargetActor->FindComponentByClass<UBG_InventoryComponent>();
	if (!ensureMsgf(InventoryComponent, TEXT("FindInventoryComponent failed because %s has no inventory component."),
	                *GetNameSafe(TargetActor)))
		return nullptr;

	return InventoryComponent;
}

bool UBG_InventoryComponent::TryAddItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity,
                                        int32& OutAddedQuantity)
{
	OutAddedQuantity = 0;

	if (!CanMutateInventoryState(TEXT("TryAddItem")))
		return false;

	int32 AcceptedQuantity = 0;
	if (!CanAddItem(ItemType, ItemTag, Quantity, AcceptedQuantity) || AcceptedQuantity <= 0)
		return false;

	const FBG_ItemDataRow* ItemRow = FindInventoryItemRow(ItemType, ItemTag, TEXT("TryAddItem"));
	if (!ItemRow)
		return false;

	OutAddedQuantity = InventoryList.AddQuantity(
		ItemType, ItemTag, AcceptedQuantity, ItemRow->MaxStackSize);
	if (!ensureMsgf(OutAddedQuantity > 0, TEXT("%s: TryAddItem failed because no quantity was added for %s %s."),
	                *GetNameSafe(this), *GetItemTypeName(ItemType), *ItemTag.ToString()))
		return false;

	RecalculateCurrentWeight();
	BroadcastInventoryChanged();
	BroadcastItemQuantityChanged(ItemType, ItemTag);
	BroadcastWeightChanged();
	return true;
}

bool UBG_InventoryComponent::TryRemoveItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity,
                                           int32& OutRemovedQuantity)
{
	OutRemovedQuantity = 0;

	if (!CanMutateInventoryState(TEXT("TryRemoveItem")))
		return false;

	if (!ensureMsgf(Quantity > 0, TEXT("%s: TryRemoveItem failed because quantity %d was not positive."),
	                *GetNameSafe(this), Quantity))
		return false;

	if (!FindInventoryItemRow(ItemType, ItemTag, TEXT("TryRemoveItem")))
		return false;

	const int32 CurrentQuantity = GetQuantity(ItemType, ItemTag);
	if (!ensureMsgf(CurrentQuantity >= Quantity,
	                TEXT("%s: TryRemoveItem failed because requested quantity %d exceeds owned quantity %d for %s %s."),
	                *GetNameSafe(this), Quantity, CurrentQuantity, *GetItemTypeName(ItemType), *ItemTag.ToString()))
		return false;

	OutRemovedQuantity = InventoryList.RemoveQuantity(ItemType, ItemTag, Quantity);
	ensureMsgf(OutRemovedQuantity == Quantity,
	           TEXT("%s: TryRemoveItem removed %d of %d for %s %s. Inventory data may be inconsistent."),
	           *GetNameSafe(this), OutRemovedQuantity, Quantity, *GetItemTypeName(ItemType), *ItemTag.ToString());

	RecalculateCurrentWeight();
	BroadcastInventoryChanged();
	BroadcastItemQuantityChanged(ItemType, ItemTag);
	BroadcastWeightChanged();
	return OutRemovedQuantity > 0;
}

void UBG_InventoryComponent::RequestDropItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
		if (!TryDropItem(ItemType, ItemTag, Quantity, FailReason))
		{
			NotifyInventoryFailure(FailReason, ItemType, ItemTag);
		}
		return;
	}

	Server_RequestDropItem(ItemType, ItemTag, Quantity);
}

bool UBG_InventoryComponent::TryDropItem(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (!CanMutateInventoryState(TEXT("TryDropItem")))
		return false;

	if (!ensureMsgf(Quantity > 0, TEXT("%s: TryDropItem failed because quantity %d was not positive."),
	                *GetNameSafe(this), Quantity))
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (!ensureMsgf(IsRegularInventoryItemType(ItemType),
	                TEXT("%s: TryDropItem failed because item type %s is not stored in regular inventory."),
	                *GetNameSafe(this), *GetItemTypeName(ItemType)))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!ensureMsgf(ItemTag.IsValid(), TEXT("%s: TryDropItem failed because ItemTag was invalid for item type %s."),
	                *GetNameSafe(this), *GetItemTypeName(ItemType)))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!FindInventoryItemRow(ItemType, ItemTag, TEXT("TryDropItem")))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	const int32 CurrentQuantity = GetQuantity(ItemType, ItemTag);
	if (!ensureMsgf(CurrentQuantity >= Quantity,
	                TEXT("%s: TryDropItem failed because requested quantity %d exceeds owned quantity %d for %s %s."),
	                *GetNameSafe(this), Quantity, CurrentQuantity, *GetItemTypeName(ItemType), *ItemTag.ToString()))
	{
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	int32 RemovedQuantity = 0;
	if (!ensureMsgf(TryRemoveItem(ItemType, ItemTag, Quantity, RemovedQuantity) && RemovedQuantity == Quantity,
	                TEXT("%s: TryDropItem failed because TryRemoveItem removed %d of %d for %s %s."),
	                *GetNameSafe(this), RemovedQuantity, Quantity, *GetItemTypeName(ItemType), *ItemTag.ToString()))
	{
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	ABG_WorldItemBase* SpawnedWorldItem = ABG_WorldItemBase::SpawnDroppedWorldItem(
		GetWorld(),
		BuildDropTransform(),
		WorldItemClass,
		ItemType,
		ItemTag,
		RemovedQuantity);
	if (!ensureMsgf(SpawnedWorldItem, TEXT("%s: TryDropItem failed because world item spawn failed for %s %s."),
	                *GetNameSafe(this), *GetItemTypeName(ItemType), *ItemTag.ToString()))
	{
		int32 RollbackAddedQuantity = 0;
		TryAddItem(ItemType, ItemTag, RemovedQuantity, RollbackAddedQuantity);
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_InventoryComponent::CanAddItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity,
                                        int32& OutAcceptedQuantity) const
{
	OutAcceptedQuantity = 0;

	if (!ensureMsgf(Quantity > 0, TEXT("%s: CanAddItem failed because quantity %d was not positive."),
	                *GetNameSafe(this), Quantity))
		return false;

	const FBG_ItemDataRow* ItemRow = FindInventoryItemRow(ItemType, ItemTag, TEXT("CanAddItem"));
	if (!ItemRow)
		return false;

	if (!ensureMsgf(ItemRow->bStackable, TEXT("%s: CanAddItem failed because item row %s is not stackable."),
	                *GetNameSafe(this), *ItemTag.ToString()))
		return false;

	if (!ensureMsgf(ItemRow->MaxStackSize > 0,
	                TEXT("%s: CanAddItem failed because item row %s has invalid MaxStackSize %d."),
	                *GetNameSafe(this), *ItemTag.ToString(), ItemRow->MaxStackSize))
		return false;

	if (!ensureMsgf(ItemRow->UnitWeight >= 0.f,
	                TEXT("%s: CanAddItem failed because item row %s has invalid UnitWeight %.2f."),
	                *GetNameSafe(this), *ItemTag.ToString(), ItemRow->UnitWeight))
		return false;

	OutAcceptedQuantity = CalculateWeightLimitedQuantity(*ItemRow, Quantity);
	if (OutAcceptedQuantity <= 0)
	{
		LOGW(TEXT("%s: CanAddItem rejected %s %s because inventory is overweight. CurrentWeight=%.2f, MaxWeight=%.2f."),
		     *GetNameSafe(this),
		     *GetItemTypeName(ItemType),
		     *ItemTag.ToString(),
		     CurrentWeight,
		     MaxWeight);
		return false;
	}

	return true;
}

void UBG_InventoryComponent::Server_RequestDropItem_Implementation(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity)
{
	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	if (!TryDropItem(ItemType, ItemTag, Quantity, FailReason))
	{
		NotifyInventoryFailure(FailReason, ItemType, ItemTag);
	}
}

int32 UBG_InventoryComponent::GetQuantity(EBG_ItemType ItemType, FGameplayTag ItemTag) const
{
	return InventoryList.GetQuantity(ItemType, ItemTag);
}

bool UBG_InventoryComponent::SetBackpackWeightBonus(float NewBackpackWeightBonus)
{
	if (!CanMutateInventoryState(TEXT("SetBackpackWeightBonus")))
		return false;

	if (!ensureMsgf(NewBackpackWeightBonus >= 0.f,
	                TEXT("%s: SetBackpackWeightBonus failed because NewBackpackWeightBonus %.2f was negative."),
	                *GetNameSafe(this), NewBackpackWeightBonus))
		return false;

	const float NewMaxWeight = BaseMaxWeight + NewBackpackWeightBonus;
	if (CurrentWeight > NewMaxWeight + KINDA_SMALL_NUMBER)
	{
		LOGW(TEXT("%s: SetBackpackWeightBonus rejected %.2f because CurrentWeight %.2f exceeds new MaxWeight %.2f."),
		     *GetNameSafe(this),
		     NewBackpackWeightBonus,
		     CurrentWeight,
		     NewMaxWeight);
		return false;
	}

	BackpackWeightBonus = NewBackpackWeightBonus;
	RefreshMaxWeight();
	BroadcastWeightChanged();
	return true;
}

TArray<FBG_InventoryEntry> UBG_InventoryComponent::GetInventoryEntries() const
{
	return InventoryList.GetEntries();
}

bool UBG_InventoryComponent::CanMutateInventoryState(const TCHAR* OperationName) const
{
	const AActor* Owner = GetOwner();
	if (!ensureMsgf(IsValid(Owner), TEXT("%s: %s failed because owner was null."), *GetNameSafe(this), OperationName))
		return false;

	if (!ensureMsgf(Owner->HasAuthority(), TEXT("%s: %s failed because owner %s had no authority."),
	                *GetNameSafe(this), OperationName, *GetNameSafe(Owner)))
		return false;

	return true;
}

bool UBG_InventoryComponent::IsRegularInventoryItemType(EBG_ItemType ItemType) const
{
	return ItemType == EBG_ItemType::Ammo
		|| ItemType == EBG_ItemType::Heal
		|| ItemType == EBG_ItemType::Boost
		|| ItemType == EBG_ItemType::Throwable;
}

UBG_ItemDataRegistrySubsystem* UBG_InventoryComponent::GetItemDataRegistrySubsystem(const TCHAR* OperationName) const
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

const FBG_ItemDataRow* UBG_InventoryComponent::FindInventoryItemRow(
	EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	const TCHAR* OperationName) const
{
	if (!ensureMsgf(IsRegularInventoryItemType(ItemType),
	                TEXT("%s: %s failed because item type %s is not stored in regular inventory."),
	                *GetNameSafe(this), OperationName, *GetItemTypeName(ItemType)))
		return nullptr;

	if (!ensureMsgf(ItemTag.IsValid(), TEXT("%s: %s failed because ItemTag was invalid for item type %s."),
	                *GetNameSafe(this), OperationName, *GetItemTypeName(ItemType)))
		return nullptr;

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	if (!RegistrySubsystem)
	{
		return nullptr;
	}

	return RegistrySubsystem->FindItemRow(ItemType, ItemTag);
}

int32 UBG_InventoryComponent::CalculateWeightLimitedQuantity(const FBG_ItemDataRow& ItemRow,
                                                             int32 RequestedQuantity) const
{
	if (ItemRow.UnitWeight <= SMALL_NUMBER)
		return RequestedQuantity;

	const float AvailableWeight = FMath::Max(0.f, MaxWeight - CurrentWeight);
	const int32 WeightLimitedQuantity = FMath::FloorToInt((AvailableWeight + KINDA_SMALL_NUMBER) / ItemRow.UnitWeight);
	return FMath::Clamp(WeightLimitedQuantity, 0, RequestedQuantity);
}

void UBG_InventoryComponent::RefreshMaxWeight()
{
	MaxWeight = FMath::Max(0.f, BaseMaxWeight + BackpackWeightBonus);
}

void UBG_InventoryComponent::RecalculateCurrentWeight()
{
	float NewCurrentWeight = 0.f;
	for (const FBG_InventoryEntry& Entry : InventoryList.GetEntries())
	{
		if (!ensureMsgf(Entry.IsValidEntry(),
		                TEXT(
			                "%s: RecalculateCurrentWeight found invalid inventory entry. ItemType=%s, ItemTag=%s, Quantity=%d."
		                ),
		                *GetNameSafe(this), *GetItemTypeName(Entry.ItemType), *Entry.ItemTag.ToString(),
		                Entry.Quantity))
			continue;

		const FBG_ItemDataRow* ItemRow = FindInventoryItemRow(Entry.ItemType, Entry.ItemTag,
		                                                      TEXT("RecalculateCurrentWeight"));
		if (!ItemRow)
			continue;

		NewCurrentWeight += ItemRow->UnitWeight * static_cast<float>(Entry.Quantity);
	}

	CurrentWeight = FMath::Max(0.f, NewCurrentWeight);
}

FTransform UBG_InventoryComponent::BuildDropTransform() const
{
	const AActor* Owner = GetOwner();
	if (!ensureMsgf(IsValid(Owner), TEXT("%s: BuildDropTransform failed because owner was null."), *GetNameSafe(this)))
		return FTransform::Identity;

	const FVector DropLocation = Owner->GetActorLocation()
		+ Owner->GetActorForwardVector() * 120.f
		+ FVector(0.f, 0.f, 30.f);
	const FRotator DropRotation(0.f, Owner->GetActorRotation().Yaw, 0.f);
	return FTransform(DropRotation, DropLocation);
}

void UBG_InventoryComponent::NotifyInventoryFailure(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag) const
{
	ABG_Character* OwnerCharacter = Cast<ABG_Character>(GetOwner());
	if (!ensureMsgf(OwnerCharacter,
	                TEXT("%s: NotifyInventoryFailure failed because owner was not ABG_Character. FailReason=%d"),
	                *GetNameSafe(this), static_cast<int32>(FailReason)))
		return;

	OwnerCharacter->Client_ReceiveInventoryFailure(FailReason, ItemType, ItemTag);
}

void UBG_InventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

void UBG_InventoryComponent::BroadcastWeightChanged()
{
	OnInventoryWeightChanged.Broadcast(CurrentWeight, MaxWeight);
}

void UBG_InventoryComponent::BroadcastItemQuantityChanged(EBG_ItemType ItemType, const FGameplayTag& ItemTag)
{
	OnInventoryItemQuantityChanged.Broadcast(ItemType, ItemTag, GetQuantity(ItemType, ItemTag));
}

void UBG_InventoryComponent::HandleInventoryListReplicated()
{
	BroadcastInventoryChanged();
}

void UBG_InventoryComponent::OnRep_CurrentWeight()
{
	BroadcastWeightChanged();
}

void UBG_InventoryComponent::OnRep_MaxWeight()
{
	BroadcastWeightChanged();
}

FString UBG_InventoryComponent::GetItemTypeName(EBG_ItemType ItemType)
{
	const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
	return ItemTypeEnum ? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(ItemType)) : TEXT("<Unknown>");
}
