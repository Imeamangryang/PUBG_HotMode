// Fill out your copyright notice in the Description page of Project Settings.

#include "BG_PlayerHealthViewModel.h"
#include "Player/BG_PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

// --- Lifecycle ------------

UBG_PlayerHealthViewModel::UBG_PlayerHealthViewModel()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBG_PlayerHealthViewModel::BeginPlay()
{
	Super::BeginPlay();

	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by PlayerController."), *GetNameSafe(this));
		return;
	}

	// Only bind to local PlayerState
	if (!PC->IsLocalController()) return;

	if (auto* PS = PC->GetPlayerState<ABG_PlayerState>())
	{
		BindToPlayerState(PS);
	}
}

void UBG_PlayerHealthViewModel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromPlayerState();
	Super::EndPlay(EndPlayReason);
}

void UBG_PlayerHealthViewModel::NotifyPlayerStateReady(ABG_PlayerState* InPS)
{
	BindToPlayerState(InPS);
}

// --- Public API ------------

void UBG_PlayerHealthViewModel::ForceUpdateAllAttributes()
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

void UBG_PlayerHealthViewModel::BindToPlayerState(ABG_PlayerState* InPS)
{
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		LOGE(TEXT("%s must be owned by APlayerController to bind PlayerState"), *GetNameSafe(this));
		return;
	}

	// Only bind to local PlayerState
	if (!PC->IsLocalController()) return;

	if (!InPS)
	{
		LOGE(TEXT("%s received a null PlayerState in BindToPlayerState"), *GetNameSafe(this));
		return;
	}

	// Skip if already bound to this PlayerState
	if (BoundedPlayerState == InPS) return;

	// Sanity check
	if (auto* OwnerPS = PC->GetPlayerState<ABG_PlayerState>())
	{
		if (OwnerPS != InPS)
		{
			LOGE(TEXT("%s tried to bind a non-owning PlayerState. Owner=%s, Requested=%s"),
			     *GetNameSafe(this),
			     *GetNameSafe(OwnerPS),
			     *GetNameSafe(InPS));
			return;
		}
	}

	// Drop existing binding if any, then bind to the new PlayerState
	UnbindFromPlayerState();

	BoundedPlayerState = InPS;
	InPS->OnHealthChanged.AddDynamic(this, &UBG_PlayerHealthViewModel::ReceiveHealthChanged);

	// Push initial values
	ForceUpdateAllAttributes();
}

void UBG_PlayerHealthViewModel::UnbindFromPlayerState()
{
	if (ABG_PlayerState* PS = BoundedPlayerState.Get())
	{
		PS->OnHealthChanged.RemoveDynamic(this, &UBG_PlayerHealthViewModel::ReceiveHealthChanged);
	}
	BoundedPlayerState = nullptr;
}

// --- Event handlers ------------

void UBG_PlayerHealthViewModel::ReceiveHealthChanged(float NewHealth, float MaxHealth, bool bNewIsDead)
{
	float NewPercent = 0.f;
	if (MaxHealth > 0.f)
	{
		NewPercent = FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f);
	}

	if (!FMath::IsNearlyEqual(HealthPercent, NewPercent) || bIsDead != bNewIsDead)
	{
		HealthPercent = NewPercent;
		bIsDead = bNewIsDead;
		OnHealthChanged.Broadcast(HealthPercent, bIsDead);
	}
}
