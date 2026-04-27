#include "Combat/BG_HealthComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Player/BG_PlayerState.h"

UBG_HealthComponent::UBG_HealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UBG_HealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: BeginPlay failed because owner was null."), *GetNameSafe(this));
		return;
	}

	if (!Owner->IsA<ACharacter>())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: BeginPlay found non-character owner %s."), *GetNameSafe(this), *GetNameSafe(Owner));
	}

	if (!Owner->HasAuthority())
	{
		return;
	}

	if (MaxHP <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: BeginPlay found invalid MaxHP %.2f. Resetting to 100."), *GetNameSafe(this), MaxHP);
		MaxHP = 100.f;
	}

	if (MaxBoostGauge <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: BeginPlay found invalid MaxBoostGauge %.2f. Resetting to 100."), *GetNameSafe(this), MaxBoostGauge);
		MaxBoostGauge = 100.f;
	}

	BoostGauge = FMath::Clamp(BoostGauge, 0.f, MaxBoostGauge);
	ApplyHealthSnapshot(bIsDead ? 0.f : MaxHP, bIsDead);
}

UBG_HealthComponent* UBG_HealthComponent::FindHealthComponent(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Error, TEXT("FindHealthComponent failed because target actor was null or invalid."));
		return nullptr;
	}

	UBG_HealthComponent* HealthComponent = TargetActor->FindComponentByClass<UBG_HealthComponent>();
	if (!HealthComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("FindHealthComponent failed because %s has no health component."), *GetNameSafe(TargetActor));
		return nullptr;
	}

	return HealthComponent;
}

bool UBG_HealthComponent::ApplyDamage(float DamageAmount)
{
	if (!CanMutateHealthState(TEXT("ApplyDamage")))
	{
		return false;
	}

	if (DamageAmount <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamage failed because DamageAmount %.2f was not positive."), *GetNameSafe(this), DamageAmount);
		return false;
	}

	if (bIsDead)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamage failed because target is already dead."), *GetNameSafe(this));
		return false;
	}

	const float PreviousHP = CurrentHP;
	const float NewHP = FMath::Clamp(CurrentHP - DamageAmount, 0.f, MaxHP);
	const float AppliedDamage = PreviousHP - NewHP;
	if (AppliedDamage <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamage failed because health state was invalid. CurrentHP=%.2f, MaxHP=%.2f."), *GetNameSafe(this), CurrentHP, MaxHP);
		return false;
	}

	ApplyHealthSnapshot(NewHP, NewHP <= 0.f);
	OnDamaged.Broadcast(AppliedDamage, CurrentHP, MaxHP, bIsDead);
	return true;
}

bool UBG_HealthComponent::ApplyHeal(float HealAmount, float HealCap)
{
	if (!CanMutateHealthState(TEXT("ApplyHeal")))
	{
		return false;
	}

	if (HealAmount <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyHeal failed because HealAmount %.2f was not positive."), *GetNameSafe(this), HealAmount);
		return false;
	}

	if (bIsDead)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyHeal failed because target is dead."), *GetNameSafe(this));
		return false;
	}

	const float EffectiveHealCap = HealCap > 0.f ? FMath::Min(HealCap, MaxHP) : MaxHP;
	if (EffectiveHealCap <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyHeal failed because HealCap %.2f resolved to an invalid cap."), *GetNameSafe(this), HealCap);
		return false;
	}

	if (CurrentHP >= EffectiveHealCap)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyHeal failed because CurrentHP %.2f is already at or above HealCap %.2f."), *GetNameSafe(this), CurrentHP, EffectiveHealCap);
		return false;
	}

	const float PreviousHP = CurrentHP;
	const float NewHP = FMath::Clamp(CurrentHP + HealAmount, 0.f, EffectiveHealCap);
	if (NewHP <= PreviousHP)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyHeal failed because health did not increase. CurrentHP=%.2f, HealAmount=%.2f."), *GetNameSafe(this), CurrentHP, HealAmount);
		return false;
	}

	ApplyHealthSnapshot(NewHP, false);
	return true;
}

float UBG_HealthComponent::GetHealthPercent() const
{
	return MaxHP > 0.f ? FMath::Clamp(CurrentHP / MaxHP, 0.f, 1.f) : 0.f;
}

bool UBG_HealthComponent::AddBoost(float BoostAmount)
{
	if (!CanMutateHealthState(TEXT("AddBoost")))
	{
		return false;
	}

	if (BoostAmount <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: AddBoost failed because BoostAmount %.2f was not positive."), *GetNameSafe(this), BoostAmount);
		return false;
	}

	if (bIsDead)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: AddBoost failed because target is dead."), *GetNameSafe(this));
		return false;
	}

	if (MaxBoostGauge <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: AddBoost failed because MaxBoostGauge %.2f was invalid."), *GetNameSafe(this), MaxBoostGauge);
		return false;
	}

	if (BoostGauge >= MaxBoostGauge)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: AddBoost failed because BoostGauge %.2f is already at max %.2f."), *GetNameSafe(this), BoostGauge, MaxBoostGauge);
		return false;
	}

	const float PreviousBoostGauge = BoostGauge;
	BoostGauge = FMath::Clamp(BoostGauge + BoostAmount, 0.f, MaxBoostGauge);
	if (BoostGauge <= PreviousBoostGauge)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: AddBoost failed because boost did not increase. BoostGauge=%.2f, BoostAmount=%.2f."), *GetNameSafe(this), BoostGauge, BoostAmount);
		return false;
	}

	OnBoostChanged.Broadcast(BoostGauge);
	return true;
}

float UBG_HealthComponent::GetBoostPercent() const
{
	return MaxBoostGauge > 0.f ? FMath::Clamp(BoostGauge / MaxBoostGauge, 0.f, 1.f) : 0.f;
}

void UBG_HealthComponent::ApplyHealthSnapshot(float NewCurrentHP, bool bNewIsDead)
{
	const bool bWasDead = bIsDead;

	CurrentHP = FMath::Clamp(NewCurrentHP, 0.f, MaxHP);
	bIsDead = bNewIsDead || CurrentHP <= 0.f;

	BroadcastHealthChanged();
	BroadcastDeathStateChangedIfNeeded(bWasDead);
	SyncOwnerPlayerState();
}

void UBG_HealthComponent::SyncOwnerPlayerState() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: SyncOwnerPlayerState failed because owner was not a pawn."), *GetNameSafe(this));
		return;
	}

	ABG_PlayerState* OwnerPlayerState = OwnerPawn->GetPlayerState<ABG_PlayerState>();
	if (!OwnerPlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: SyncOwnerPlayerState failed because owner player state was null."), *GetNameSafe(this));
		return;
	}

	OwnerPlayerState->SetHealthSnapshot(CurrentHP, bIsDead, MaxHP);
}

void UBG_HealthComponent::OnRep_CurrentHP()
{
	BroadcastHealthChanged();
}

void UBG_HealthComponent::OnRep_MaxHP()
{
	BroadcastHealthChanged();
}

void UBG_HealthComponent::OnRep_IsDead(bool bOldIsDead)
{
	BroadcastHealthChanged();
	BroadcastDeathStateChangedIfNeeded(bOldIsDead);
}

void UBG_HealthComponent::OnRep_BoostGauge()
{
	OnBoostChanged.Broadcast(BoostGauge);
}

bool UBG_HealthComponent::CanMutateHealthState(const TCHAR* OperationName) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: %s failed because owner was null."), *GetNameSafe(this), OperationName);
		return false;
	}

	if (!Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: %s failed because owner %s had no authority."), *GetNameSafe(this), OperationName, *GetNameSafe(Owner));
		return false;
	}

	if (MaxHP <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: %s failed because MaxHP %.2f was invalid."), *GetNameSafe(this), OperationName, MaxHP);
		return false;
	}

	return true;
}

void UBG_HealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHP, MaxHP, bIsDead);
}

void UBG_HealthComponent::BroadcastDeathStateChangedIfNeeded(bool bWasDead)
{
	if (bWasDead != bIsDead)
	{
		OnDeathStateChanged.Broadcast(bIsDead);
	}
}

void UBG_HealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBG_HealthComponent, CurrentHP);
	DOREPLIFETIME(UBG_HealthComponent, MaxHP);
	DOREPLIFETIME(UBG_HealthComponent, bIsDead);
	DOREPLIFETIME_CONDITION(UBG_HealthComponent, BoostGauge, COND_OwnerOnly);
}
