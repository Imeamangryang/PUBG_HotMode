// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "BG_EquippedWeaponBase.generated.h"

class ABG_Character;
class ABG_Scope;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class PUBG_HOTMODE_API ABG_EquippedWeaponBase : public AActor
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	ABG_EquippedWeaponBase();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public: // --- Owner Contract ---
	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	bool InitializeEquippedWeapon(ABG_Character* NewOwningCharacter);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	bool SetOwningCharacter(ABG_Character* NewOwningCharacter);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void ClearOwningCharacter();

	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	ABG_Character* GetOwningCharacter() const { return OwningCharacter; }

public: // --- Weapon Geometry ---
	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	UStaticMeshComponent* GetEquippedMeshComponent() const { return EquippedMeshComponent; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	FName GetMuzzleSocketName() const { return MuzzleSocketName; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void SetMuzzleSocketName(FName NewMuzzleSocketName);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	bool GetMuzzleTransform(FTransform& OutMuzzleTransform) const;

	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	FName GetGripSocketName() const { return GripSocketName; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void SetGripSocketName(FName NewGripSocketName);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	bool GetGripTransform(FTransform& OutGripTransform) const;

	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	FName GetSupportHandSocketName() const { return SupportHandSocketName; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void SetSupportHandSocketName(FName NewSupportHandSocketName);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	bool GetSupportHandTransform(FTransform& OutSupportHandTransform) const;

	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	FTransform GetHandAttachTransform() const { return HandAttachTransform; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void SetHandAttachTransform(const FTransform& NewHandAttachTransform);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon")
	FTransform GetBackAttachTransform() const { return BackAttachTransform; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void SetBackAttachTransform(const FTransform& NewBackAttachTransform);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Preview")
	FTransform GetPreviewTransform() const { return InventoryPreviewTransform; }

public: // --- Gameplay Hooks ---
	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void NotifyEquipped();

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void NotifyUnequipped();

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void NotifyFireTriggered(const FTransform& MuzzleTransform, int32 ShotPredictionId);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void NotifyReloadStarted(float ReloadDuration);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon")
	void NotifyReloadFinished(bool bReloadSucceeded);

public: // --- Weapon Runtime State ---
	// This block owns the runtime magazine state that must survive weapon swaps and server replication.
	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Runtime")
	void ApplyWeaponRuntimeDefinition(
		EBGWeaponPoseType NewWeaponPoseType,
		const FGameplayTag& NewAmmoItemTag,
		int32 NewMagazineCapacity,
		int32 NewLoadedAmmo);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	EBGWeaponPoseType GetWeaponPoseType() const { return WeaponPoseType; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	FGameplayTag GetAmmoItemTag() const { return AmmoItemTag; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	int32 GetMagazineCapacity() const { return MagazineCapacity; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	int32 GetLoadedAmmo() const { return LoadedAmmo; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	bool IsReloadingWeapon() const { return bIsReloadingWeapon; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Debug")
	bool UsesInfiniteDebugAmmo() const { return bUseInfiniteDebugAmmo; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Debug")
	void SetUseInfiniteDebugAmmo(bool bNewUseInfiniteDebugAmmo);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Scope")
	bool HasScope() const { return bHasScope; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Scope")
	bool IsScoping() const { return bIsScoping; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Scope")
	void SetScoping(bool bNewIsScoping);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Scope")
	float GetScopeMagnification() const { return ScopeMagnification; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Scope")
	FName GetScopeAttachSocketName() const { return ScopeAttachSocketName; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Scope")
	TSubclassOf<ABG_Scope> GetScopeActorClass() const { return ScopeActorClass; }

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Scope")
	ABG_Scope* GetScopeActor() const { return ScopeActor; }

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Scope")
	void SetScopeActor(ABG_Scope* NewScopeActor);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	bool CanFire(int32 AmmoCost = 1) const;

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Runtime")
	bool ConsumeLoadedAmmo(int32 AmmoCost);

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	int32 GetMissingAmmoCount() const;

	UFUNCTION(BlueprintPure, Category="Equipped Weapon|Runtime")
	bool CanReloadWithInventoryAmmo(int32 InventoryAmmoCount) const;

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Runtime")
	bool BeginWeaponReload();

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Runtime")
	int32 ResolveReloadAmount(int32 InventoryAmmoCount) const;

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Runtime")
	void FinishWeaponReload(int32 ReloadedAmmo);

	UFUNCTION(BlueprintCallable, Category="Equipped Weapon|Runtime")
	void CancelWeaponReload();

protected: // --- Extension Hooks ---
	UFUNCTION(BlueprintNativeEvent, Category="Equipped Weapon")
	void OnOwningCharacterChanged(ABG_Character* NewOwningCharacter);
	virtual void OnOwningCharacterChanged_Implementation(ABG_Character* NewOwningCharacter);

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Weapon")
	void OnEquipped();
	virtual void OnEquipped_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Weapon")
	void OnUnequipped();
	virtual void OnUnequipped_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Weapon")
	void OnFireTriggered(const FTransform& MuzzleTransform, int32 ShotPredictionId);
	virtual void OnFireTriggered_Implementation(const FTransform& MuzzleTransform, int32 ShotPredictionId);

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Weapon")
	void OnReloadStarted(float ReloadDuration);
	virtual void OnReloadStarted_Implementation(float ReloadDuration);

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Weapon")
	void OnReloadFinished(bool bReloadSucceeded);
	virtual void OnReloadFinished_Implementation(bool bReloadSucceeded);

private: // --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipped Weapon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> WeaponRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipped Weapon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> EquippedMeshComponent;

private: // --- Authoring Data ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon", meta=(AllowPrivateAccess="true"))
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Attach", meta=(AllowPrivateAccess="true"))
	FName GripSocketName = TEXT("Grip");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Attach", meta=(AllowPrivateAccess="true"))
	FName SupportHandSocketName = TEXT("SupportHand");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Attach", meta=(AllowPrivateAccess="true"))
	FTransform HandAttachTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Attach", meta=(AllowPrivateAccess="true"))
	FTransform BackAttachTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Preview", meta=(AllowPrivateAccess="true"))
	FTransform InventoryPreviewTransform = FTransform::Identity;

	// These authoring values let BP child weapon actors drive pose/ammo defaults before table data is wired in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime", meta=(AllowPrivateAccess="true"))
	EBGWeaponPoseType DefaultWeaponPoseType = EBGWeaponPoseType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true", Categories="Item.Ammo"))
	FGameplayTag DefaultAmmoItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 DefaultMagazineCapacity = 0;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Debug", meta=(AllowPrivateAccess="true"))
	bool bUseInfiniteDebugAmmo = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Scope", meta=(AllowPrivateAccess="true"))
	bool bHasScope = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Scope", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float ScopeMagnification = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Scope", meta=(AllowPrivateAccess="true"))
	FName ScopeAttachSocketName = TEXT("Scope");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipped Weapon|Scope", meta=(AllowPrivateAccess="true"))
	TSubclassOf<ABG_Scope> ScopeActorClass = nullptr;

private: // --- Runtime Data ---
	UPROPERTY(Transient, BlueprintReadOnly, Category="Equipped Weapon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ABG_Character> OwningCharacter = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true"))
	EBGWeaponPoseType WeaponPoseType = EBGWeaponPoseType::None;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true", Categories="Item.Ammo"))
	FGameplayTag AmmoItemTag;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 MagazineCapacity = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 LoadedAmmo = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Runtime",
		meta=(AllowPrivateAccess="true"))
	bool bIsReloadingWeapon = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Scope",
		meta=(AllowPrivateAccess="true"))
	bool bIsScoping = false;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Weapon|Scope",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<ABG_Scope> ScopeActor = nullptr;
};
