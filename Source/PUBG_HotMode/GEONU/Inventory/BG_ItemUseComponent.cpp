// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_ItemUseComponent.h"

#include "BG_InventoryComponent.h"
#include "BG_ItemDataRegistrySubsystem.h"
#include "BG_ItemDataRow.h"
#include "Combat/BG_HealthComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"
#include "TimerManager.h"

UBG_ItemUseComponent::UBG_ItemUseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
	SetIsReplicatedByDefault(true);
}

void UBG_ItemUseComponent::OnRegister()
{
	Super::OnRegister();
	CacheOwnerComponents();
}

void UBG_ItemUseComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheOwnerComponents();

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		BindCancelDelegates();
	}
}

void UBG_ItemUseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearItemUseTimer();
	UnbindCancelDelegates();
	Super::EndPlay(EndPlayReason);
}

void UBG_ItemUseComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ItemUseState.bIsUsingItem)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && CachedCharacter && MovementCancelSpeedThreshold > 0.f)
	{
		const float MovementCancelSpeedSq = FMath::Square(MovementCancelSpeedThreshold);
		if (CachedCharacter->GetVelocity().SizeSquared2D() > MovementCancelSpeedSq)
		{
			CancelActiveItemUse(TEXT("movement"));
			return;
		}
	}

	BroadcastItemUseProgress();
}

void UBG_ItemUseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UBG_ItemUseComponent, ItemUseState, COND_OwnerOnly);
}

UBG_ItemUseComponent* UBG_ItemUseComponent::FindItemUseComponent(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		LOGE(TEXT("FindItemUseComponent failed because target actor was null or invalid."));
		return nullptr;
	}

	UBG_ItemUseComponent* ItemUseComponent = TargetActor->FindComponentByClass<UBG_ItemUseComponent>();
	if (!ItemUseComponent)
	{
		LOGE(TEXT("FindItemUseComponent failed because %s has no item use component."),
		     *GetNameSafe(TargetActor));
		return nullptr;
	}

	return ItemUseComponent;
}

void UBG_ItemUseComponent::RequestUseItem(EBG_ItemType ItemType, FGameplayTag ItemTag)
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
		if (!TryStartUseItem(ItemType, ItemTag, FailReason))
		{
			NotifyItemUseFailure(FailReason, ItemType, ItemTag);
		}
		return;
	}

	Server_RequestUseItem(ItemType, ItemTag);
}

void UBG_ItemUseComponent::RequestCancelItemUse()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		CancelActiveItemUse(TEXT("explicit cancel"));
		return;
	}

	Server_RequestCancelItemUse();
}

void UBG_ItemUseComponent::NotifyMovementInput()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		CancelActiveItemUse(TEXT("movement input"));
		return;
	}

	if (ItemUseState.bIsUsingItem)
	{
		Server_RequestCancelItemUse();
	}
}

void UBG_ItemUseComponent::NotifyFireInput()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		CancelActiveItemUse(TEXT("fire input"));
		return;
	}

	Server_RequestCancelItemUse();
}

void UBG_ItemUseComponent::NotifyWeaponSwitch()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		CancelActiveItemUse(TEXT("weapon switch"));
		return;
	}

	Server_RequestCancelItemUse();
}

bool UBG_ItemUseComponent::TryStartUseItem(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (!CanMutateItemUseState(TEXT("TryStartUseItem")))
	{
		return false;
	}

	if (!GetWorld())
	{
		LOGE(TEXT("%s: TryStartUseItem failed because World was null."), *GetNameSafe(this));
		return false;
	}

	CacheOwnerComponents();

	float UseDuration = 0.f;
	if (!ValidateUseStart(ItemType, ItemTag, UseDuration, OutFailReason))
	{
		return false;
	}

	BeginItemUse(ItemType, ItemTag, UseDuration);
	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

float UBG_ItemUseComponent::GetItemUseProgress() const
{
	if (!ItemUseState.bIsUsingItem)
	{
		return 0.f;
	}

	if (ItemUseState.UseDuration <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const float ElapsedTime = FMath::Max(0.f, GetServerWorldTimeSeconds() - ItemUseState.UseStartServerTime);
	return FMath::Clamp(ElapsedTime / ItemUseState.UseDuration, 0.f, 1.f);
}

float UBG_ItemUseComponent::GetRemainingUseTime() const
{
	if (!ItemUseState.bIsUsingItem)
	{
		return 0.f;
	}

	const float ElapsedTime = FMath::Max(0.f, GetServerWorldTimeSeconds() - ItemUseState.UseStartServerTime);
	return FMath::Max(0.f, ItemUseState.UseDuration - ElapsedTime);
}

void UBG_ItemUseComponent::Server_RequestUseItem_Implementation(EBG_ItemType ItemType, FGameplayTag ItemTag)
{
	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	if (!TryStartUseItem(ItemType, ItemTag, FailReason))
	{
		NotifyItemUseFailure(FailReason, ItemType, ItemTag);
	}
}

void UBG_ItemUseComponent::Server_RequestCancelItemUse_Implementation()
{
	CancelActiveItemUse(TEXT("explicit cancel"));
}

void UBG_ItemUseComponent::OnRep_ItemUseState()
{
	RefreshProgressTickEnabled();
	BroadcastItemUseStateChanged();
	BroadcastItemUseProgress();
}

void UBG_ItemUseComponent::HandleOwnerDamaged(float DamageAmount, float CurrentHP, float MaxHP, bool bIsDead)
{
	CancelActiveItemUse(TEXT("damage"));
}

void UBG_ItemUseComponent::HandleDeathStateChanged(bool bIsDead)
{
	if (bIsDead)
	{
		CancelActiveItemUse(TEXT("death"));
	}
}

void UBG_ItemUseComponent::HandleActiveWeaponSlotChanged(EBG_EquipmentSlot ActiveWeaponSlot)
{
	CancelActiveItemUse(TEXT("active weapon slot change"));
}

void UBG_ItemUseComponent::CacheOwnerComponents()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner was null."), *GetNameSafe(this));
		return;
	}

	CachedCharacter = Cast<ABG_Character>(Owner);
	if (!CachedCharacter)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s was not ABG_Character."),
		     *GetNameSafe(this),
		     *GetNameSafe(Owner));
	}

	CachedInventory = Owner->FindComponentByClass<UBG_InventoryComponent>();
	if (!CachedInventory)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s had no InventoryComponent."),
		     *GetNameSafe(this),
		     *GetNameSafe(Owner));
	}

	CachedHealth = Owner->FindComponentByClass<UBG_HealthComponent>();
	if (!CachedHealth)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s had no HealthComponent."),
		     *GetNameSafe(this),
		     *GetNameSafe(Owner));
	}

	CachedEquipment = Owner->FindComponentByClass<UBG_EquipmentComponent>();
	if (!CachedEquipment)
	{
		LOGE(TEXT("%s: CacheOwnerComponents failed because owner %s had no EquipmentComponent."),
		     *GetNameSafe(this),
		     *GetNameSafe(Owner));
	}
}

void UBG_ItemUseComponent::BindCancelDelegates()
{
	if (CachedHealth)
	{
		CachedHealth->OnDamaged.AddUniqueDynamic(this, &UBG_ItemUseComponent::HandleOwnerDamaged);
		CachedHealth->OnDeathStateChanged.AddUniqueDynamic(this, &UBG_ItemUseComponent::HandleDeathStateChanged);
	}

	if (CachedEquipment)
	{
		CachedEquipment->OnActiveWeaponSlotChanged.AddUniqueDynamic(
			this, &UBG_ItemUseComponent::HandleActiveWeaponSlotChanged);
	}
}

void UBG_ItemUseComponent::UnbindCancelDelegates()
{
	if (CachedHealth)
	{
		CachedHealth->OnDamaged.RemoveDynamic(this, &UBG_ItemUseComponent::HandleOwnerDamaged);
		CachedHealth->OnDeathStateChanged.RemoveDynamic(this, &UBG_ItemUseComponent::HandleDeathStateChanged);
	}

	if (CachedEquipment)
	{
		CachedEquipment->OnActiveWeaponSlotChanged.RemoveDynamic(
			this, &UBG_ItemUseComponent::HandleActiveWeaponSlotChanged);
	}
}

bool UBG_ItemUseComponent::CanMutateItemUseState(const TCHAR* OperationName) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		LOGE(TEXT("%s: %s failed because owner was null."), *GetNameSafe(this), OperationName);
		return false;
	}

	if (!Owner->HasAuthority())
	{
		LOGE(TEXT("%s: %s failed because owner %s had no authority."),
		     *GetNameSafe(this),
		     OperationName,
		     *GetNameSafe(Owner));
		return false;
	}

	return true;
}

bool UBG_ItemUseComponent::IsSupportedItemUseType(EBG_ItemType ItemType) const
{
	return ItemType == EBG_ItemType::Heal || ItemType == EBG_ItemType::Boost;
}

bool UBG_ItemUseComponent::ValidateUseStart(
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	float& OutUseDuration,
	EBGInventoryFailReason& OutFailReason) const
{
	OutUseDuration = 0.f;
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (ItemUseState.bIsUsingItem)
	{
		LOGE(TEXT("%s: ValidateUseStart failed because another item is already being used. ItemType=%s, ItemTag=%s."),
		     *GetNameSafe(this),
		     *GetItemTypeName(ItemUseState.ItemType),
		     *ItemUseState.ItemTag.ToString());
		OutFailReason = EBGInventoryFailReason::AlreadyUsingItem;
		return false;
	}

	if (!IsSupportedItemUseType(ItemType))
	{
		LOGE(TEXT("%s: ValidateUseStart failed because item type %s is not usable here."),
		     *GetNameSafe(this),
		     *GetItemTypeName(ItemType));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: ValidateUseStart failed because ItemTag was invalid for item type %s."),
		     *GetNameSafe(this),
		     *GetItemTypeName(ItemType));
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!CachedInventory || !CachedHealth || !CachedCharacter)
	{
		LOGE(TEXT("%s: ValidateUseStart failed. Character=%s, Inventory=%s, Health=%s."),
		     *GetNameSafe(this),
		     *GetNameSafe(CachedCharacter),
		     *GetNameSafe(CachedInventory),
		     *GetNameSafe(CachedHealth));
		return false;
	}

	if (CachedHealth->IsDead() || CachedCharacter->GetCharacterState() == EBGCharacterState::Dead)
	{
		LOGE(TEXT("%s: ValidateUseStart failed because owner character is dead."), *GetNameSafe(this));
		return false;
	}

	if (CachedInventory->GetQuantity(ItemType, ItemTag) <= 0)
	{
		LOGE(TEXT("%s: ValidateUseStart failed because inventory has no %s %s."),
		     *GetNameSafe(this),
		     *GetItemTypeName(ItemType),
		     *ItemTag.ToString());
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (ItemType == EBG_ItemType::Heal)
	{
		const FBG_HealItemDataRow* HealRow = nullptr;
		if (!ValidateHealUseStart(ItemTag, HealRow, OutFailReason))
		{
			return false;
		}

		OutUseDuration = HealRow->UseDuration;
		return true;
	}

	const FBG_BoostItemDataRow* BoostRow = nullptr;
	if (!ValidateBoostUseStart(ItemTag, BoostRow, OutFailReason))
	{
		return false;
	}

	OutUseDuration = BoostRow->UseDuration;
	return true;
}

bool UBG_ItemUseComponent::ValidateHealUseStart(
	const FGameplayTag& ItemTag,
	const FBG_HealItemDataRow*& OutHealRow,
	EBGInventoryFailReason& OutFailReason) const
{
	OutHealRow = FindHealItemRow(ItemTag, TEXT("ValidateHealUseStart"));
	if (!OutHealRow)
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (OutHealRow->HealAmount <= 0.f || OutHealRow->HealCap <= 0.f || OutHealRow->UseDuration < 0.f)
	{
		LOGE(TEXT("%s: ValidateHealUseStart failed because heal row %s had invalid values. HealAmount=%.2f, HealCap=%.2f, UseDuration=%.2f."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     OutHealRow->HealAmount,
		     OutHealRow->HealCap,
		     OutHealRow->UseDuration);
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!CachedHealth)
	{
		LOGE(TEXT("%s: ValidateHealUseStart failed because CachedHealth was null."), *GetNameSafe(this));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	const float EffectiveHealCap = FMath::Min(OutHealRow->HealCap, CachedHealth->GetMaxHP());
	if (EffectiveHealCap <= 0.f)
	{
		LOGE(TEXT("%s: ValidateHealUseStart failed because heal row %s resolved to invalid cap %.2f."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     EffectiveHealCap);
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (CachedHealth->GetCurrentHP() >= EffectiveHealCap - KINDA_SMALL_NUMBER)
	{
		LOGE(TEXT("%s: ValidateHealUseStart rejected %s because CurrentHP %.2f is at or above HealCap %.2f."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     CachedHealth->GetCurrentHP(),
		     EffectiveHealCap);
		OutFailReason = EBGInventoryFailReason::HealthCapReached;
		return false;
	}

	return true;
}

bool UBG_ItemUseComponent::ValidateBoostUseStart(
	const FGameplayTag& ItemTag,
	const FBG_BoostItemDataRow*& OutBoostRow,
	EBGInventoryFailReason& OutFailReason) const
{
	OutBoostRow = FindBoostItemRow(ItemTag, TEXT("ValidateBoostUseStart"));
	if (!OutBoostRow)
	{
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (OutBoostRow->BoostAmount <= 0.f || OutBoostRow->UseDuration < 0.f)
	{
		LOGE(TEXT("%s: ValidateBoostUseStart failed because boost row %s had invalid values. BoostAmount=%.2f, UseDuration=%.2f."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     OutBoostRow->BoostAmount,
		     OutBoostRow->UseDuration);
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!CachedHealth)
	{
		LOGE(TEXT("%s: ValidateBoostUseStart failed because CachedHealth was null."), *GetNameSafe(this));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (CachedHealth->GetMaxBoostGauge() <= 0.f)
	{
		LOGE(TEXT("%s: ValidateBoostUseStart failed because MaxBoostGauge %.2f was invalid."),
		     *GetNameSafe(this),
		     CachedHealth->GetMaxBoostGauge());
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (CachedHealth->GetBoostGauge() >= CachedHealth->GetMaxBoostGauge() - KINDA_SMALL_NUMBER)
	{
		LOGE(TEXT("%s: ValidateBoostUseStart rejected %s because BoostGauge %.2f is at max %.2f."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     CachedHealth->GetBoostGauge(),
		     CachedHealth->GetMaxBoostGauge());
		OutFailReason = EBGInventoryFailReason::HealthCapReached;
		return false;
	}

	return true;
}

UBG_ItemDataRegistrySubsystem* UBG_ItemUseComponent::GetItemDataRegistrySubsystem(
	const TCHAR* OperationName) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: %s failed because World was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		LOGE(TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		LOGE(TEXT("%s: %s failed because UBG_ItemDataRegistrySubsystem was null."),
		     *GetNameSafe(this),
		     OperationName);
	}

	return RegistrySubsystem;
}

const FBG_HealItemDataRow* UBG_ItemUseComponent::FindHealItemRow(
	const FGameplayTag& ItemTag,
	const TCHAR* OperationName) const
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because heal ItemTag was invalid."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		? RegistrySubsystem->FindTypedItemRow<FBG_HealItemDataRow>(EBG_ItemType::Heal, ItemTag)
		: nullptr;
}

const FBG_BoostItemDataRow* UBG_ItemUseComponent::FindBoostItemRow(
	const FGameplayTag& ItemTag,
	const TCHAR* OperationName) const
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because boost ItemTag was invalid."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	return RegistrySubsystem
		? RegistrySubsystem->FindTypedItemRow<FBG_BoostItemDataRow>(EBG_ItemType::Boost, ItemTag)
		: nullptr;
}

void UBG_ItemUseComponent::BeginItemUse(
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	float UseDuration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: BeginItemUse failed because World was null."), *GetNameSafe(this));
		return;
	}

	SetItemUseState(ItemType, ItemTag, UseDuration);

	if (UseDuration <= KINDA_SMALL_NUMBER)
	{
		CompleteActiveItemUse();
		return;
	}

	World->GetTimerManager().SetTimer(
		ItemUseTimerHandle,
		this,
		&UBG_ItemUseComponent::CompleteActiveItemUse,
		UseDuration,
		false);
}

void UBG_ItemUseComponent::CompleteActiveItemUse()
{
	if (!CanMutateItemUseState(TEXT("CompleteActiveItemUse")))
	{
		return;
	}

	if (!ItemUseState.bIsUsingItem)
	{
		return;
	}

	const FBG_ItemUseRepState CompletedUseState = ItemUseState;
	ClearItemUseTimer();
	CacheOwnerComponents();

	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	const bool bApplied = ApplyCompletedItemUse(CompletedUseState, FailReason);

	ResetItemUseState();

	if (!bApplied && FailReason != EBGInventoryFailReason::None)
	{
		NotifyItemUseFailure(FailReason, CompletedUseState.ItemType, CompletedUseState.ItemTag);
	}
}

bool UBG_ItemUseComponent::ApplyCompletedItemUse(
	const FBG_ItemUseRepState& CompletedUseState,
	EBGInventoryFailReason& OutFailReason)
{
	OutFailReason = EBGInventoryFailReason::ServerRejected;

	if (!CompletedUseState.IsValidUse())
	{
		LOGE(TEXT("%s: ApplyCompletedItemUse failed because completed use state was invalid. ItemType=%s, ItemTag=%s."),
		     *GetNameSafe(this),
		     *GetItemTypeName(CompletedUseState.ItemType),
		     *CompletedUseState.ItemTag.ToString());
		OutFailReason = EBGInventoryFailReason::InvalidItem;
		return false;
	}

	if (!CachedInventory || !CachedHealth || !CachedCharacter)
	{
		LOGE(TEXT("%s: ApplyCompletedItemUse failed. Character=%s, Inventory=%s, Health=%s."),
		     *GetNameSafe(this),
		     *GetNameSafe(CachedCharacter),
		     *GetNameSafe(CachedInventory),
		     *GetNameSafe(CachedHealth));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (CachedHealth->IsDead() || CachedCharacter->GetCharacterState() == EBGCharacterState::Dead)
	{
		LOGE(TEXT("%s: ApplyCompletedItemUse failed because owner character is dead."), *GetNameSafe(this));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	if (CompletedUseState.ItemType == EBG_ItemType::Heal)
	{
		return ApplyCompletedHealUse(CompletedUseState.ItemTag, OutFailReason);
	}

	if (CompletedUseState.ItemType == EBG_ItemType::Boost)
	{
		return ApplyCompletedBoostUse(CompletedUseState.ItemTag, OutFailReason);
	}

	OutFailReason = EBGInventoryFailReason::InvalidItem;
	return false;
}

bool UBG_ItemUseComponent::ApplyCompletedHealUse(
	const FGameplayTag& ItemTag,
	EBGInventoryFailReason& OutFailReason)
{
	const FBG_HealItemDataRow* HealRow = nullptr;
	if (!ValidateHealUseStart(ItemTag, HealRow, OutFailReason))
	{
		return false;
	}

	if (!CachedInventory || !CachedHealth)
	{
		LOGE(TEXT("%s: ApplyCompletedHealUse failed. Inventory=%s, Health=%s."),
		     *GetNameSafe(this),
		     *GetNameSafe(CachedInventory),
		     *GetNameSafe(CachedHealth));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	int32 RemovedQuantity = 0;
	if (!CachedInventory->TryRemoveItem(EBG_ItemType::Heal, ItemTag, 1, RemovedQuantity)
		|| RemovedQuantity != 1)
	{
		LOGE(TEXT("%s: ApplyCompletedHealUse failed because TryRemoveItem removed %d instead of 1 for %s."),
		     *GetNameSafe(this),
		     RemovedQuantity,
		     *ItemTag.ToString());
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (!CachedHealth->ApplyHeal(HealRow->HealAmount, HealRow->HealCap))
	{
		int32 RollbackAddedQuantity = 0;
		CachedInventory->TryAddItem(EBG_ItemType::Heal, ItemTag, RemovedQuantity, RollbackAddedQuantity);
		LOGE(TEXT("%s: ApplyCompletedHealUse failed because ApplyHeal failed for %s. RollbackAdded=%d."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     RollbackAddedQuantity);
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_ItemUseComponent::ApplyCompletedBoostUse(
	const FGameplayTag& ItemTag,
	EBGInventoryFailReason& OutFailReason)
{
	const FBG_BoostItemDataRow* BoostRow = nullptr;
	if (!ValidateBoostUseStart(ItemTag, BoostRow, OutFailReason))
	{
		return false;
	}

	if (!CachedInventory || !CachedHealth)
	{
		LOGE(TEXT("%s: ApplyCompletedBoostUse failed. Inventory=%s, Health=%s."),
		     *GetNameSafe(this),
		     *GetNameSafe(CachedInventory),
		     *GetNameSafe(CachedHealth));
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	int32 RemovedQuantity = 0;
	if (!CachedInventory->TryRemoveItem(EBG_ItemType::Boost, ItemTag, 1, RemovedQuantity)
		|| RemovedQuantity != 1)
	{
		LOGE(TEXT("%s: ApplyCompletedBoostUse failed because TryRemoveItem removed %d instead of 1 for %s."),
		     *GetNameSafe(this),
		     RemovedQuantity,
		     *ItemTag.ToString());
		OutFailReason = EBGInventoryFailReason::InvalidQuantity;
		return false;
	}

	if (!CachedHealth->AddBoost(BoostRow->BoostAmount))
	{
		int32 RollbackAddedQuantity = 0;
		CachedInventory->TryAddItem(EBG_ItemType::Boost, ItemTag, RemovedQuantity, RollbackAddedQuantity);
		LOGE(TEXT("%s: ApplyCompletedBoostUse failed because AddBoost failed for %s. RollbackAdded=%d."),
		     *GetNameSafe(this),
		     *ItemTag.ToString(),
		     RollbackAddedQuantity);
		OutFailReason = EBGInventoryFailReason::ServerRejected;
		return false;
	}

	OutFailReason = EBGInventoryFailReason::None;
	return true;
}

bool UBG_ItemUseComponent::CancelActiveItemUse(const TCHAR* CancelReason)
{
	if (!CanMutateItemUseState(TEXT("CancelActiveItemUse")))
	{
		return false;
	}

	if (!ItemUseState.bIsUsingItem)
	{
		return false;
	}

	LOGD(TEXT("%s: Canceled item use. Reason=%s, ItemType=%s, ItemTag=%s."),
	     *GetNameSafe(this),
	     CancelReason ? CancelReason : TEXT("<Unknown>"),
	     *GetItemTypeName(ItemUseState.ItemType),
	     *ItemUseState.ItemTag.ToString());

	ClearItemUseTimer();
	ResetItemUseState();
	return true;
}

void UBG_ItemUseComponent::SetItemUseState(
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag,
	float UseDuration)
{
	ItemUseState.bIsUsingItem = true;
	ItemUseState.ItemType = ItemType;
	ItemUseState.ItemTag = ItemTag;
	ItemUseState.UseDuration = FMath::Max(0.f, UseDuration);
	ItemUseState.UseStartServerTime = GetServerWorldTimeSeconds();

	RefreshProgressTickEnabled();
	BroadcastItemUseStateChanged();
	BroadcastItemUseProgress();

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UBG_ItemUseComponent::ResetItemUseState()
{
	ItemUseState = FBG_ItemUseRepState();
	RefreshProgressTickEnabled();
	BroadcastItemUseStateChanged();
	BroadcastItemUseProgress();

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UBG_ItemUseComponent::ClearItemUseTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ItemUseTimerHandle);
		return;
	}

	if (ItemUseTimerHandle.IsValid())
	{
		LOGE(TEXT("%s: ClearItemUseTimer could not clear timer because World was null."), *GetNameSafe(this));
	}
}

void UBG_ItemUseComponent::RefreshProgressTickEnabled()
{
	SetComponentTickEnabled(ItemUseState.bIsUsingItem);
}

void UBG_ItemUseComponent::NotifyItemUseFailure(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	const FGameplayTag& ItemTag) const
{
	ABG_Character* OwnerCharacter = CachedCharacter.Get();
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ABG_Character>(GetOwner());
	}

	if (!OwnerCharacter)
	{
		LOGE(TEXT("%s: NotifyItemUseFailure failed because owner was not ABG_Character. FailReason=%d"),
		     *GetNameSafe(this),
		     static_cast<int32>(FailReason));
		return;
	}

	OwnerCharacter->Client_ReceiveInventoryFailure(FailReason, ItemType, ItemTag);
}

float UBG_ItemUseComponent::GetServerWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: GetServerWorldTimeSeconds failed because World was null."), *GetNameSafe(this));
		return 0.f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void UBG_ItemUseComponent::BroadcastItemUseStateChanged()
{
	OnItemUseStateChanged.Broadcast(ItemUseState);
}

void UBG_ItemUseComponent::BroadcastItemUseProgress()
{
	const float Progress = GetItemUseProgress();
	const float RemainingTime = GetRemainingUseTime();
	const float ElapsedTime = ItemUseState.bIsUsingItem
		? FMath::Max(0.f, ItemUseState.UseDuration - RemainingTime)
		: 0.f;
	OnItemUseProgressChanged.Broadcast(Progress, ElapsedTime, RemainingTime);
}

FString UBG_ItemUseComponent::GetItemTypeName(EBG_ItemType ItemType)
{
	const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
	return ItemTypeEnum ? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(ItemType)) : TEXT("<Unknown>");
}
