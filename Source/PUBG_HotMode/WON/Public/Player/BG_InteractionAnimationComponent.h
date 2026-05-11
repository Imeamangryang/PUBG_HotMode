#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/BG_ItemTypes.h"
#include "BG_InteractionAnimationComponent.generated.h"

class ABG_Character;
class UAnimMontage;

UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_InteractionAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_InteractionAnimationComponent();

	UFUNCTION(BlueprintCallable, Category = "Animation|Interaction")
	void PlayPickupMontage(EBG_ItemType ItemType);

	UFUNCTION(BlueprintPure, Category = "Animation|Interaction")
	float GetPickupMontageDuration(EBG_ItemType ItemType) const;

protected:
	virtual void BeginPlay() override;

private:
	UAnimMontage* ResolvePickupMontage(EBG_ItemType ItemType) const;
	float ResolvePickupMontagePlayRate(EBG_ItemType ItemType) const;

private:
	UPROPERTY()
	TObjectPtr<ABG_Character> CachedCharacter = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> DefaultPickupMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> WeaponPickupMontage = nullptr;
};
