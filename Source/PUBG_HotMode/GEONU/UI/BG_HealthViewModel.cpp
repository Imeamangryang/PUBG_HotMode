// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_HealthViewModel.h"
#include "Combat/BG_EquippedWeaponBase.h"
#include "Combat/BG_HealthComponent.h"
#include "Combat/BG_WeaponFireComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemDataRow.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

namespace
{
UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(
	const UActorComponent& Component,
	const TCHAR* OperationName)
{
	UWorld* World = Component.GetWorld();
	if (!World)
	{
		LOGE(TEXT("%s: %s failed because World was null."), *GetNameSafe(&Component), OperationName);
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		LOGE(TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(&Component), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		LOGE(TEXT("%s: %s failed because UBG_ItemDataRegistrySubsystem was null."),
			*GetNameSafe(&Component),
			OperationName);
		return nullptr;
	}

	return RegistrySubsystem;
}

const FBG_WeaponItemDataRow* FindWeaponItemRow(
	const UActorComponent& Component,
	const FGameplayTag& ItemTag,
	const TCHAR* OperationName)
{
	if (!ItemTag.IsValid())
	{
		LOGE(TEXT("%s: %s failed because weapon ItemTag was invalid."), *GetNameSafe(&Component), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(Component, OperationName);
	if (!RegistrySubsystem)
		return nullptr;

	FString FailureReason;
	const FBG_WeaponItemDataRow* WeaponRow = RegistrySubsystem->FindTypedItemRow<FBG_WeaponItemDataRow>(
		EBG_ItemType::Weapon,
		ItemTag,
		&FailureReason);
	if (!WeaponRow)
	{
		LOGE(TEXT("%s: %s failed to find weapon row for %s. Reason=%s"),
			*GetNameSafe(&Component),
			OperationName,
			*ItemTag.ToString(),
			*FailureReason);
		return nullptr;
	}

	return WeaponRow;
}

bool AreActiveWeaponAmmoDataEqual(
	const FBGActiveWeaponAmmoRenderData& Left,
	const FBGActiveWeaponAmmoRenderData& Right)
{
	return Left.bHasActiveGun == Right.bHasActiveGun
		&& Left.ActiveWeaponSlot == Right.ActiveWeaponSlot
		&& Left.ActiveWeaponItemTag == Right.ActiveWeaponItemTag
		&& Left.AmmoItemTag == Right.AmmoItemTag
		&& Left.LoadedAmmo == Right.LoadedAmmo
		&& Left.MagazineSize == Right.MagazineSize
		&& Left.ReserveAmmo == Right.ReserveAmmo
		&& Left.bUsesInfiniteDebugAmmo == Right.bUsesInfiniteDebugAmmo;
}

FBGInventoryWeightRenderData MakeInventoryWeightData(float CurrentWeight, float MaxWeight)
{
	FBGInventoryWeightRenderData RenderData;
	RenderData.CurrentWeight = FMath::Max(0.f, CurrentWeight);
	RenderData.MaxWeight = FMath::Max(0.f, MaxWeight);
	RenderData.bHasCapacity = RenderData.MaxWeight > KINDA_SMALL_NUMBER;
	RenderData.FillPercent = RenderData.bHasCapacity
		                         ? FMath::Clamp(RenderData.CurrentWeight / RenderData.MaxWeight, 0.f, 1.f)
		                         : 0.f;
	return RenderData;
}

bool AreInventoryWeightDataEqual(
	const FBGInventoryWeightRenderData& Left,
	const FBGInventoryWeightRenderData& Right)
{
	return FMath::IsNearlyEqual(Left.CurrentWeight, Right.CurrentWeight)
		&& FMath::IsNearlyEqual(Left.MaxWeight, Right.MaxWeight)
		&& FMath::IsNearlyEqual(Left.FillPercent, Right.FillPercent)
		&& Left.bHasCapacity == Right.bHasCapacity;
}
}

// --- Lifecycle ------------

UBG_HealthViewModel::UBG_HealthViewModel()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBG_HealthViewModel::BeginPlay()
{
	Super::BeginPlay();

	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by PlayerController."), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController()) return;

	NotifyPossessedCharacterReady(Cast<ABG_Character>(PC->GetPawn()));
}

void UBG_HealthViewModel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();
	Super::EndPlay(EndPlayReason);
}

void UBG_HealthViewModel::NotifyPossessedCharacterReady(ABG_Character* InCharacter)
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to bind possessed character"), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController()) return;

	if (!InCharacter)
	{
		LOGE(TEXT("%s received a null character in NotifyPossessedCharacterReady"), *GetNameSafe(this));
		Unbind();
		ResetActiveWeaponAmmoData();
		return;

	}

	BindCharacter(InCharacter);
	Refresh();
	ForceUpdateAllAttributes();
}

void UBG_HealthViewModel::NotifyPossessedCharacterCleared()
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to clear possessed character binding"), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController()) return;

	Unbind();

	// Clear to default values
	HealthPercent = 0.f;
	BoostPercent = 0.f;
	bIsDead = false;
	HelmetLevel = 0;
	VestLevel = 0;
	BackpackLevel = 0;
	ResetActiveWeaponAmmoData();
	InventoryWeightData = FBGInventoryWeightRenderData();
	ForceUpdateAllAttributes();
}

// --- Public API ------------

void UBG_HealthViewModel::ForceUpdateAllAttributes()
{
	OnHealthChanged.Broadcast(HealthPercent, bIsDead);
	OnBoostChanged.Broadcast(BoostPercent);
	OnDBNOChanged.Broadcast(DBNOPercent, bIsDBNO);
	OnHelmetChanged.Broadcast(HelmetLevel);
	OnVestChanged.Broadcast(VestLevel);
	OnBackpackChanged.Broadcast(BackpackLevel);
	OnBoostRunChanged.Broadcast(bHasBoostRun);
	OnRegenerationChanged.Broadcast(bHasRegeneration);
	OnBreathChanged.Broadcast(BreathPercent, bShouldShowBreath);
	OnActiveWeaponAmmoChanged.Broadcast(ActiveWeaponAmmoData);
	OnInventoryWeightChanged.Broadcast(InventoryWeightData);
}

// --- Binding ------------

void UBG_HealthViewModel::BindCharacter(ABG_Character* InCharacter)
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to bind character"), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController()) return;

	if (!InCharacter)
	{
		LOGE(TEXT("%s received a null character in BindCharacter"), *GetNameSafe(this));
		return;
	}

	if (InCharacter != PC->GetPawn())
	{
		LOGE(TEXT("%s tried to bind a non-possessed pawn. Pawn=%s, RequestedPawn=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PC->GetPawn()),
			*GetNameSafe(InCharacter));
		return;
	}

	UBG_HealthComponent* HealthComponent = InCharacter->GetHealthComponent();
	if (!HealthComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no HealthComponent"), *GetNameSafe(this), *GetNameSafe(InCharacter));
	}

	UBG_InventoryComponent* InventoryComponent = InCharacter->GetInventoryComponent();
	if (!InventoryComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no InventoryComponent"),
			*GetNameSafe(this),
			*GetNameSafe(InCharacter));
	}

	UBG_EquipmentComponent* EquipmentComponent = InCharacter->GetEquipmentComponent();
	if (!EquipmentComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no EquipmentComponent"),
			*GetNameSafe(this),
			*GetNameSafe(InCharacter));
	}

	UBG_WeaponFireComponent* WeaponFireComponent = InCharacter->GetWeaponFireComponent();
	if (!WeaponFireComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no WeaponFireComponent"),
			*GetNameSafe(this),
			*GetNameSafe(InCharacter));
	}

	Unbind();

	BoundHealthComponent = HealthComponent;
	BoundInventoryComponent = InventoryComponent;
	BoundEquipmentComponent = EquipmentComponent;
	BoundWeaponFireComponent = WeaponFireComponent;

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveHealthChanged);
		HealthComponent->OnBoostChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveBoostChanged);
	}

	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveInventoryChanged);
		InventoryComponent->OnInventoryItemQuantityChanged.AddDynamic(
			this,
			&UBG_HealthViewModel::ReceiveInventoryItemQuantityChanged);
		InventoryComponent->OnInventoryWeightChanged.AddDynamic(
			this,
			&UBG_HealthViewModel::ReceiveInventoryWeightChanged);
	}

	if (EquipmentComponent)
	{
		EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveEquipmentChanged);
		EquipmentComponent->OnActiveWeaponSlotChanged.AddDynamic(
			this,
			&UBG_HealthViewModel::ReceiveActiveWeaponSlotChanged);
	}

	if (WeaponFireComponent)
	{
		WeaponFireComponent->OnAmmoChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveWeaponAmmoChanged);
	}
}

void UBG_HealthViewModel::Unbind()
{
	if (UBG_HealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveHealthChanged);
		HealthComponent->OnBoostChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveBoostChanged);
	}
	if (UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get())
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveInventoryChanged);
		InventoryComponent->OnInventoryItemQuantityChanged.RemoveDynamic(
			this,
			&UBG_HealthViewModel::ReceiveInventoryItemQuantityChanged);
		InventoryComponent->OnInventoryWeightChanged.RemoveDynamic(
			this,
			&UBG_HealthViewModel::ReceiveInventoryWeightChanged);
	}
	if (UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get())
	{
		EquipmentComponent->OnEquipmentChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveEquipmentChanged);
		EquipmentComponent->OnActiveWeaponSlotChanged.RemoveDynamic(
			this,
			&UBG_HealthViewModel::ReceiveActiveWeaponSlotChanged);
	}
	if (UBG_WeaponFireComponent* WeaponFireComponent = BoundWeaponFireComponent.Get())
	{
		WeaponFireComponent->OnAmmoChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveWeaponAmmoChanged);
	}

	BoundHealthComponent = nullptr;
	BoundInventoryComponent = nullptr;
	BoundEquipmentComponent = nullptr;
	BoundWeaponFireComponent = nullptr;
}

void UBG_HealthViewModel::Refresh()
{
	if (const UBG_HealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		HealthPercent = HealthComponent->GetHealthPercent();
		BoostPercent = HealthComponent->GetBoostPercent();
		bIsDead = HealthComponent->IsDead();
	}
	else
	{
		HealthPercent = 0.f;
		BoostPercent = 0.f;
		bIsDead = false;
	}

	RefreshActiveWeaponAmmoData();
	RefreshEquipmentLevels();
	RefreshInventoryWeightData();
}

void UBG_HealthViewModel::RefreshEquipmentLevels()
{
	const int32 NewHelmetLevel = ResolveEquipmentLevel(EBG_EquipmentSlot::Helmet);
	if (HelmetLevel != NewHelmetLevel)
	{
		HelmetLevel = NewHelmetLevel;
		OnHelmetChanged.Broadcast(HelmetLevel);
	}

	const int32 NewVestLevel = ResolveEquipmentLevel(EBG_EquipmentSlot::Vest);
	if (VestLevel != NewVestLevel)
	{
		VestLevel = NewVestLevel;
		OnVestChanged.Broadcast(VestLevel);
	}

	const int32 NewBackpackLevel = ResolveEquipmentLevel(EBG_EquipmentSlot::Backpack);
	if (BackpackLevel != NewBackpackLevel)
	{
		BackpackLevel = NewBackpackLevel;
		OnBackpackChanged.Broadcast(BackpackLevel);
	}
}

void UBG_HealthViewModel::RefreshActiveWeaponAmmoData()
{
	const FBGActiveWeaponAmmoRenderData NewData = BuildActiveWeaponAmmoData();
	if (!AreActiveWeaponAmmoDataEqual(ActiveWeaponAmmoData, NewData))
	{
		ActiveWeaponAmmoData = NewData;
		OnActiveWeaponAmmoChanged.Broadcast(ActiveWeaponAmmoData);
	}
}

void UBG_HealthViewModel::ResetActiveWeaponAmmoData()
{
	ActiveWeaponAmmoData = FBGActiveWeaponAmmoRenderData();
}

void UBG_HealthViewModel::RefreshInventoryWeightData()
{
	const FBGInventoryWeightRenderData NewData = BuildInventoryWeightData();
	if (!AreInventoryWeightDataEqual(InventoryWeightData, NewData))
	{
		InventoryWeightData = NewData;
		OnInventoryWeightChanged.Broadcast(InventoryWeightData);
	}
}

FBGActiveWeaponAmmoRenderData UBG_HealthViewModel::BuildActiveWeaponAmmoData() const
{
	FBGActiveWeaponAmmoRenderData RenderData;

	const UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get();
	if (!EquipmentComponent)
		return RenderData;

	const EBG_EquipmentSlot ActiveWeaponSlot = EquipmentComponent->GetActiveWeaponSlot();
	if (ActiveWeaponSlot == EBG_EquipmentSlot::None)
		return RenderData;

	const FGameplayTag ActiveWeaponItemTag = EquipmentComponent->GetActiveWeaponItemTag();
	if (!ActiveWeaponItemTag.IsValid())
		return RenderData;

	const FBG_WeaponItemDataRow* WeaponRow = FindWeaponItemRow(
		*this,
		ActiveWeaponItemTag,
		TEXT("BuildActiveWeaponAmmoData"));
	if (!WeaponRow)
		return RenderData;

	if (!WeaponRow->AmmoItemTag.IsValid())
		return RenderData;

	if (WeaponRow->MagazineSize <= 0)
		return RenderData;

	FGameplayTag AmmoItemTag = WeaponRow->AmmoItemTag;
	int32 MagazineSize = FMath::Max(0, WeaponRow->MagazineSize);
	int32 LoadedAmmo = FMath::Clamp(EquipmentComponent->GetLoadedAmmo(ActiveWeaponSlot), 0, MagazineSize);
	bool bUsesInfiniteDebugAmmo = false;

	if (const ABG_EquippedWeaponBase* WeaponActor = EquipmentComponent->GetEquippedWeaponActor(ActiveWeaponSlot))
	{
		const FGameplayTag ActorAmmoItemTag = WeaponActor->GetAmmoItemTag();
		if (ActorAmmoItemTag.IsValid())
		{
			AmmoItemTag = ActorAmmoItemTag;
		}

		const int32 ActorMagazineSize = WeaponActor->GetMagazineCapacity();
		if (ActorMagazineSize > 0)
		{
			MagazineSize = ActorMagazineSize;
		}

		LoadedAmmo = FMath::Clamp(WeaponActor->GetLoadedAmmo(), 0, MagazineSize);
		bUsesInfiniteDebugAmmo = WeaponActor->UsesInfiniteDebugAmmo();
	}

	if (!AmmoItemTag.IsValid() || MagazineSize <= 0)
		return FBGActiveWeaponAmmoRenderData();

	RenderData.bHasActiveGun = true;
	RenderData.ActiveWeaponSlot = ActiveWeaponSlot;
	RenderData.ActiveWeaponItemTag = ActiveWeaponItemTag;
	RenderData.AmmoItemTag = AmmoItemTag;
	RenderData.LoadedAmmo = LoadedAmmo;
	RenderData.MagazineSize = MagazineSize;
	RenderData.bUsesInfiniteDebugAmmo = bUsesInfiniteDebugAmmo;

	if (const UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get())
	{
		RenderData.ReserveAmmo = FMath::Max(0, InventoryComponent->GetQuantity(EBG_ItemType::Ammo, AmmoItemTag));
	}

	return RenderData;
}

FBGInventoryWeightRenderData UBG_HealthViewModel::BuildInventoryWeightData() const
{
	const UBG_InventoryComponent* InventoryComponent = BoundInventoryComponent.Get();
	if (!InventoryComponent)
		return FBGInventoryWeightRenderData();

	return MakeInventoryWeightData(
		InventoryComponent->GetCurrentWeight(),
		InventoryComponent->GetMaxWeight());
}

int32 UBG_HealthViewModel::ResolveEquipmentLevel(EBG_EquipmentSlot EquipmentSlot) const
{
	const UBG_EquipmentComponent* EquipmentComponent = BoundEquipmentComponent.Get();
	if (!EquipmentComponent)
		return 0;

	const FGameplayTag ItemTag = EquipmentComponent->GetEquippedItemTag(EquipmentSlot);
	if (!ItemTag.IsValid())
		return 0;

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem =
		GetItemDataRegistrySubsystem(*this, TEXT("ResolveEquipmentLevel"));
	if (!RegistrySubsystem)
		return 0;

	FString FailureReason;
	if (EquipmentSlot == EBG_EquipmentSlot::Helmet || EquipmentSlot == EBG_EquipmentSlot::Vest)
	{
		const FBG_ArmorItemDataRow* ArmorRow = RegistrySubsystem->FindTypedItemRow<FBG_ArmorItemDataRow>(
			EBG_ItemType::Armor,
			ItemTag,
			&FailureReason);
		if (!ArmorRow)
		{
			LOGE(TEXT("%s: ResolveEquipmentLevel failed to find armor row for %s. Reason=%s"),
			     *GetNameSafe(this),
			     *ItemTag.ToString(),
			     *FailureReason);
			return 0;
		}

		return FMath::Max(0, ArmorRow->EquipmentLevel);
	}

	if (EquipmentSlot == EBG_EquipmentSlot::Backpack)
	{
		const FBG_BackpackItemDataRow* BackpackRow = RegistrySubsystem->FindTypedItemRow<FBG_BackpackItemDataRow>(
			EBG_ItemType::Backpack,
			ItemTag,
			&FailureReason);
		if (!BackpackRow)
		{
			LOGE(TEXT("%s: ResolveEquipmentLevel failed to find backpack row for %s. Reason=%s"),
			     *GetNameSafe(this),
			     *ItemTag.ToString(),
			     *FailureReason);
			return 0;
		}

		return FMath::Max(0, BackpackRow->EquipmentLevel);
	}

	return 0;
}

// --- Event handlers ------------

void UBG_HealthViewModel::ReceiveHealthChanged(float NewHealth, float MaxHealth, bool bNewDead)
{
	float NewPercent = 0.f;
	if (MaxHealth > 0.f)
	{
		NewPercent = FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f);
	}

	if (!FMath::IsNearlyEqual(HealthPercent, NewPercent) || bIsDead != bNewDead)
	{
		HealthPercent = NewPercent;
		bIsDead = bNewDead;
		OnHealthChanged.Broadcast(HealthPercent, bIsDead);
	}
}

void UBG_HealthViewModel::ReceiveBoostChanged(float NewBoostGauge)
{
	const UBG_HealthComponent* HealthComponent = BoundHealthComponent.Get();
	if (!HealthComponent)
	{
		LOGE(TEXT("%s could not receive boost change because BoundHealthComponent was null. NewBoostGauge=%.2f"), *GetNameSafe(this), NewBoostGauge);
		return;
	}

	const float NewPercent = HealthComponent->GetBoostPercent();
	if (!FMath::IsNearlyEqual(BoostPercent, NewPercent))
	{
		BoostPercent = NewPercent;
		OnBoostChanged.Broadcast(BoostPercent);
	}
}

void UBG_HealthViewModel::ReceiveInventoryChanged()
{
	RefreshActiveWeaponAmmoData();
	RefreshInventoryWeightData();
}

void UBG_HealthViewModel::ReceiveInventoryItemQuantityChanged(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity)
{
	RefreshActiveWeaponAmmoData();
}

void UBG_HealthViewModel::ReceiveInventoryWeightChanged(float NewCurrentWeight, float NewMaxWeight)
{
	const FBGInventoryWeightRenderData NewData = MakeInventoryWeightData(NewCurrentWeight, NewMaxWeight);
	if (!AreInventoryWeightDataEqual(InventoryWeightData, NewData))
	{
		InventoryWeightData = NewData;
		OnInventoryWeightChanged.Broadcast(InventoryWeightData);
	}
}

void UBG_HealthViewModel::ReceiveEquipmentChanged()
{
	RefreshActiveWeaponAmmoData();
	RefreshEquipmentLevels();
}

void UBG_HealthViewModel::ReceiveActiveWeaponSlotChanged(EBG_EquipmentSlot ActiveWeaponSlot)
{
	RefreshActiveWeaponAmmoData();
}

void UBG_HealthViewModel::ReceiveWeaponAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo)
{
	RefreshActiveWeaponAmmoData();
}
