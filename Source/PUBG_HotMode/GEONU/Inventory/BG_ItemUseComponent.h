// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BG_EquipmentComponent.h"
#include "BG_ItemTypes.h"
#include "BG_ItemUseComponent.generated.h"

class ABG_Character;
class UBG_HealthComponent;
class UBG_InventoryComponent;
class UBG_ItemDataRegistrySubsystem;
struct FBG_BoostItemDataRow;
struct FBG_HealItemDataRow;

USTRUCT(BlueprintType)
struct PUBG_HOTMODE_API FBG_ItemUseRepState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Item Use")
	bool bIsUsingItem = false;

	UPROPERTY(BlueprintReadOnly, Category="Item Use")
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(BlueprintReadOnly, Category="Item Use", meta=(Categories="Item"))
	FGameplayTag ItemTag;

	UPROPERTY(BlueprintReadOnly, Category="Item Use", meta=(ClampMin="0.0"))
	float UseDuration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Item Use")
	float UseStartServerTime = 0.f;

	bool IsValidUse() const
	{
		return bIsUsingItem
			&& (ItemType == EBG_ItemType::Heal || ItemType == EBG_ItemType::Boost)
			&& ItemTag.IsValid()
			&& UseDuration >= 0.f;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGItemUseStateChanged, const FBG_ItemUseRepState&, ItemUseState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBGItemUseProgressChanged, float, Progress, float, ElapsedTime, float, RemainingTime);

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_ItemUseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_ItemUseComponent();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UFUNCTION(BlueprintCallable, Category="Item Use")
	static UBG_ItemUseComponent* FindItemUseComponent(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Item Use")
	void UseItem(EBG_ItemType ItemType, FGameplayTag ItemTag);

	UFUNCTION(BlueprintCallable, Category="Item Use")
	void CancelItemUse();

	UFUNCTION(BlueprintCallable, Category="Item Use")
	void NotifyMovementInput();

	UFUNCTION(BlueprintCallable, Category="Item Use")
	void NotifyFireInput();

	UFUNCTION(BlueprintCallable, Category="Item Use")
	void NotifyWeaponSwitch();

	bool Auth_StartUseItem(EBG_ItemType ItemType, FGameplayTag ItemTag, EBGInventoryFailReason& OutFailReason);

	UFUNCTION(BlueprintPure, Category="Item Use")
	bool IsUsingItem() const { return ItemUseState.bIsUsingItem; }

	UFUNCTION(BlueprintPure, Category="Item Use")
	FBG_ItemUseRepState GetItemUseState() const { return ItemUseState; }

	UFUNCTION(BlueprintPure, Category="Item Use")
	float GetItemUseProgress() const;

	UFUNCTION(BlueprintPure, Category="Item Use")
	float GetRemainingUseTime() const;

public:
	UPROPERTY(BlueprintAssignable, Category="Item Use")
	FOnBGItemUseStateChanged OnItemUseStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Item Use")
	FOnBGItemUseProgressChanged OnItemUseProgressChanged;

private:
	UFUNCTION(Server, Reliable)
	void Server_UseItem(EBG_ItemType ItemType, FGameplayTag ItemTag);

	UFUNCTION(Server, Reliable)
	void Server_CancelItemUse();

	UFUNCTION()
	void OnRep_ItemUseState();

	UFUNCTION()
	void HandleOwnerDamaged(float DamageAmount, float CurrentHP, float MaxHP, bool bIsDead);

	UFUNCTION()
	void HandleDeathStateChanged(bool bIsDead);

	UFUNCTION()
	void HandleActiveWeaponSlotChanged(EBG_EquipmentSlot ActiveWeaponSlot);

private:
	void CacheOwnerComponents();
	void BindCancelDelegates();
	void UnbindCancelDelegates();

	bool CanMutateItemUseState(const TCHAR* OperationName) const;
	bool IsSupportedItemUseType(EBG_ItemType ItemType) const;
	bool ValidateUseStart(EBG_ItemType ItemType, const FGameplayTag& ItemTag,
	                      float& OutUseDuration, EBGInventoryFailReason& OutFailReason) const;
	bool ValidateHealUseStart(const FGameplayTag& ItemTag, const FBG_HealItemDataRow*& OutHealRow,
	                          EBGInventoryFailReason& OutFailReason) const;
	bool ValidateBoostUseStart(const FGameplayTag& ItemTag, const FBG_BoostItemDataRow*& OutBoostRow,
	                           EBGInventoryFailReason& OutFailReason) const;

	UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(const TCHAR* OperationName) const;
	const FBG_HealItemDataRow* FindHealItemRow(const FGameplayTag& ItemTag, const TCHAR* OperationName) const;
	const FBG_BoostItemDataRow* FindBoostItemRow(const FGameplayTag& ItemTag, const TCHAR* OperationName) const;

	void BeginItemUse(EBG_ItemType ItemType, const FGameplayTag& ItemTag, float UseDuration);
	void CompleteActiveItemUse();
	bool ApplyCompletedItemUse(const FBG_ItemUseRepState& CompletedUseState,
	                           EBGInventoryFailReason& OutFailReason);
	bool ApplyCompletedHealUse(const FGameplayTag& ItemTag, EBGInventoryFailReason& OutFailReason);
	bool ApplyCompletedBoostUse(const FGameplayTag& ItemTag, EBGInventoryFailReason& OutFailReason);
	bool CancelActiveItemUse(const TCHAR* CancelReason);

	void SetItemUseState(EBG_ItemType ItemType, const FGameplayTag& ItemTag, float UseDuration);
	void ResetItemUseState();
	void ClearItemUseTimer();
	void RefreshProgressTickEnabled();
	void NotifyItemUseFailure(EBGInventoryFailReason FailReason, EBG_ItemType ItemType,
	                          const FGameplayTag& ItemTag) const;

	float GetServerWorldTimeSeconds() const;
	void BroadcastItemUseStateChanged();
	void BroadcastItemUseProgress();

	static FString GetItemTypeName(EBG_ItemType ItemType);

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_ItemUseState, Category="Item Use",
		meta=(AllowPrivateAccess="true"))
	FBG_ItemUseRepState ItemUseState;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item Use",
		meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float MovementCancelSpeedThreshold = 5.f;

	UPROPERTY(Transient)
	TObjectPtr<ABG_Character> CachedCharacter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBG_InventoryComponent> CachedInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBG_HealthComponent> CachedHealth = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBG_EquipmentComponent> CachedEquipment = nullptr;

	FTimerHandle ItemUseTimerHandle;
};
