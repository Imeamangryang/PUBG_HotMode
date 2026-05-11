// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "BG_ItemTypes.h"
#include "BG_InventoryTypes.generated.h"

class UBG_InventoryComponent;

/// Inventory stack entry
USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_InventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public: // --- Data ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	/// Stack quantity
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0"))
	int32 Quantity = 0;

public: // --- Helpers ---
	/// Type and tag match
	bool Matches(EBG_ItemType InItemType, const FGameplayTag& InItemTag) const
	{
		return ItemType == InItemType && ItemTag == InItemTag;
	}

	/// Check non-empty entry
	bool IsValidEntry() const
	{
		return ItemType != EBG_ItemType::None && ItemTag.IsValid() && Quantity > 0;
	}
};

/// Owner-only replicated inventory list
USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_InventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

protected: // --- FastArray Data ---
	/// Replicated stack entries
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	TArray<FBG_InventoryEntry> Entries;

public: // --- Replication ---
	/// FastArray delta serialization
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	/// Replication receive notification
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

public: // --- Inventory List Interface ---
	const TArray<FBG_InventoryEntry>& GetEntries() const
	{
		return Entries;
	}

	/// Total item quantity
	int32 GetQuantity(EBG_ItemType ItemType, const FGameplayTag& ItemTag) const;

	FBG_InventoryEntry* FindEntry(EBG_ItemType ItemType, const FGameplayTag& ItemTag);
	const FBG_InventoryEntry* FindEntry(EBG_ItemType ItemType, const FGameplayTag& ItemTag) const;

	/// Stack-first quantity add
	int32 AddQuantity(EBG_ItemType ItemType, const FGameplayTag& ItemTag, int32 Quantity, int32 MaxStackSize);

	/// Stack quantity removal
	int32 RemoveQuantity(EBG_ItemType ItemType, const FGameplayTag& ItemTag, int32 Quantity);

private:
	FBG_InventoryEntry& AddEntry(EBG_ItemType ItemType, const FGameplayTag& ItemTag, int32 Quantity);
	void RemoveEntryAt(int32 EntryIndex);

public: // --- Owner Access ---
	void SetOwnerComponent(UBG_InventoryComponent* InOwnerComponent)
	{
		OwnerComponent = InOwnerComponent;
	}

private:
	UBG_InventoryComponent* OwnerComponent = nullptr;

	void NotifyOwnerInventoryChanged() const;
};

// FastArray serializer opt-in: Enable NetDeltaSerialize for FBG_InventoryList
template <>
struct TStructOpsTypeTraits<FBG_InventoryList> : public TStructOpsTypeTraitsBase2<FBG_InventoryList>
{
	// Net delta serializer flag
	enum { WithNetDeltaSerializer = true };
};
