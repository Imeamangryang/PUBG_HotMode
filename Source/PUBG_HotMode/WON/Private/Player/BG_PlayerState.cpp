// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/BG_PlayerState.h"
#include "Net/UnrealNetwork.h"

ABG_PlayerState::ABG_PlayerState()
{
	MaxHP = 100.f;
	CurrentHP = MaxHP;
	bIsDead = false;
}

bool ABG_PlayerState::ApplyDamage(float DamageAmount)
{
	if (!HasAuthority() || bIsDead || DamageAmount <= 0.f)
	{
		return false;
	}

	SetHealthSnapshot(CurrentHP - DamageAmount, CurrentHP - DamageAmount <= 0.f, MaxHP);
	return true;  
}

void ABG_PlayerState::SetHealthSnapshot(float NewCurrentHP, bool bNewIsDead, float NewMaxHP)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: SetHealthSnapshot failed because PlayerState had no authority."), *GetNameSafe(this));
		return;
	}

	MaxHP = FMath::Max(1.f, NewMaxHP);
	CurrentHP = FMath::Clamp(NewCurrentHP, 0.f, MaxHP);
	bIsDead = bNewIsDead || CurrentHP <= 0.f;

	OnRep_CurrentHP();
	if (bIsDead)
	{
		OnRep_IsDead();
	}
}

void ABG_PlayerState::OnRep_CurrentHP()
{
	// 이후 HP 바, 피격 이펙트 같은 클라이언트 반응을 여기서 처리할 수 있다.
	OnHealthChanged.Broadcast(CurrentHP, MaxHP, bIsDead);
}

void ABG_PlayerState::OnRep_IsDead()
{
	// 이후 사망 UI, 래그돌, 관전 전환 같은 클라이언트 로직을 여기서 연결할 수 있다.
	OnHealthChanged.Broadcast(CurrentHP, MaxHP, bIsDead);
}

void ABG_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_PlayerState, CurrentHP);
	DOREPLIFETIME(ABG_PlayerState, bIsDead);
}
