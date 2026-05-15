#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BG_AirplaneCameraRig.generated.h"

class ABG_Airplane;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UAudioComponent;

UCLASS()
class PUBG_HOTMODE_API ABG_AirplaneCameraRig : public AActor
{
	GENERATED_BODY()

public:
	ABG_AirplaneCameraRig();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void Initialize(ABG_Airplane* InTargetAirplane);
	void AddLookInput(const FVector2D& LookInput);

	UFUNCTION(BlueprintPure, Category = "BG|AirplaneCamera")
	ABG_Airplane* GetTargetAirplane() const { return TargetAirplane.Get(); }
	
	void InitializeFlightSnapshot(class ABG_Airplane* InTargetAirplane, const FVector& InStartLocation, const FVector& InEndLocation, float InFlightSpeed, float InFlightStartTimeSeconds);

protected:
	void UpdateFollowTransform();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	TObjectPtr<USceneComponent> OrbitPivotComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	TObjectPtr<USpringArmComponent> SpringArmComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	TObjectPtr<UCameraComponent> CameraComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	TObjectPtr<UStaticMeshComponent> VisualAirplaneMeshComponent;
	
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AirplaneSoundComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	float LookYawSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	float LookPitchSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	float MinPitch = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	float MaxPitch = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|AirplaneCamera")
	float FollowLocationInterpSpeed = 8.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABG_Airplane> TargetAirplane;

	UPROPERTY(Transient)
	float CurrentYaw = 0.0f;

	UPROPERTY(Transient)
	float CurrentPitch = -45.0f;
	
	UPROPERTY(Transient)
	FVector FlightStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector FlightEndLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float FlightSpeed = 0.0f;

	UPROPERTY(Transient)
	float FlightStartTimeSeconds = 0.0f;

	UPROPERTY(Transient)
	bool bUseIndependentFlightMotion = false;
};