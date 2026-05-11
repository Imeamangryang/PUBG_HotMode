// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BG_ItemTypes.h"
#include "BG_WorldItemBase.generated.h"

class ABG_Character;
class UBG_EquipmentComponent;
class UBG_InventoryComponent;
class UBG_ItemDataRegistrySubsystem;
class UStaticMeshComponent;
class UWorld;
enum class EBG_EquipmentSlot : uint8;
struct FBG_ArmorItemDataRow;
struct FBG_BackpackItemDataRow;
struct FBG_ItemDataRow;
struct FBG_WeaponItemDataRow;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGWorldItemStateChanged);

UCLASS()
class PUBG_HOTMODE_API ABG_WorldItemBase : public AActor
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	ABG_WorldItemBase();

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public: // --- Spawn ---
	// Server drop spawn helper using row class override with fallback class
	static ABG_WorldItemBase* SpawnDroppedWorldItem(
		UWorld* World,
		const FTransform& SpawnTransform,
		TSubclassOf<ABG_WorldItemBase> WorldItemClass,
		EBG_ItemType ItemType,
		FGameplayTag ItemTag,
		int32 Quantity,
		int32 LoadedAmmo = 0);

public: // --- World Item Mutation ---
	// Server initialization after spawn or editor-placed setup
	UFUNCTION(BlueprintCallable, Category="World Item")
	void InitializeWorldItem(EBG_ItemType NewItemType, FGameplayTag NewItemTag, int32 NewQuantity,
	                         int32 NewWeaponLoadedAmmo = 0);

	// Server pickup attempt routed to Inventory or Equipment
	UFUNCTION(BlueprintCallable, Category="World Item")
	bool TryPickup(ABG_Character* RequestingCharacter, int32 RequestedQuantity,
	               EBGInventoryFailReason& OutFailReason);

public: // --- World Item Query ---
	// Runtime item category
	UFUNCTION(BlueprintPure, Category="World Item")
	EBG_ItemType GetItemType() const { return ItemType; }

	// Runtime item tag
	UFUNCTION(BlueprintPure, Category="World Item")
	FGameplayTag GetItemTag() const { return ItemTag; }

	// Remaining stack or equipment quantity
	UFUNCTION(BlueprintPure, Category="World Item")
	int32 GetQuantity() const { return Quantity; }

	// Preserved weapon ammo for dropped weapons
	UFUNCTION(BlueprintPure, Category="World Item")
	int32 GetWeaponLoadedAmmo() const { return WeaponLoadedAmmo; }

	// Blueprint-authored world mesh component
	UFUNCTION(BlueprintPure, Category="World Item")
	UStaticMeshComponent* GetStaticMeshComponent() const { return WorldMeshComponent; }

public: // --- Events ---
	// Called on BeginPlay, server initialization, and item state replication
	UFUNCTION(BlueprintImplementableEvent, Category="World Item")
	void ReceiveWorldItemStateChanged();

	UPROPERTY(BlueprintAssignable, Category="World Item")
	FOnBGWorldItemStateChanged OnWorldItemStateChanged;

private: // --- Pickup Validation ---
	// Validate server authority, range, identity, registry row, and requested quantity
	bool ValidatePickupRequest(
		ABG_Character* RequestingCharacter,
		int32 RequestedQuantity,
		int32& OutPickupQuantity,
		EBGInventoryFailReason& OutFailReason) const;

private: // --- Pickup Routing ---
	// Add stack item quantity to requesting character inventory
	bool TryPickupStackItem(
		ABG_Character* RequestingCharacter,
		int32 PickupQuantity,
		EBGInventoryFailReason& OutFailReason);

	// Equip weapon and drop replaced weapon when needed
	bool TryPickupWeapon(
		ABG_Character* RequestingCharacter,
		int32 PickupQuantity,
		EBGInventoryFailReason& OutFailReason);

	// Equip armor and drop replaced armor when needed
	bool TryPickupArmor(
		ABG_Character* RequestingCharacter,
		int32 PickupQuantity,
		EBGInventoryFailReason& OutFailReason);

	// Equip backpack and drop replaced backpack when needed
	bool TryPickupBackpack(
		ABG_Character* RequestingCharacter,
		int32 PickupQuantity,
		EBGInventoryFailReason& OutFailReason);

private: // --- State Mutation ---
	// Spawn equipment replaced by pickup at this item's location
	bool TrySpawnReplacementItem(EBG_ItemType ReplacedItemType, const FGameplayTag& ReplacedItemTag,
	                             int32 ReplacedQuantity, int32 ReplacedLoadedAmmo) const;

	// Consume picked up quantity or destroy empty world item
	void ConsumePickedUpQuantity(int32 PickedUpQuantity);

private: // --- Validation Helpers ---
	// True for item types stored in regular inventory stacks
	bool IsStackInventoryItemType() const;

	// Check character distance against PickupInteractionRange
	bool IsInPickupRange(const ABG_Character* RequestingCharacter) const;

private: // --- Item Data ---
	// Registry subsystem lookup
	UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(const TCHAR* OperationName) const;

	// Base item row lookup for current item identity
	const FBG_ItemDataRow* FindItemRow(const TCHAR* OperationName) const;

	// Weapon row lookup for current item identity
	const FBG_WeaponItemDataRow* FindWeaponItemRow(const TCHAR* OperationName) const;

	// Armor row lookup for current item identity
	const FBG_ArmorItemDataRow* FindArmorItemRow(const TCHAR* OperationName) const;

	// Backpack row lookup for current item identity
	const FBG_BackpackItemDataRow* FindBackpackItemRow(const TCHAR* OperationName) const;

private: // --- Slot Selection ---
	// Choose a compatible weapon slot for pickup
	static EBG_EquipmentSlot SelectWeaponSlotForPickup(
		const UBG_EquipmentComponent& EquipmentComponent,
		const FBG_WeaponItemDataRow& WeaponRow);

	// Choose a compatible armor slot for pickup
	static EBG_EquipmentSlot SelectArmorSlotForPickup(const FBG_ArmorItemDataRow& ArmorRow);

private: // --- Spawn Class Resolution ---
	// Resolve item row world item class or component fallback class
	static UClass* ResolveWorldItemClass(
		UWorld* World,
		TSubclassOf<ABG_WorldItemBase> FallbackWorldItemClass,
		EBG_ItemType ItemType,
		const FGameplayTag& ItemTag);

	// Static item row lookup used before an actor exists
	static const FBG_ItemDataRow* FindWorldItemRow(
		UWorld* World,
		EBG_ItemType ItemType,
		const FGameplayTag& ItemTag,
		const TCHAR* OperationName);

private: // --- Utility ---
	// Notify C++ and Blueprint display observers
	void BroadcastWorldItemStateChanged();

	// Item type display name for logs
	static FString GetItemTypeName(EBG_ItemType InItemType);

private: // --- Components ---
	// Root mesh component configured by item-specific Blueprint subclasses
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Item", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> WorldMeshComponent;

private: // --- Replicated State ---
	// Runtime item category used for registry routing
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, EditAnywhere, BlueprintReadOnly, Category="World Item|State",
		meta=(AllowPrivateAccess="true"))
	EBG_ItemType ItemType = EBG_ItemType::None;

	// Runtime item tag used for registry row lookup
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, EditAnywhere, BlueprintReadOnly, Category="World Item|State",
		meta=(AllowPrivateAccess="true", Categories="Item"))
	FGameplayTag ItemTag;

	// Remaining stack quantity or 1 for equipment items
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, EditAnywhere, BlueprintReadOnly, Category="World Item|State",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 Quantity = 1;

	// Preserved loaded ammo for dropped weapon items
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, EditAnywhere, BlueprintReadOnly, Category="World Item|State",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 WeaponLoadedAmmo = 0;

	// Replicated item state notification
	UFUNCTION()
	void OnRep_ItemState();

private: // --- Tuning ---
	// Maximum server-authorized pickup distance
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World Item",
		meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float PickupInteractionRange = 250.f;
};
