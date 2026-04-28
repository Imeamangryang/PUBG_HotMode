// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BG_DamageSystem.h"
#include "Combat/BG_HealthComponent.h"


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

	UBG_HealthComponent* HealthComponent = UBG_HealthComponent::FindHealthComponent(TargetActor);
	if (!HealthComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ApplyDamageToActor failed because target had no HealthComponent. Target=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor));
		return false;
	}

	return HealthComponent->ApplyDamage(DamageAmount);
}
