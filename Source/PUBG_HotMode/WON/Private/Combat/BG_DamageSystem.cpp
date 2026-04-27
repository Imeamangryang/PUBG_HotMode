// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BG_DamageSystem.h"
#include "Combat/BG_HealthComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/BG_PlayerState.h"


UBG_DamageSystem::UBG_DamageSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UBG_DamageSystem::ApplyDamageToActor(AActor* TargetActor, float DamageAmount) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamageToActor failed because owner was null or had no authority."), *GetNameSafe(this));
		return false;
	}

	if (!IsValid(TargetActor) || DamageAmount <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamageToActor failed because TargetActor was invalid or DamageAmount was not positive."), *GetNameSafe(this));
		return false;
	}

	if (UBG_HealthComponent* HealthComponent = TargetActor->FindComponentByClass<UBG_HealthComponent>())
	{
		return HealthComponent->ApplyDamage(DamageAmount);
	}

	const APawn* TargetPawn = Cast<APawn>(TargetActor);

	if (!TargetPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamageToActor could not find a health component and target was not a pawn. Target=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor));
		return false;
	}

	ABG_PlayerState* TargetPlayerState = TargetPawn->GetPlayerState<ABG_PlayerState>();
	if (!TargetPlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamageToActor failed because TargetPlayerState was null. Target=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor));
		return false;
	}

	return TargetPlayerState->ApplyDamage(DamageAmount);
}
