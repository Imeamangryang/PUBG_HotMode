// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BG_EquippedWeaponBase.generated.h"

class ABG_Character;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class PUBG_HOTMODE_API ABG_EquippedWeaponBase : public AActor
{
	GENERATED_BODY()

public: // --- Lifecycle ---
	ABG_EquippedWeaponBase();

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

private: // --- Runtime Data ---
	UPROPERTY(Transient, BlueprintReadOnly, Category="Equipped Weapon", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ABG_Character> OwningCharacter = nullptr;
};
