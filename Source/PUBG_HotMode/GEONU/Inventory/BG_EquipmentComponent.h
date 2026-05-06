// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BG_ItemTypes.h"
#include "BG_EquipmentComponent.generated.h"

class ABG_Character;
class ABG_EquippedWeaponBase;
class ABG_WorldItemBase;
class UBG_InventoryComponent;
class UBG_ItemDataRegistrySubsystem;
class USceneComponent;
struct FBG_ArmorItemDataRow;
struct FBG_BackpackItemDataRow;
struct FBG_ThrowableItemDataRow;
struct FBG_WeaponItemDataRow;

// --- Equipment definitions ---

/// Fixed equipment slot
UENUM(BlueprintType)
enum class EBG_EquipmentSlot : uint8
{
	None UMETA(DisplayName="None"),
	PrimaryA UMETA(DisplayName="Primary A"),
	PrimaryB UMETA(DisplayName="Primary B"),
	Pistol UMETA(DisplayName="Pistol"),
	Melee UMETA(DisplayName="Melee"),
	Throwable UMETA(DisplayName="Throwable"),
	Helmet UMETA(DisplayName="Helmet"),
	Vest UMETA(DisplayName="Vest"),
	Backpack UMETA(DisplayName="Backpack")
};

/// Public equipment state for remote visuals
USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_EquipmentPublicState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(Categories="Item.Weapon"))
	FGameplayTag PrimaryAWeaponTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(Categories="Item.Weapon"))
	FGameplayTag PrimaryBWeaponTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(Categories="Item.Weapon"))
	FGameplayTag PistolWeaponTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(Categories="Item.Weapon"))
	FGameplayTag MeleeWeaponTag;

	/// Selected throwable item tag
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Throwable",
		meta=(Categories="Item.Throwable"))
	FGameplayTag ThrowableItemTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Armor",
		meta=(Categories="Item.Equipment.Helmet"))
	FGameplayTag HelmetItemTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Armor",
		meta=(Categories="Item.Equipment.Vest"))
	FGameplayTag VestItemTag;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Backpack",
		meta=(Categories="Item.Equipment.Backpack"))
	FGameplayTag BackpackItemTag;

	/// Active weapon slot for legacy character adapter
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon")
	EBG_EquipmentSlot ActiveWeaponSlot = EBG_EquipmentSlot::None;
};

/// Owner-only equipment detail state
USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_EquipmentOwnerState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(ClampMin="0"))
	int32 PrimaryALoadedAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(ClampMin="0"))
	int32 PrimaryBLoadedAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(ClampMin="0"))
	int32 PistolLoadedAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon", meta=(ClampMin="0"))
	int32 MeleeLoadedAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Armor", meta=(ClampMin="0.0"))
	float HelmetDurability = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Armor", meta=(ClampMin="0.0"))
	float VestDurability = 0.f;
};

/// Replicated equipped weapon actor references for owner and remote visuals
USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_EquippedWeaponActorState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon")
	TObjectPtr<ABG_EquippedWeaponBase> PrimaryAWeaponActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon")
	TObjectPtr<ABG_EquippedWeaponBase> PrimaryBWeaponActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon")
	TObjectPtr<ABG_EquippedWeaponBase> PistolWeaponActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment|Weapon")
	TObjectPtr<ABG_EquippedWeaponBase> MeleeWeaponActor = nullptr;
};

// --- Component declaration ---

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGEquipmentChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGActiveWeaponSlotChanged, EBG_EquipmentSlot, ActiveWeaponSlot);

/// Character-owned equipment and legacy weapon adapter
UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_EquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	/// Replicated component defaults
	UBG_EquipmentComponent();

protected:
	/// Owner component cache
	virtual void OnRegister() override;

public:
	/// Inventory delegate binding
	virtual void BeginPlay() override;

	/// Public and owner-only replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public: // --- Component Lookup ---
	/// Actor equipment lookup
	UFUNCTION(BlueprintCallable, Category="Equipment")
	static UBG_EquipmentComponent* FindEquipmentComponent(class AActor* TargetActor);

public: // --- Equipment Mutation ---
	UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
	bool TryEquipWeapon(EBG_EquipmentSlot WeaponSlot, FGameplayTag WeaponItemTag, int32 LoadedAmmo,
	                    FGameplayTag& OutReplacedWeaponItemTag, int32& OutReplacedLoadedAmmo);

	UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
	bool TryUnequipWeapon(EBG_EquipmentSlot WeaponSlot,
	                      FGameplayTag& OutRemovedWeaponItemTag, int32& OutRemovedLoadedAmmo);

	UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
	bool TryActivateWeapon(EBG_EquipmentSlot WeaponSlot);

	UFUNCTION(BlueprintCallable, Category="Equipment|Weapon")
	bool TryLoadAmmo(EBG_EquipmentSlot WeaponSlot, int32 LoadedAmmo);

	UFUNCTION(BlueprintCallable, Category="Equipment|Armor")
	bool TryEquipArmor(EBG_EquipmentSlot ArmorSlot, FGameplayTag ArmorItemTag,
	                   FGameplayTag& OutReplacedArmorItemTag, float& OutReplacedArmorDurability);

	UFUNCTION(BlueprintCallable, Category="Equipment|Armor")
	bool TryUnequipArmor(EBG_EquipmentSlot ArmorSlot, FGameplayTag& OutRemovedArmorItemTag,
	                     float& OutRemovedArmorDurability);

	UFUNCTION(BlueprintCallable, Category="Equipment|Backpack")
	bool TryEquipBackpack(FGameplayTag BackpackItemTag, FGameplayTag& OutReplacedBackpackItemTag);

	UFUNCTION(BlueprintCallable, Category="Equipment|Backpack")
	bool TryUnequipBackpack(FGameplayTag& OutRemovedBackpackItemTag);

	UFUNCTION(BlueprintCallable, Category="Equipment|Throwable")
	bool TrySetThrowable(FGameplayTag ThrowableItemTag);

	UFUNCTION(BlueprintCallable, Category="Equipment|Throwable")
	bool TryClearThrowable();

	UFUNCTION(BlueprintCallable, Category="Equipment")
	void RequestDropEquipment(EBG_EquipmentSlot EquipmentSlot);

	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool TryDropEquipment(EBG_EquipmentSlot EquipmentSlot, EBGInventoryFailReason& OutFailReason);

public: // --- Equipment Query ---
	UFUNCTION(BlueprintPure, Category="Equipment")
	FBG_EquipmentPublicState GetPublicEquipmentState() const { return PublicState; }

	UFUNCTION(BlueprintPure, Category="Equipment")
	FBG_EquipmentOwnerState GetOwnerEquipmentState() const { return OwnerState; }

	UFUNCTION(BlueprintPure, Category="Equipment")
	FGameplayTag GetEquippedItemTag(EBG_EquipmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	bool HasEquippedItem(EBG_EquipmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category="Equipment|Weapon")
	EBG_EquipmentSlot GetActiveWeaponSlot() const { return PublicState.ActiveWeaponSlot; }

	UFUNCTION(BlueprintPure, Category="Equipment|Weapon")
	FGameplayTag GetActiveWeaponItemTag() const;

	UFUNCTION(BlueprintPure, Category="Equipment|Weapon")
	EBG_EquipmentSlot GetSlotForEquippedWeaponTag(FGameplayTag WeaponItemTag) const;

	UFUNCTION(BlueprintPure, Category="Equipment|Weapon")
	int32 GetLoadedAmmo(EBG_EquipmentSlot WeaponSlot) const;

	UFUNCTION(BlueprintPure, Category="Equipment|Weapon")
	ABG_EquippedWeaponBase* GetEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot) const;

	UFUNCTION(BlueprintPure, Category="Equipment|Weapon")
	ABG_EquippedWeaponBase* GetActiveEquippedWeaponActor() const;

	UFUNCTION(BlueprintPure, Category="Equipment|Armor")
	float GetArmorDurability(EBG_EquipmentSlot ArmorSlot) const;

public: // --- Events ---
	/// Equipment state delegate
	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnBGEquipmentChanged OnEquipmentChanged;

	/// Active weapon slot delegate
	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnBGActiveWeaponSlotChanged OnActiveWeaponSlotChanged;

private: // --- Inventory Hooks ---
	/// Throwable empty-slot cleanup
	UFUNCTION()
	void HandleInventoryItemQuantityChanged(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity);

private: // --- Owner Cache ---
	/// Character and inventory lookup
	void CacheOwnerComponents();

	/// Inventory component guard
	UBG_InventoryComponent* GetInventoryComponentForEquipment(const TCHAR* OperationName) const;

private: // --- Validation ---
	/// Server mutation guard
	bool CanMutateEquipmentState(const TCHAR* OperationName) const;

	UFUNCTION(Server, Reliable)
	void Server_RequestDropEquipment(EBG_EquipmentSlot EquipmentSlot);

	bool IsWeaponSlot(EBG_EquipmentSlot Slot) const;
	bool IsArmorSlot(EBG_EquipmentSlot Slot) const;

	bool IsWeaponSlotCompatible(EBG_EquipmentSlot Slot, const FBG_WeaponItemDataRow& WeaponRow) const;
	bool IsArmorSlotCompatible(EBG_EquipmentSlot Slot, const FBG_ArmorItemDataRow& ArmorRow) const;

private: // --- Item Data ---
	UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(const TCHAR* OperationName) const;
	const FBG_WeaponItemDataRow* FindWeaponItemRow(const FGameplayTag& ItemTag, const TCHAR* OperationName) const;
	const FBG_ArmorItemDataRow* FindArmorItemRow(const FGameplayTag& ItemTag, const TCHAR* OperationName) const;
	const FBG_BackpackItemDataRow* FindBackpackItemRow(const FGameplayTag& ItemTag, const TCHAR* OperationName) const;
	const FBG_ThrowableItemDataRow* FindThrowableItemRow(const FGameplayTag& ItemTag, const TCHAR* OperationName) const;

private: // --- State Mutation ---
	void SetEquippedItemTag(EBG_EquipmentSlot Slot, const FGameplayTag& ItemTag);
	void SetLoadedAmmo(EBG_EquipmentSlot WeaponSlot, int32 LoadedAmmo);
	void SetArmorDurability(EBG_EquipmentSlot ArmorSlot, float Durability);
	void SetEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot, ABG_EquippedWeaponBase* WeaponActor);

	/// Active slot validity normalization
	void NormalizeActiveWeaponSlot();

	/// Net update request
	void ForceEquipmentNetUpdate() const;

	bool SpawnAndAttachEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot, const FBG_WeaponItemDataRow& WeaponRow,
	                                       ABG_EquippedWeaponBase*& OutWeaponActor);

	UClass* ResolveEquippedWeaponClass(const FBG_WeaponItemDataRow& WeaponRow, EBG_EquipmentSlot WeaponSlot,
	                                   const TCHAR* OperationName) const;

	bool AttachEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot, ABG_EquippedWeaponBase* WeaponActor) const;

	USceneComponent* GetWeaponAttachParent(const TCHAR* OperationName) const;

	void RefreshEquippedWeaponAttachments();

	void DestroyEquippedWeaponActor(EBG_EquipmentSlot WeaponSlot);

	void DestroyEquippedWeaponActorInstance(ABG_EquippedWeaponBase* WeaponActor) const;

	bool SpawnDroppedEquipmentItem(EBG_ItemType DroppedItemType, const FGameplayTag& DroppedItemTag,
	                               int32 DroppedLoadedAmmo) const;

	FTransform BuildDropTransform() const;

	void NotifyEquipmentFailure(EBGInventoryFailReason FailReason, EBG_EquipmentSlot EquipmentSlot) const;

private: // --- Legacy Adapter ---
	/// Character weapon state sync
	void ApplyActiveWeaponStateToCharacter();

private: // --- Broadcast ---
	void BroadcastEquipmentChanged();
	void BroadcastActiveWeaponSlotChanged();

private: // --- Utility ---
	/// Slot display name
	static FString GetEquipmentSlotName(EBG_EquipmentSlot Slot);

	static FString GetItemTypeName(EBG_ItemType ItemType);

private: // --- Data ---
	/// Full replication visual state
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_PublicState, Category="Equipment",
		meta=(AllowPrivateAccess="true"))
	FBG_EquipmentPublicState PublicState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|World Item",
		meta=(AllowPrivateAccess="true"))
	TSubclassOf<ABG_WorldItemBase> WorldItemClass;

	UFUNCTION()
	void OnRep_PublicState();

	/// Owner-only ammo and durability state
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_OwnerState, Category="Equipment",
		meta=(AllowPrivateAccess="true"))
	FBG_EquipmentOwnerState OwnerState;

	UFUNCTION()
	void OnRep_OwnerState();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_EquippedWeaponActors,
		Category="Equipment", meta=(AllowPrivateAccess="true"))
	FBG_EquippedWeaponActorState EquippedWeaponActors;

	UFUNCTION()
	void OnRep_EquippedWeaponActors();

	/// Cached owning character
	UPROPERTY(Transient)
	TObjectPtr<ABG_Character> CachedCharacter = nullptr;

	/// Cached regular inventory
	UPROPERTY(Transient)
	TObjectPtr<UBG_InventoryComponent> CachedInventory = nullptr;
};
