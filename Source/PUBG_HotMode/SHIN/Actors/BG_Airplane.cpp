#include "BG_Airplane.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"

ABG_Airplane::ABG_Airplane()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootSceneComponent);

	AirplaneMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AirplaneMesh"));
	AirplaneMeshComponent->SetupAttachment(RootSceneComponent);
	AirplaneMeshComponent->SetVisibility(false);
}

void ABG_Airplane::BeginPlay()
{
	Super::BeginPlay();
}

void ABG_Airplane::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsFlying)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, EndLocation, DeltaSeconds, FlightSpeed);
	SetActorLocation(NewLocation);

	if (FVector::DistSquared(NewLocation, EndLocation) <= FMath::Square(ArrivalTolerance))
	{
		SetActorLocation(EndLocation);
		FinishFlight();
	}
}

void ABG_Airplane::InitializeFlightPath(const FVector& InCircleCenter, float InCircleRadius, float InFlightAngleDegrees)
{
	CircleCenter = InCircleCenter;
	CircleRadius = FMath::Max(0.0f, InCircleRadius);
	FlightAngleDegrees = InFlightAngleDegrees;

	UpdateFlightPath();
	SetActorLocation(StartLocation);
	SetActorRotation(FlightDirection.Rotation());

	if (bDrawDebugPath)
	{
		DrawDebugSphere(GetWorld(), CircleCenter, 300.0f, 16, FColor::Green, false, 10.0f, 0, 5.0f);
		DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Yellow, false, 10.0f, 0, 10.0f);
		DrawDebugSphere(GetWorld(), StartLocation, 250.0f, 12, FColor::Blue, false, 10.0f, 0, 3.0f);
		DrawDebugSphere(GetWorld(), EndLocation, 250.0f, 12, FColor::Red, false, 10.0f, 0, 3.0f);
	}
}

void ABG_Airplane::StartFlight()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsFlying)
	{
		return;
	}

	if (bUseRandomFlightAngle)
	{
		FlightAngleDegrees = FMath::FRandRange(0.0f, 360.0f);
	}

	UpdateFlightPath();
	SetActorLocation(StartLocation);
	SetActorRotation(FlightDirection.Rotation());

	if (bDrawDebugPath)
	{
		DrawDebugSphere(GetWorld(), CircleCenter, 300.0f, 16, FColor::Green, false, 10.0f, 0, 5.0f);
		DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Yellow, false, 10.0f, 0, 10.0f);
		DrawDebugSphere(GetWorld(), StartLocation, 250.0f, 12, FColor::Blue, false, 10.0f, 0, 3.0f);
		DrawDebugSphere(GetWorld(), EndLocation, 250.0f, 12, FColor::Red, false, 10.0f, 0, 3.0f);
	}

	bIsFlying = true;
	
	FlightStartTimeSeconds = GetWorld()->GetTimeSeconds();
}

void ABG_Airplane::StopFlight()
{
	bIsFlying = false;
}

void ABG_Airplane::UpdateFlightPath()
{
	const FRotator FlightRotator(0.0f, FlightAngleDegrees, 0.0f);
	FlightDirection = FlightRotator.Vector().GetSafeNormal();

	const float TravelExtent = CircleRadius + StartEndPadding;

	StartLocation = CircleCenter - (FlightDirection * TravelExtent);
	EndLocation = CircleCenter + (FlightDirection * TravelExtent);
}

void ABG_Airplane::FinishFlight()
{
	bIsFlying = false;
}