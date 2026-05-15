// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/BG_EquippedItemBase.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

ABG_EquippedItemBase::ABG_EquippedItemBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	ItemRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ItemRoot"));
	SetRootComponent(ItemRootComponent);

	EquippedMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EquippedMesh"));
	EquippedMeshComponent->SetupAttachment(ItemRootComponent);
	EquippedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquippedMeshComponent->SetGenerateOverlapEvents(false);
	EquippedMeshComponent->SetSimulatePhysics(false);
}

bool ABG_EquippedItemBase::InitializeEquippedItem(
	ABG_Character* NewOwningCharacter,
	EBG_ItemType NewItemType,
	FGameplayTag NewItemTag)
{
	if (!(NewItemType != EBG_ItemType::None && NewItemTag.IsValid()))
	{
		LOGE(TEXT("%s: InitializeEquippedItem failed because item identity was invalid. ItemType=%d, ItemTag=%s."),
		     *GetNameSafe(this),
		     static_cast<int32>(NewItemType),
		     *NewItemTag.ToString());
		return false;
	}

	if (!SetOwningCharacter(NewOwningCharacter))
		return false;

	ItemType = NewItemType;
	ItemTag = NewItemTag;
	bReplicates = false;
	SetReplicateMovement(false);
	DisableEquippedCollision();
	NotifyEquipped();
	return true;
}

bool ABG_EquippedItemBase::SetOwningCharacter(ABG_Character* NewOwningCharacter)
{
	if (!IsValid(NewOwningCharacter))
	{
		LOGE(TEXT("%s: SetOwningCharacter failed because NewOwningCharacter was null or invalid."),
		     *GetNameSafe(this));
		return false;
	}

	OwningCharacter = NewOwningCharacter;
	SetOwner(NewOwningCharacter);
	SetInstigator(NewOwningCharacter);
	DisableEquippedCollision();

	OnOwningCharacterChanged(NewOwningCharacter);
	return true;
}

void ABG_EquippedItemBase::ClearOwningCharacter()
{
	OwningCharacter = nullptr;
	SetOwner(nullptr);
	SetInstigator(nullptr);

	OnOwningCharacterChanged(nullptr);
}

void ABG_EquippedItemBase::DisableEquippedCollision()
{
	SetActorEnableCollision(false);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			LOGE(TEXT("%s: DisableEquippedCollision found a null PrimitiveComponent."),
			     *GetNameSafe(this));
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		PrimitiveComponent->SetSimulatePhysics(false);
	}
}

void ABG_EquippedItemBase::NotifyEquipped()
{
	OnEquipped();
}

void ABG_EquippedItemBase::NotifyUnequipped()
{
	OnUnequipped();
}

void ABG_EquippedItemBase::OnOwningCharacterChanged_Implementation(ABG_Character* NewOwningCharacter)
{
}

void ABG_EquippedItemBase::OnEquipped_Implementation()
{
}

void ABG_EquippedItemBase::OnUnequipped_Implementation()
{
}
