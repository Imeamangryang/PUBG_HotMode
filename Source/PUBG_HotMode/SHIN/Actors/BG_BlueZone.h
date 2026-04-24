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

private:

	/* ===== Mesh ===== */
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ZoneMesh;

	UPROPERTY()
	UMaterialInstanceDynamic* ZoneMID;

	/* ===== Zone Data ===== */
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

	/* ===== Cylinder Height ===== */
	UPROPERTY(EditAnywhere, Category="Zone")
	float CylinderHalfHeight = 25000.f;

	/* ===== State ===== */
	UPROPERTY()
	EZoneState ZoneState = EZoneState::Waiting;

	float ElapsedTime = 0.f;

private:

	void UpdateZone(float DeltaTime);
	void StartShrink();
	float GetShrinkAlpha() const;

	void UpdateVisuals();
	void CheckPlayersOutside(); // TODO
};