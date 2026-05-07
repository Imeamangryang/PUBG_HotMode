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
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: BeginPlay failed because owner was null."), *GetNameSafe(this));
		return;
	}

	if (!Owner->HasAuthority())
	{
		return;
	}

	if (!(BaseMaxWeight >= 0.f))
	{
		LOGE(TEXT("%s: BeginPlay found invalid BaseMaxWeight %.2f. Resetting to 50."),
			*GetNameSafe(this), BaseMaxWeight);
		BaseMaxWeight = 50.f;
	}

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
	if (!IsValid(TargetActor))
	{
		LOGE(TEXT("FindInventoryComponent failed because target actor was null or invalid."));
		return nullptr;
	}

	UBG_InventoryComponent* InventoryComponent = TargetActor->FindComponentByClass<UBG_InventoryComponent>();
	if (!InventoryComponent)
	{
		LOGE(TEXT("FindInventoryComponent failed because %s has no inventory component."),
			*GetNameSafe(TargetActor));
		return nullptr;
	}

	return InventoryComponent;
}

bool UBG_InventoryComponent::Auth_AddItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity,
                                        int32& OutAddedQuantity)
{
	OutAddedQuantity = 0;

	if (!CanMutateInventoryState(TEXT("Auth_AddItem")))
		return false;

	int32 AcceptedQuantity = 0;
	if (!CanAddItem(ItemType, ItemTag, Quantity, AcceptedQuantity) || AcceptedQuantity <= 0)
		return false;

	const FBG_ItemDataRow* ItemRow = FindInventoryItemRow(ItemType, ItemTag, TEXT("Auth_AddItem"));
	if (!ItemRow)
		return false;

	OutAddedQuantity = InventoryList.AddQuantity(
		ItemType, ItemTag, AcceptedQuantity, ItemRow->MaxStackSize);
	if (!(OutAddedQuantity > 0))
	{
		LOGE(TEXT("%s: Auth_AddItem failed because no quantity was added for %s %s."),
			*GetNameSafe(this), *GetItemTypeName(ItemType), *ItemTag.ToString());
		return false;
	}

	RecalculateCurrentWeight();
	BroadcastInventoryChanged();
	BroadcastItemQuantityChanged(ItemType, ItemTag);
	BroadcastWeightChanged();
	return true;
}

bool UBG_InventoryComponent::Auth_RemoveItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity,
                                           int32& OutRemovedQuantity)
{
	OutRemovedQuantity = 0;

	if (!CanMutateInventoryState(TEXT("Auth_RemoveItem")))
		return false;

	if (!(Quantity > 0))
	{
		LOGE(TEXT("%s: Auth_RemoveItem failed because quantity %d was not positive."),
			*GetNameSafe(this), Quantity);
		return false;
	}

	if (!FindInventoryItemRow(ItemType, ItemTag, TEXT("Auth_RemoveItem")))
		return false;

	const int32 CurrentQuantity = GetQuantity(ItemType, ItemTag);
	if (!(CurrentQuantity >= Quantity))
	{
		LOGE(TEXT("%s: Auth_RemoveItem failed because requested quantity %d exceeds owned quantity %d for %s %s."),
			*GetNameSafe(this), Quantity, CurrentQuantity, *GetItemTypeName(ItemType), *ItemTag.ToString());
		return false;
	}

	OutRemovedQuantity = InventoryList.RemoveQuantity(ItemType, ItemTag, Quantity);
	if (!(OutRemovedQuantity == Quantity))
	{
		LOGE(TEXT("%s: Auth_RemoveItem removed %d of %d for %s %s. Inventory data may be inconsistent."),
			*GetNameSafe(this), OutRemovedQuantity, Quantity, *GetItemTypeName(ItemType), *ItemTag.ToString());
	}

	RecalculateCurrentWeight();
	BroadcastInventoryChanged();
	BroadcastItemQuantityChanged(ItemType, ItemTag);
	BroadcastWeightChanged();
	return OutRemovedQuantity > 0;
}

void UBG_InventoryComponent::DropItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
		if (!Auth_DropItem(ItemType, ItemTag, Quantity, FailReason))
		{
			NotifyInventoryFailure(FailReason, ItemType, ItemTag);
		}
		return;
	}

	Server_DropItem(ItemType, ItemTag, Quantity);
}

bool UBG_InventoryComponent::Auth_DropItem(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (!CanMutateInventoryState(TEXT("Auth_DropItem")))
		return false;

	if (!CanDropItem(ItemType, ItemTag, Quantity, OutFailReason))
		return false;

	int32 RemovedQuantity = 0;
	if (!(Auth_RemoveItem(ItemType, ItemTag, Quantity, RemovedQuantity) && RemovedQuantity == Quantity))
	{
		LOGE(TEXT("%s: Auth_DropItem failed because Auth_RemoveItem removed %d of %d for %s %s."),
			*GetNameSafe(this), RemovedQuantity, Quantity, *GetItemTypeName(ItemType), *ItemTag.ToString());
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
	if (!SpawnedWorldItem)
	{
		LOGE(TEXT("%s: Auth_DropItem failed because world item spawn failed for %s %s."),
			*GetNameSafe(this), *GetItemTypeName(ItemType), *ItemTag.ToString());
		int32 RollbackAddedQuantity = 0;
		Auth_AddItem(ItemType, ItemTag, RemovedQuantity, RollbackAddedQuantity);
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

	if (!(Quantity > 0))
	{
		LOGE(TEXT("%s: CanAddItem failed because quantity %d was not positive."),
			*GetNameSafe(this), Quantity);
		return false;
	}

	const FBG_ItemDataRow* ItemRow = FindInventoryItemRow(ItemType, ItemTag, TEXT("CanAddItem"));
	if (!ItemRow)
		return false;

	if (!ItemRow->bStackable)
	{
		LOGE(TEXT("%s: CanAddItem failed because item row %s is not stackable."),
			*GetNameSafe(this), *ItemTag.ToString());
		return false;
	}

	if (!(ItemRow->MaxStackSize > 0))
	{
		LOGE(TEXT("%s: CanAddItem failed because item row %s has invalid MaxStackSize %d."),
			*GetNameSafe(this), *ItemTag.ToString(), ItemRow->MaxStackSize);
		return false;
	}

	if (!(ItemRow->UnitWeight >= 0.f))
	{
		LOGE(TEXT("%s: CanAddItem failed because item row %s has invalid UnitWeight %.2f."),
			*GetNameSafe(this), *ItemTag.ToString(), ItemRow->UnitWeight);
		return false;
	}

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

void UBG_InventoryComponent::Server_DropItem_Implementation(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity)
{
	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	if (!Auth_DropItem(ItemType, ItemTag, Quantity, FailReason))
	{
		NotifyInventoryFailure(FailReason, ItemType, ItemTag);
	}
}

int32 UBG_InventoryComponent::GetQuantity(EBG_ItemType ItemType, FGameplayTag ItemTag) const
{
	return InventoryList.GetQuantity(ItemType, ItemTag);
}

bool UBG_InventoryComponent::Auth_SetBackpackBonus(float NewBackpackWeightBonus)
{
	if (!CanMutateInventoryState(TEXT("Auth_SetBackpackBonus")))
		return false;

	if (!(NewBackpackWeightBonus >= 0.f))
	{
		LOGE(TEXT("%s: Auth_SetBackpackBonus failed because NewBackpackWeightBonus %.2f was negative."),
			*GetNameSafe(this), NewBackpackWeightBonus);
		return false;
	}

	const float NewMaxWeight = BaseMaxWeight + NewBackpackWeightBonus;
	if (CurrentWeight > NewMaxWeight + KINDA_SMALL_NUMBER)
	{
		LOGW(TEXT("%s: Auth_SetBackpackBonus rejected %.2f because CurrentWeight %.2f exceeds new MaxWeight %.2f."),
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

bool UBG_InventoryComponent::IsRegularInventoryItemType(EBG_ItemType ItemType) const
{
	return ItemType == EBG_ItemType::Ammo
		|| ItemType == EBG_ItemType::Heal
		|| ItemType == EBG_ItemType::Boost
		|| ItemType == EBG_ItemType::Throwable;
}

bool UBG_InventoryComponent::CanDropItem(
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	int32 Quantity,
	EBGInventoryFailReason& OutFailReason) const
{
	if (Quantity <= 0)
	{
		LOGW(TEXT("%s: CanDropItem rejected quantity %d because it was not positive."),
		     *GetNameSafe(this), Quantity);
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (!IsRegularInventoryItemType(ItemType))
	{
		LOGW(TEXT("%s: CanDropItem rejected item type %s because it is not stored in regular inventory."),
		     *GetNameSafe(this), *GetItemTypeName(ItemType));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!ItemTag.IsValid())
	{
		LOGW(TEXT("%s: CanDropItem rejected invalid ItemTag for item type %s."),
		     *GetNameSafe(this), *GetItemTypeName(ItemType));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!FindInventoryItemRow(ItemType, ItemTag, TEXT("CanDropItem")))
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	const int32 CurrentQuantity = GetQuantity(ItemType, ItemTag);
	if (CurrentQuantity < Quantity)
	{
		LOGW(TEXT("%s: CanDropItem rejected quantity %d because only %d is owned for %s %s."),
		     *GetNameSafe(this), Quantity, CurrentQuantity, *GetItemTypeName(ItemType), *ItemTag.ToString());
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

UBG_ItemDataRegistrySubsystem* UBG_InventoryComponent::GetItemDataRegistrySubsystem(const TCHAR* OperationName) const
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
		LOGE(TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(this),
			OperationName);
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

const FBG_ItemDataRow* UBG_InventoryComponent::FindInventoryItemRow(
	EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	const TCHAR* OperationName) const
{
	if (!IsRegularInventoryItemType(ItemType))
	{
		LOGE(TEXT("%s: %s failed because item type %s is not stored in regular inventory."),
			*GetNameSafe(this), OperationName, *GetItemTypeName(ItemType));
		return nullptr;
	}

	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because ItemTag was invalid for item type %s."),
			*GetNameSafe(this), OperationName, *GetItemTypeName(ItemType));
		return nullptr;
	}

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
		if (!Entry.IsValidEntry())
		{
			LOGE(TEXT(
				"%s: RecalculateCurrentWeight found invalid inventory entry. ItemType=%s, ItemTag=%s, Quantity=%d."
				),
				*GetNameSafe(this), *GetItemTypeName(Entry.ItemType), *Entry.ItemTag.ToString(),
				Entry.Quantity);
			continue;
		}

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

void UBG_InventoryComponent::NotifyInventoryFailure(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag) const
{
	ABG_Character* OwnerCharacter = Cast<ABG_Character>(GetOwner());
	if (!OwnerCharacter)
	{
		LOGE(TEXT("%s: NotifyInventoryFailure failed because owner was not ABG_Character. FailReason=%d"),
			*GetNameSafe(this), static_cast<int32>(FailReason));
		return;
	}

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
