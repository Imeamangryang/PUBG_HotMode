#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BG_BlueZone.generated.h"

UENUM()
enum class EZoneState : uint8
{
	Waiting,
	Shrinking
};

UCLASS()
class PUBG_HOTMODE_API ABG_BlueZone : public AActor
{
	GENERATED_BODY()

public:
	ABG_BlueZone();

protected:
	virtual void BeginPlay() override;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Zone")
	void SetZoneActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category="Zone")
	bool IsZoneActive() const { return bIsActive; }

private:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ZoneMesh;

	UPROPERTY()
	UMaterialInstanceDynamic* ZoneMID;

	UPROPERTY()
	FVector ZoneCenter;

	UPROPERTY()
	float CurrentRadius;

	UPROPERTY(EditAnywhere, Category="Zone")
	float InitialRadius = 5000.f;

	UPROPERTY(EditAnywhere, Category="Zone")
	float WaitTime = 180.f;

	UPROPERTY(EditAnywhere, Category="Zone")
	float ShrinkTime = 60.f;

	UPROPERTY(EditAnywhere, Category="Zone")
	float MinRadius = 50.f;

	UPROPERTY(EditAnywhere, Category="Zone")
	float CylinderHalfHeight = 25000.f;

	UPROPERTY()
	EZoneState ZoneState = EZoneState::Waiting;

	UPROPERTY(VisibleInstanceOnly, Category="Zone")
	bool bIsActive = false;

	UPROPERTY(EditDefaultsOnly, Category="Zone|Damage", meta=(ClampMin="0.1"))
	float DamageTickInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Zone|Damage", meta=(ClampMin="0.0"))
	float DamagePerTick = 5.0f;

	float ElapsedTime = 0.f;

	FTimerHandle DamageTimerHandle;

private:
	void UpdateZone(float DeltaTime);
	void StartShrink();
	float GetShrinkAlpha() const;

	void UpdateVisuals();
	void CheckPlayersOutside();
	void ApplyBlueZoneDamageTick();
};