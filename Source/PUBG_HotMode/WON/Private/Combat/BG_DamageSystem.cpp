// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BG_DamageSystem.h"
#include "GameFramework/Pawn.h"
#include "Player/BG_PlayerState.h"


UBG_DamageSystem::UBG_DamageSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UBG_DamageSystem::ApplyDamageToActor(AActor* TargetActor, float DamageAmount) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(TargetActor) || DamageAmount <= 0.f)
	{
		return false;
	}

	const APawn* TargetPawn = Cast<APawn>(TargetActor);

	if (!TargetPawn)
	{
		return false;
	}

	ABG_PlayerState* TargetPlayerState = TargetPawn->GetPlayerState<ABG_PlayerState>();
	return TargetPlayerState ? TargetPlayerState->ApplyDamage(DamageAmount) : false;
}
