// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BG_ItemTypes.h"
#include "BG_EquippedItemBase.generated.h"

class ABG_Character;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class PUBG_HOTMODE_API ABG_EquippedItemBase : public AActor
{
	GENERATED_BODY()

public:
	ABG_EquippedItemBase();

	UFUNCTION(BlueprintCallable, Category="Equipped Item")
	bool InitializeEquippedItem(ABG_Character* NewOwningCharacter, EBG_ItemType NewItemType,
	                            FGameplayTag NewItemTag);

	UFUNCTION(BlueprintCallable, Category="Equipped Item")
	bool SetOwningCharacter(ABG_Character* NewOwningCharacter);

	UFUNCTION(BlueprintCallable, Category="Equipped Item")
	void ClearOwningCharacter();

	UFUNCTION(BlueprintCallable, Category="Equipped Item")
	void DisableEquippedCollision();

	UFUNCTION(BlueprintPure, Category="Equipped Item")
	ABG_Character* GetOwningCharacter() const { return OwningCharacter; }

	UFUNCTION(BlueprintPure, Category="Equipped Item")
	EBG_ItemType GetItemType() const { return ItemType; }

	UFUNCTION(BlueprintPure, Category="Equipped Item")
	FGameplayTag GetItemTag() const { return ItemTag; }

	UFUNCTION(BlueprintPure, Category="Equipped Item")
	UStaticMeshComponent* GetEquippedMeshComponent() const { return EquippedMeshComponent; }

	UFUNCTION(BlueprintCallable, Category="Equipped Item")
	void NotifyEquipped();

	UFUNCTION(BlueprintCallable, Category="Equipped Item")
	void NotifyUnequipped();

protected:
	UFUNCTION(BlueprintNativeEvent, Category="Equipped Item")
	void OnOwningCharacterChanged(ABG_Character* NewOwningCharacter);
	virtual void OnOwningCharacterChanged_Implementation(ABG_Character* NewOwningCharacter);

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Item")
	void OnEquipped();
	virtual void OnEquipped_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category="Equipped Item")
	void OnUnequipped();
	virtual void OnUnequipped_Implementation();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipped Item", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ItemRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipped Item", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> EquippedMeshComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Equipped Item", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ABG_Character> OwningCharacter = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Item|State",
		meta=(AllowPrivateAccess="true"))
	EBG_ItemType ItemType = EBG_ItemType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipped Item|State",
		meta=(AllowPrivateAccess="true", Categories="Item"))
	FGameplayTag ItemTag;
};
