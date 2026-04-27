#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "ParkourComponent.generated.h"

UENUM()
enum class EParkourState : uint8
{
	None,
	Hurdle,
	Climb
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UParkourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UParkourComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category="Parkour|Animation")
	void TryParkour();

	UFUNCTION(BlueprintCallable, Category="Parkour|Animation")
	void EndParkour();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StartParkour(EParkourState NewState, FVector NetTargetLocation);

	UFUNCTION(Server, Reliable)
	void Server_EndParkour();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayParkourMontage(EParkourState State);

	USkeletalMeshComponent* GetBodyMesh() const;

private:
	bool DetectObstacle(FHitResult& OutHit, float& OutHeight, bool& bOutThinObstacle, FVector& OutTopLocation) const;

	void StartParkourMovement();
	void UpdateParkourMovement(float DeltaTime);
	void FinishParkourMovement();

	FVector CalculateHurdleLocation(float Alpha) const;
	FVector CalculateClimbLocation(float Alpha) const;

private:
	UPROPERTY()
	class ACharacter* OwnerCharacter;

	UPROPERTY()
	class UCharacterMovementComponent* Movement;

	UPROPERTY(EditAnywhere, Category="Parkour|Detect")
	float TraceDistance = 120.f;

	UPROPERTY(EditAnywhere, Category="Parkour|Detect")
	float HurdleThreshold = 80.f;

	UPROPERTY(EditAnywhere, Category="Parkour|Detect")
	float ClimbThreshold = 200.f;

	UPROPERTY(EditAnywhere, Category="Parkour|Detect")
	float DetectCapsuleRadius = 24.f;

	UPROPERTY(EditAnywhere, Category="Parkour|Detect")
	float DetectCapsuleHalfHeight = 48.f;

	UPROPERTY(EditAnywhere, Category="Parkour|Detect")
	float MaxObstacleThickness = 100.f;

	UPROPERTY(EditAnywhere, Category="Parkour|Move")
	float HurdleDuration = 1.4f;

	UPROPERTY(EditAnywhere, Category="Parkour|Move")
	float ClimbDuration = 2.8f;

	UPROPERTY(EditAnywhere, Category="Parkour|Move")
	float HurdleArcHeight = 60.f;

	UPROPERTY(Replicated)
	FVector TargetLocation;

	UPROPERTY(Replicated)
	EParkourState ParkourState = EParkourState::None;

	UPROPERTY(EditAnywhere, Category="Parkour|Animation")
	class UAnimMontage* HurdleMontage;

	UPROPERTY(EditAnywhere, Category="Parkour|Animation")
	class UAnimMontage* ClimbMontage;

	bool bParkourMoving = false;
	float ParkourElapsedTime = 0.f;
	float CurrentParkourDuration = 0.f;
	FVector ParkourStartLocation = FVector::ZeroVector;
	FVector ParkourMidLocation = FVector::ZeroVector;
};