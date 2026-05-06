// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BG_HealthViewModel.generated.h"

//Shared delegate types

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBGHUD_FloatBoolValueChanged, float, Value, bool, bValue);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_FloatValueChanged, float, Value);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_IntValueChanged, int32, Value);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBGHUD_BoolValueChanged, bool, bValue);

/**
 * Data bridge for widgets, providing delegates for each attribute.
 * Attach to PlayerController to subscribe the possessed character's HealthComponent.
 */
UCLASS(ClassGroup="UI", meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_HealthViewModel : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_HealthViewModel();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="HUD")
	void NotifyPossessedCharacterReady(class ABG_Character* InCharacter);

	UFUNCTION(BlueprintCallable, Category="HUD")
	void NotifyPossessedCharacterCleared();

public: // --- Per-attribute delegates ---
	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatBoolValueChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatValueChanged OnBoostChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatBoolValueChanged OnDBNOChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_IntValueChanged OnHelmetChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_IntValueChanged OnVestChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_IntValueChanged OnBackpackChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_BoolValueChanged OnBoostRunChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_BoolValueChanged OnRegenerationChanged;

	UPROPERTY(BlueprintAssignable, Category="HUD")
	FOnBGHUD_FloatBoolValueChanged OnBreathChanged;

	UFUNCTION(BlueprintCallable, Category="HUD")
	void ForceUpdateAllAttributes();

public: // --- Getters (for initial values) ---
	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE void GetHealthData(float& OutHealthPercent, bool& bOutIsDead) const
	{
		OutHealthPercent = HealthPercent;
		bOutIsDead = bIsDead;
	}

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE float GetBoostPercent() const { return BoostPercent; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE void GetDBNOData(float& OutDBNOPercent, bool& bOutIsDBNO) const
	{
		OutDBNOPercent = DBNOPercent;
		bOutIsDBNO = bIsDBNO;
	}

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE int32 GetHelmetLevel() const { return HelmetLevel; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE int32 GetVestLevel() const { return VestLevel; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE int32 GetBackpackLevel() const { return BackpackLevel; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE bool GetHasBoostRun() const { return bHasBoostRun; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE bool GetHasRegeneration() const { return bHasRegeneration; }

	UFUNCTION(BlueprintPure, Category="HUD")
	FORCEINLINE void GetBreathData(float& OutBreathPercent, bool& bOutShouldShowBreath) const
	{
		OutBreathPercent = BreathPercent;
		bOutShouldShowBreath = bShouldShowBreath;
	}

private: // --- State binding ---
	void BindToHealthComponent(class UBG_HealthComponent* InHealthComponent);
	void UnbindFromHealthComponent();
	void RefreshFromHealthComponent();

	UFUNCTION()
	void ReceiveHealthChanged(float NewHealth, float MaxHealth, bool bNewDead);

	UFUNCTION()
	void ReceiveBoostChanged(float NewBoostGauge);

private: // --- Cached attribute values ---
	// Health
	float HealthPercent = 0.f;
	bool bIsDead = false;

	// Boost
	float BoostPercent = 0.f;

	// DBNO
	float DBNOPercent = 0.f;
	bool bIsDBNO = false;

	// Armor
	int32 HelmetLevel = 0;
	int32 VestLevel = 0;
	int32 BackpackLevel = 0;

	// Status effects
	bool bHasBoostRun = false;
	bool bHasRegeneration = false;

	// Breath
	float BreathPercent = 1.f;
	bool bShouldShowBreath = false;

	UPROPERTY()
	TWeakObjectPtr<class UBG_HealthComponent> BoundHealthComponent;
};