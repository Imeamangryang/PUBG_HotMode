#include "Combat/BG_HealthComponent.h"

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

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ApplyHealthSnapshot(MaxHP, false);
		SyncOwnerPlayerState();
	}
}

bool UBG_HealthComponent::ApplyDamage(float DamageAmount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamage failed because owner was null or had no authority."), *GetNameSafe(this));
		return false;
	}

	if (bIsDead || DamageAmount <= 0.f)
	{
		return false;
	}

	const float NewHP = FMath::Clamp(CurrentHP - DamageAmount, 0.f, MaxHP);
	ApplyHealthSnapshot(NewHP, NewHP <= 0.f);
	SyncOwnerPlayerState();
	return true;
}

float UBG_HealthComponent::GetHealthPercent() const
{
	return MaxHP > 0.f ? CurrentHP / MaxHP : 0.f;
}

void UBG_HealthComponent::ApplyHealthSnapshot(float NewCurrentHP, bool bNewIsDead)
{
	CurrentHP = FMath::Clamp(NewCurrentHP, 0.f, MaxHP);
	bIsDead = bNewIsDead || CurrentHP <= 0.f;

	OnRep_CurrentHP();

	if (bIsDead)
	{
		OnRep_IsDead();
	}
}

void UBG_HealthComponent::SyncOwnerPlayerState() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	ABG_PlayerState* OwnerPlayerState = OwnerPawn->GetPlayerState<ABG_PlayerState>();
	if (!OwnerPlayerState)
	{
		return;
	}

	OwnerPlayerState->SetHealthSnapshot(CurrentHP, bIsDead, MaxHP);
}

void UBG_HealthComponent::OnRep_CurrentHP()
{
	OnHealthChanged.Broadcast(CurrentHP, MaxHP, bIsDead);
}

void UBG_HealthComponent::OnRep_IsDead()
{
	OnHealthChanged.Broadcast(CurrentHP, MaxHP, bIsDead);
}

void UBG_HealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBG_HealthComponent, CurrentHP);
	DOREPLIFETIME(UBG_HealthComponent, bIsDead);
}

