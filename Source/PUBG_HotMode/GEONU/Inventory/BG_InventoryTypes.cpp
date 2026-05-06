// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_InventoryTypes.h"

#include "BG_InventoryComponent.h"

// --- Replication ---

bool FBG_InventoryList::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FBG_InventoryEntry, FBG_InventoryList>(
		Entries, DeltaParms, *this);
}

void FBG_InventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	NotifyOwnerInventoryChanged();
}

// --- Inventory List Interface ---

int32 FBG_InventoryList::GetQuantity(EBG_ItemType ItemType, const FGameplayTag& ItemTag) const
{
	int32 TotalQuantity = 0;
	for (const FBG_InventoryEntry& Entry : Entries)
	{
		if (Entry.Matches(ItemType, ItemTag))
		{
			TotalQuantity += Entry.Quantity;
		}
	}

	return TotalQuantity;
}

FBG_InventoryEntry* FBG_InventoryList::FindEntry(EBG_ItemType ItemType, const FGameplayTag& ItemTag)
{
	return Entries.FindByPredicate(
		[ItemType, &ItemTag](const FBG_InventoryEntry& Entry)
		{
			return Entry.Matches(ItemType, ItemTag);
		});
}

const FBG_InventoryEntry* FBG_InventoryList::FindEntry(EBG_ItemType ItemType, const FGameplayTag& ItemTag) const
{
	return Entries.FindByPredicate(
		[ItemType, &ItemTag](const FBG_InventoryEntry& Entry)
		{
			return Entry.Matches(ItemType, ItemTag);
		});
}

int32 FBG_InventoryList::AddQuantity(
	EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	int32 Quantity, int32 MaxStackSize)
{
	// Guard: no stack mutation for invalid request data
	if (Quantity <= 0 || MaxStackSize <= 0)
		return 0;

	// Existing stacks first: preserve stack order and fill partial stacks before creating new entries
	int32 RemainingQuantity = Quantity;
	for (FBG_InventoryEntry& Entry : Entries)
	{
		if (RemainingQuantity <= 0)
			break;

		// Different item or already-full stack
		if (!Entry.Matches(ItemType, ItemTag) || Entry.Quantity >= MaxStackSize)
			continue;

		// Stack fill: merge as much as this stack can accept
		const int32 StackCapacity = MaxStackSize - Entry.Quantity;
		const int32 QuantityToStack = FMath::Min(StackCapacity, RemainingQuantity);
		Entry.Quantity += QuantityToStack;
		RemainingQuantity -= QuantityToStack;

		// FastArray dirty mark: keep replication bookkeeping inside InventoryList
		MarkItemDirty(Entry);
	}

	// New stacks: split remaining quantity by MaxStackSize
	while (RemainingQuantity > 0)
	{
		const int32 QuantityForNewStack = FMath::Min(MaxStackSize, RemainingQuantity);
		AddEntry(ItemType, ItemTag, QuantityForNewStack);
		RemainingQuantity -= QuantityForNewStack;
	}

	// Actual added quantity
	return Quantity - RemainingQuantity;
}

int32 FBG_InventoryList::RemoveQuantity(EBG_ItemType ItemType, const FGameplayTag& ItemTag, int32 Quantity)
{
	if (Quantity <= 0)
		return 0;

	int32 RemainingQuantity = Quantity;
	for (int32 EntryIndex = Entries.Num() - 1; EntryIndex >= 0 && RemainingQuantity > 0; --EntryIndex)
	{
		FBG_InventoryEntry& Entry = Entries[EntryIndex];
		if (!Entry.Matches(ItemType, ItemTag))
			continue;

		const int32 QuantityToRemove = FMath::Min(Entry.Quantity, RemainingQuantity);
		Entry.Quantity -= QuantityToRemove;
		RemainingQuantity -= QuantityToRemove;

		if (Entry.Quantity <= 0)
		{
			RemoveEntryAt(EntryIndex);
		}
		else
		{
			MarkItemDirty(Entry);
		}
	}

	return Quantity - RemainingQuantity;
}

FBG_InventoryEntry& FBG_InventoryList::AddEntry(EBG_ItemType ItemType, const FGameplayTag& ItemTag, int32 Quantity)
{
	FBG_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.ItemType = ItemType;
	NewEntry.ItemTag = ItemTag;
	NewEntry.Quantity = Quantity;
	MarkItemDirty(NewEntry);
	return NewEntry;
}

void FBG_InventoryList::RemoveEntryAt(int32 EntryIndex)
{
	if (!Entries.IsValidIndex(EntryIndex))
		return;

	Entries.RemoveAt(EntryIndex);
	MarkArrayDirty();
}

// --- Owner Access ---

void FBG_InventoryList::NotifyOwnerInventoryChanged() const
{
	if (OwnerComponent)
	{
		OwnerComponent->HandleInventoryListReplicated();
	}
}
