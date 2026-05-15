#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "BG_HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBGCharacterHealthChanged, float, NewHealth, float, MaxHealth, bool, bNewIsDead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnBGCharacterDamaged, float, DamageAmount, float, CurrentHP, float, MaxHP, bool, bIsDead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCharacterBoostChanged, float, BoostGauge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCharacterDeathStateChanged, bool, bIsDead);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_HealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_HealthComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	static UBG_HealthComponent* FindHealthComponent(class AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool ApplyHeal(float HealAmount, float HealCap = -1.f);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool SetHealthPercent(float NewHealthPercent);

	UFUNCTION(BlueprintCallable, Category = "Combat|Boost")
	bool AddBoost(float BoostAmount);

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Combat|Boost")
	float GetBoostGauge() const { return BoostGauge; }

	UFUNCTION(BlueprintPure, Category = "Combat|Boost")
	float GetMaxBoostGauge() const { return MaxBoostGauge; }

	UFUNCTION(BlueprintPure, Category = "Combat|Boost")
	float GetBoostPercent() const;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnBGCharacterDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnBGCharacterHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Boost")
	FOnBGCharacterBoostChanged OnBoostChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnBGCharacterDeathStateChanged OnDeathStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_CurrentHP();

	UFUNCTION()
	void OnRep_MaxHP();

	UFUNCTION()
	void OnRep_IsDead(bool bOldIsDead);

	UFUNCTION()
	void OnRep_BoostGauge();

private:
	void ApplyHealthSnapshot(float NewCurrentHP, bool bNewIsDead);
	bool CanMutateHealthState(const TCHAR* OperationName) const;
	void BroadcastHealthChanged();
	void BroadcastDeathStateChangedIfNeeded(bool bWasDead);

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHP, Category = "Combat|Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHP, Category = "Combat|Health", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MaxHP = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Combat|Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_BoostGauge, Category = "Combat|Boost", meta = (AllowPrivateAccess = "true"))
	float BoostGauge = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Boost", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float MaxBoostGauge = 100.f;
};
