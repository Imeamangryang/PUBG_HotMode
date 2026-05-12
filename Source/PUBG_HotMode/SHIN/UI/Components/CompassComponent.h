#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompassComponent.generated.h"

UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UCompassComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompassComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Compass")
	float GetCompassYaw() const;

	UFUNCTION(BlueprintPure, Category="Compass")
	float GetNormalizedControlYaw() const;

	UFUNCTION(BlueprintPure, Category="Compass")
	float GetDeltaToWorldYaw(float TargetWorldYaw) const;

	UFUNCTION(BlueprintPure, Category="Compass")
	float GetDeltaToWorldLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category="Compass")
	static float Normalize360(float Degree);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compass")
	bool bUseControlRotation = true;
};