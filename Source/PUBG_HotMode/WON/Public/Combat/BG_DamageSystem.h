// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BG_DamageSystem.generated.h"

class UBG_HealthComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_DamageSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_DamageSystem();

	// 캐릭터가 직접 HP를 깎지 않고 컴포넌트에 위임해 전투 규칙을 한곳에 모은다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool ApplyDamageToActor(AActor* TargetActor, float DamageAmount) const;
};
