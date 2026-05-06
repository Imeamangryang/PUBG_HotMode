#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BG_Airplane.generated.h"

UCLASS()
class PUBG_HOTMODE_API ABG_Airplane : public AActor
{
	GENERATED_BODY()

public:
	ABG_Airplane();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Airplane")
	void InitializeFlightPath(const FVector& InCircleCenter, float InCircleRadius, float InFlightAngleDegrees);

	UFUNCTION(BlueprintCallable, Category = "BG|Airplane")
	void StartFlight();

	UFUNCTION(BlueprintCallable, Category = "BG|Airplane")
	void StopFlight();

	UFUNCTION(BlueprintPure, Category = "BG|Airplane")
	FVector GetStartLocation() const { return StartLocation; }

	UFUNCTION(BlueprintPure, Category = "BG|Airplane")
	FVector GetEndLocation() const { return EndLocation; }

	UFUNCTION(BlueprintPure, Category = "BG|Airplane")
	FVector GetFlightDirection() const { return FlightDirection; }

	UFUNCTION(BlueprintPure, Category = "BG|Airplane")
	bool IsFlying() const { return bIsFlying; }

protected:
	void UpdateFlightPath();
	void FinishFlight();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Airplane")
	TObjectPtr<class USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Airplane")
	TObjectPtr<class UStaticMeshComponent> AirplaneMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Path")
	FVector CircleCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Path", meta = (ClampMin = "0.0"))
	float CircleRadius = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Path")
	float FlightAngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Path", meta = (ClampMin = "0.0"))
	float StartEndPadding = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Movement", meta = (ClampMin = "0.0"))
	float FlightSpeed = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Movement", meta = (ClampMin = "0.0"))
	float ArrivalTolerance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Airplane|Debug")
	bool bDrawDebugPath = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "BG|Airplane|Runtime")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "BG|Airplane|Runtime")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "BG|Airplane|Runtime")
	FVector FlightDirection = FVector::ForwardVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "BG|Airplane|Runtime")
	bool bIsFlying = false;
};