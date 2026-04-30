// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BG_InventoryTypes.h"
#include "BG_InventoryComponent.generated.h"

struct FBG_ItemDataRow;
class ABG_WorldItemBase;
class UBG_ItemDataRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGInventoryChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBGInventoryWeightChanged, float, CurrentWeight, float, MaxWeight);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBGInventoryItemQuantityChanged, EBG_ItemType, ItemType, FGameplayTag, ItemTag, int32, Quantity);

/// Character-owned regular inventory
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	// Replicated component defaults
	UBG_InventoryComponent();

protected:
	// Owner cache setup
	virtual void OnRegister() override;

public:
	// Initial weight normalization
	virtual void BeginPlay() override;

	// Owner-only replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public: // --- Component Lookup ---
	// Actor inventory lookup
	UFUNCTION(BlueprintCallable, Category="Inventory")
	static UBG_InventoryComponent* FindInventoryComponent(class AActor* TargetActor);

public: // --- Inventory Mutation ---
	// Server item add
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool TryAddItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity, int32& OutAddedQuantity);

	// Server item remove
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool TryRemoveItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity, int32& OutRemovedQuantity);

	// Client/server drop request
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RequestDropItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity);

	// Server item drop
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool TryDropItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity,
	                 EBGInventoryFailReason& OutFailReason);

	// Backpack capacity bonus update
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool SetBackpackWeightBonus(float NewBackpackWeightBonus);

public: // --- Inventory Query ---
	// Add validation and accepted quantity
	UFUNCTION(BlueprintPure, Category="Inventory")
	bool CanAddItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity, int32& OutAcceptedQuantity) const;

	// Owned quantity lookup
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetQuantity(EBG_ItemType ItemType, FGameplayTag ItemTag) const;

	// Inventory snapshot
	UFUNCTION(BlueprintPure, Category="Inventory")
	TArray<FBG_InventoryEntry> GetInventoryEntries() const;

public: // --- Weight Query ---
	// Current carried weight
	UFUNCTION(BlueprintPure, Category="Inventory")
	float GetCurrentWeight() const { return CurrentWeight; }

	// Current max weight
	UFUNCTION(BlueprintPure, Category="Inventory")
	float GetMaxWeight() const { return MaxWeight; }

	// Base max weight
	UFUNCTION(BlueprintPure, Category="Inventory")
	float GetBaseMaxWeight() const { return BaseMaxWeight; }

	// Backpack bonus weight
	UFUNCTION(BlueprintPure, Category="Inventory")
	float GetBackpackWeightBonus() const { return BackpackWeightBonus; }

public: // --- Events ---
	// Inventory list delegate
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnBGInventoryChanged OnInventoryChanged;

	// Weight delegate
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnBGInventoryWeightChanged OnInventoryWeightChanged;

	// Quantity delegate
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnBGInventoryItemQuantityChanged OnInventoryItemQuantityChanged;

private: // --- FastArray Access ---
	friend struct FBG_InventoryList;

	// FastArray replication handler
	void HandleInventoryListReplicated();

private: // --- Validation ---
	// Server mutation guard
	bool CanMutateInventoryState(const TCHAR* OperationName) const;

	UFUNCTION(Server, Reliable)
	void Server_RequestDropItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity);

	// Regular inventory type guard
	bool IsRegularInventoryItemType(EBG_ItemType ItemType) const;

private: // --- Item Data ---
	// Registry subsystem lookup
	UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(const TCHAR* OperationName) const;

	// Item row validation lookup
	const FBG_ItemDataRow* FindInventoryItemRow(EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	                                            const TCHAR* OperationName) const;

private: // --- Weight Calculation ---
	// Weight-limited accepted quantity
	int32 CalculateWeightLimitedQuantity(const FBG_ItemDataRow& ItemRow, int32 RequestedQuantity) const;

	// Max weight recalculation
	void RefreshMaxWeight();

	// Current weight recalculation
	void RecalculateCurrentWeight();

	// Drop spawn transform
	FTransform BuildDropTransform() const;

	// Owning client failure notification
	void NotifyInventoryFailure(EBGInventoryFailReason FailReason, EBG_ItemType ItemType,
	                            const FGameplayTag& ItemTag) const;

private: // --- Broadcast ---
	// Inventory delegate broadcast
	void BroadcastInventoryChanged();

	// Weight delegate broadcast
	void BroadcastWeightChanged();

	// Quantity delegate broadcast
	void BroadcastItemQuantityChanged(EBG_ItemType ItemType, const FGameplayTag& ItemTag);

private: // --- Data ---
	// Owner-only stack list
	UPROPERTY(Replicated)
	FBG_InventoryList InventoryList;

	// Generic world item actor class used by drop flows
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|World Item",
		meta=(AllowPrivateAccess="true"))
	TSubclassOf<ABG_WorldItemBase> WorldItemClass;

	// Owner-only current weight
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentWeight, Category="Inventory",
		meta=(AllowPrivateAccess="true"))
	float CurrentWeight = 0.f;

	UFUNCTION()
	void OnRep_CurrentWeight();

	// Owner-only max weight
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_MaxWeight, Category="Inventory",
		meta=(AllowPrivateAccess="true"))
	float MaxWeight = 50.f;

	UFUNCTION()
	void OnRep_MaxWeight();

	// Default carry capacity
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory",
		meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float BaseMaxWeight = 50.f;

	// Owner-only backpack capacity bonus
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Inventory",
		meta=(AllowPrivateAccess="true"))
	float BackpackWeightBonus = 0.f;

private: // --- Utility ---
	// Item type display name
	static FString GetItemTypeName(EBG_ItemType ItemType);
};
