// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_ItemUseComponent.h"
#include "Inventory/BG_ItemTypes.h"
#include "BG_InventoryViewModel.generated.h"

class ABG_Character;
class ABG_WorldItemBase;
class UBG_EquipmentComponent;
class UBG_InventoryComponent;
class UBG_ItemUseComponent;
class UBG_ItemDataRegistrySubsystem;
struct FBG_ItemDataRow;

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGInventoryItemRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0"))
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float UnitWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float TotalWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bStackable = false;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="1"))
	int32 MaxStackSize = 1;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bHasDisplayRow = false;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGEquipmentSlotRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBG_EquipmentSlot Slot = EBG_EquipmentSlot::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0"))
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0"))
	int32 LoadedAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float Durability = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bHasDisplayRow = false;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGNearbyWorldItemRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	TObjectPtr<ABG_WorldItemBase> WorldItem = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0"))
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float Distance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bHasDisplayRow = false;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGInventoryFailureRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bHasDisplayRow = false;
};

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBGItemUseRenderData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bIsUsingItem = false;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Progress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float ElapsedTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float RemainingTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(ClampMin="0.0"))
	float UseDuration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI")
	bool bHasDisplayRow = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGInventoryViewModelChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBGInventoryViewModelWeightChanged, float, CurrentWeight, float, MaxWeight);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGInventoryFailureReceived, const FBGInventoryFailureRenderData&, FailureData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGItemUseRenderDataChanged, const FBGItemUseRenderData&, ItemUseData);

UCLASS(ClassGroup="UI", meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_InventoryViewModel : public UActorComponent
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	UBG_InventoryViewModel();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public: // --- Possession Binding ---
	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void NotifyPossessedCharacterReady(ABG_Character* InCharacter);

	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void NotifyPossessedCharacterCleared();

public: // --- Commands ---
	UFUNCTION(BlueprintCallable, Category="Inventory UI|Command")
	bool PickupWorldItem(ABG_WorldItemBase* WorldItem, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Command")
	bool PickupNearbyWorldItem(int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Command")
	bool DropInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Command")
	bool DropEquipment(EBG_EquipmentSlot EquipmentSlot);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Command")
	bool UseInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag);

	UFUNCTION(BlueprintCallable, Category="Inventory UI|Command")
	bool CancelItemUse();

public: // --- Failure Feedback ---
	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void NotifyInventoryFailure(EBGInventoryFailReason FailReason, EBG_ItemType ItemType, FGameplayTag ItemTag);

public: // --- Refresh ---
	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void RefreshAllRenderData();

	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void ForceBroadcastAll();

public: // --- Render Data Getters ---
	UFUNCTION(BlueprintPure, Category="Inventory UI")
	TArray<FBGInventoryItemRenderData> GetInventoryItems() const { return InventoryItems; }

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	TArray<FBGEquipmentSlotRenderData> GetEquipmentSlots() const { return EquipmentSlots; }

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	TArray<FBGNearbyWorldItemRenderData> GetNearbyWorldItems() const { return NearbyWorldItems; }

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	float GetCurrentWeight() const { return CurrentWeight; }

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	float GetMaxWeight() const { return MaxWeight; }

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	FBGInventoryFailureRenderData GetLastFailure() const { return LastFailure; }

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	FBGItemUseRenderData GetItemUseData() const { return ItemUseData; }

public: // --- Events ---
	UPROPERTY(BlueprintAssignable, Category="Inventory UI")
	FOnBGInventoryViewModelChanged OnInventoryItemsChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory UI")
	FOnBGInventoryViewModelWeightChanged OnInventoryWeightChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory UI")
	FOnBGInventoryViewModelChanged OnEquipmentSlotsChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory UI")
	FOnBGInventoryViewModelChanged OnNearbyWorldItemsChanged;

	UPROPERTY(BlueprintAssignable, Category="Inventory UI")
	FOnBGInventoryFailureReceived OnInventoryFailureReceived;

	UPROPERTY(BlueprintAssignable, Category="Inventory UI")
	FOnBGItemUseRenderDataChanged OnItemUseChanged;

private: // --- Binding ---
	void BindToCharacter(ABG_Character* InCharacter, UBG_InventoryComponent* InInventoryComponent,
	                     UBG_EquipmentComponent* InEquipmentComponent, UBG_ItemUseComponent* InItemUseComponent);
	void UnbindFromCharacter();

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleInventoryWeightChanged(float NewCurrentWeight, float NewMaxWeight);

	UFUNCTION()
	void HandleEquipmentChanged();

	UFUNCTION()
	void HandleNearbyWorldItemsChanged();

	UFUNCTION()
	void HandleNearbyWorldItemStateChanged();

	UFUNCTION()
	void HandleItemUseStateChanged(const FBG_ItemUseRepState& ItemUseState);

	UFUNCTION()
	void HandleItemUseProgressChanged(float Progress, float ElapsedTime, float RemainingTime);

private: // --- Render Data ---
	void RefreshInventoryRenderData();
	void RefreshEquipmentRenderData();
	void RefreshNearbyWorldItemRenderData();
	void RefreshItemUseRenderData();
	void RebindNearbyWorldItemStateDelegates();
	void UnbindNearbyWorldItemStateDelegates();

	FBGInventoryItemRenderData BuildInventoryItemRenderData(
		EBG_ItemType ItemType, const FGameplayTag& ItemTag, int32 Quantity) const;
	FBGEquipmentSlotRenderData BuildEquipmentSlotRenderData(EBG_EquipmentSlot Slot) const;
	FBGNearbyWorldItemRenderData BuildNearbyWorldItemRenderData(ABG_WorldItemBase* WorldItem) const;
	FBGInventoryFailureRenderData BuildFailureRenderData(
		EBGInventoryFailReason FailReason, EBG_ItemType ItemType, const FGameplayTag& ItemTag) const;
	FBGItemUseRenderData BuildItemUseRenderData() const;

	void ApplyItemDisplayData(const FBG_ItemDataRow& ItemRow, FText& OutDisplayName, FText& OutDescription,
	                          TSoftObjectPtr<UTexture2D>& OutIcon) const;

private: // --- Item Data ---
	const FBG_ItemDataRow* FindItemRow(EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	                                   const TCHAR* OperationName) const;
	UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(const TCHAR* OperationName) const;

private: // --- Component Access ---
	ABG_Character* GetBoundCharacter(const TCHAR* OperationName) const;
	UBG_InventoryComponent* GetBoundInventoryComponent(const TCHAR* OperationName) const;
	UBG_EquipmentComponent* GetBoundEquipmentComponent(const TCHAR* OperationName) const;
	UBG_ItemUseComponent* GetBoundItemUseComponent(const TCHAR* OperationName) const;
	ABG_WorldItemBase* GetBestNearbyWorldItem() const;

private: // --- Utility ---
	static EBG_ItemType GetItemTypeForEquipmentSlot(EBG_EquipmentSlot Slot);
	static FText GetFallbackItemName(const FGameplayTag& ItemTag);

private: // --- Cached Bindings ---
	UPROPERTY(Transient)
	TWeakObjectPtr<ABG_Character> BoundCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBG_InventoryComponent> BoundInventoryComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBG_EquipmentComponent> BoundEquipmentComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBG_ItemUseComponent> BoundItemUseComponent;

	TArray<TWeakObjectPtr<ABG_WorldItemBase>> BoundNearbyWorldItemActors;

private: // --- Cached Render Data ---
	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	TArray<FBGInventoryItemRenderData> InventoryItems;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	TArray<FBGEquipmentSlotRenderData> EquipmentSlots;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	TArray<FBGNearbyWorldItemRenderData> NearbyWorldItems;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	FBGInventoryFailureRenderData LastFailure;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	FBGItemUseRenderData ItemUseData;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	float CurrentWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(AllowPrivateAccess="true"))
	float MaxWeight = 0.f;
};
