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

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category="Parkour|Animation")
	void TryParkour();
	
	UFUNCTION(BlueprintCallable, Category="Parkour|Animation")
	void EndParkour();

private:
	bool DetectObstacle(FHitResult& OutHit, float& OutHeight, bool& bOutThinObstacle, FVector& OutTopLocation) const;
	void DecideAndStart(const FHitResult& Hit, float Height, bool bIsThin, const FVector& TopLocation);

	void StartHurdle(const FHitResult& Hit);
	void StartClimb(const FHitResult& Hit, const FVector& TopLocation);

private:
	UPROPERTY()
	class ACharacter* OwnerCharacter;

	UPROPERTY()
	class UCharacterMovementComponent* Movement;

	UPROPERTY(EditAnywhere, Category="Parkour")
	float TraceDistance = 100.f;

	UPROPERTY(EditAnywhere, Category="Parkour")
	float HurdleThreshold = 80.f;

	UPROPERTY(EditAnywhere, Category="Parkour")
	float ClimbThreshold = 200.f;
	
	UPROPERTY()
	FVector TargetLocation;

	UPROPERTY(EditAnywhere, Category="Parkour|Animation")
	class UAnimMontage* HurdleMontage;

	UPROPERTY(EditAnywhere, Category="Parkour|Animation")
	class UAnimMontage* ClimbMontage;

	EParkourState ParkourState = EParkourState::None;
};