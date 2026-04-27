#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "BG_HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBGCharacterHealthChanged, float, NewHealth, float, MaxHealth, bool, bNewIsDead);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_HealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_HealthComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	bool IsDead() const { return bIsDead; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnBGCharacterHealthChanged OnHealthChanged;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_CurrentHP();

	UFUNCTION()
	void OnRep_IsDead();

private:
	void SyncOwnerPlayerState() const;
	void ApplyHealthSnapshot(float NewCurrentHP, bool bNewIsDead);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHP, Category = "Combat|Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHP = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Health", meta = (AllowPrivateAccess = "true"))
	float MaxHP = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Combat|Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;
};

