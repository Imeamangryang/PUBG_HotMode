// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_HealthViewModel.h"
#include "Combat/BG_HealthComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/BG_Character.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

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
	UnbindFromHealthComponent();
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
		UnbindFromHealthComponent();
		return;

	}

	UBG_HealthComponent* HealthComponent = InCharacter->GetHealthComponent();
	if (!HealthComponent)
	{
		LOGE(TEXT("%s could not bind because %s had no HealthComponent"), *GetNameSafe(this), *GetNameSafe(InCharacter));
		UnbindFromHealthComponent();
		return;

	}

	BindToHealthComponent(HealthComponent);
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

	UnbindFromHealthComponent();

	// Clear to default values
	HealthPercent = 0.f;
	BoostPercent = 0.f;
	bIsDead = false;
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
}

// --- Binding ------------

void UBG_HealthViewModel::BindToHealthComponent(UBG_HealthComponent* InHealthComponent)
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to bind HealthComponent"), *GetNameSafe(this));
		return;
	}

	if (!PC->IsLocalController()) return;

	if (!InHealthComponent)
	{
		LOGE(TEXT("%s received a null HealthComponent in BindToHealthComponent"), *GetNameSafe(this));
		return;
	}

	if (!(InHealthComponent->GetOwner() == PC->GetPawn()))
	{
		LOGE(TEXT("%s tried to bind a non-possessed HealthComponent. Pawn=%s, ComponentOwner=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PC->GetPawn()),
			*GetNameSafe(InHealthComponent->GetOwner()));
		return;
	}

	// Skip if already bound to the same component
	if (BoundHealthComponent == InHealthComponent)
	{
		RefreshFromHealthComponent();
		ForceUpdateAllAttributes();
		return;
	}

	// Unbind from previous component and bind to the new one
	UnbindFromHealthComponent();

	BoundHealthComponent = InHealthComponent;
	InHealthComponent->OnHealthChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveHealthChanged);
	InHealthComponent->OnBoostChanged.AddDynamic(this, &UBG_HealthViewModel::ReceiveBoostChanged);

	RefreshFromHealthComponent();
	ForceUpdateAllAttributes();
}

void UBG_HealthViewModel::UnbindFromHealthComponent()
{
	if (UBG_HealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveHealthChanged);
		HealthComponent->OnBoostChanged.RemoveDynamic(this, &UBG_HealthViewModel::ReceiveBoostChanged);
	}
	BoundHealthComponent = nullptr;
}

void UBG_HealthViewModel::RefreshFromHealthComponent()
{
	const UBG_HealthComponent* HealthComponent = BoundHealthComponent.Get();
	if (!HealthComponent)
	{
		LOGE(TEXT("%s could not refresh because BoundHealthComponent was null"), *GetNameSafe(this));
		HealthPercent = 0.f;
		BoostPercent = 0.f;
		bIsDead = false;
		return;

	}

	HealthPercent = HealthComponent->GetHealthPercent();
	BoostPercent = HealthComponent->GetBoostPercent();
	bIsDead = HealthComponent->IsDead();
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
