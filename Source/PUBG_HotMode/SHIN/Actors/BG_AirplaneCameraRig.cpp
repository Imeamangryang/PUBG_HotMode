#include "BG_AirplaneCameraRig.h"
#include "BG_Airplane.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"

ABG_AirplaneCameraRig::ABG_AirplaneCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootSceneComponent);

	OrbitPivotComponent = CreateDefaultSubobject<USceneComponent>(TEXT("OrbitPivot"));
	OrbitPivotComponent->SetRelativeLocation(FVector(-500.0f, 0.0f, 180.0f));
	OrbitPivotComponent->SetupAttachment(RootSceneComponent);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArmComponent->SetupAttachment(OrbitPivotComponent);
	SpringArmComponent->SocketOffset = FVector(0.0f, 0.0f, 650.0f);
	SpringArmComponent->TargetArmLength = 2800.0f;
	SpringArmComponent->bDoCollisionTest = false;
	SpringArmComponent->bEnableCameraLag = true;
	SpringArmComponent->CameraLagSpeed = 5.0f;
	SpringArmComponent->SetRelativeRotation(FRotator::ZeroRotator);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	
	VisualAirplaneMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualAirplaneMesh"));
	VisualAirplaneMeshComponent->SetupAttachment(RootSceneComponent);
	
	ConstructorHelpers::FObjectFinder<UStaticMesh> AirplaneMeshAsset(TEXT("/Game/SHIN/Assets/Models/Aircraft/SM_Aircraft.SM_Aircraft"));
	if (AirplaneMeshAsset.Succeeded())
	{
		VisualAirplaneMeshComponent->SetStaticMesh(AirplaneMeshAsset.Object);
		VisualAirplaneMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		VisualAirplaneMeshComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}
}

void ABG_AirplaneCameraRig::BeginPlay()
{
	Super::BeginPlay();
}

void ABG_AirplaneCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFollowTransform();
}

void ABG_AirplaneCameraRig::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AirplaneSoundComponent)
	{
		AirplaneSoundComponent->Stop();
		AirplaneSoundComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ABG_AirplaneCameraRig::Initialize(ABG_Airplane* InTargetAirplane)
{
	TargetAirplane = InTargetAirplane;

	if (InTargetAirplane)
	{
		FlightStartLocation = InTargetAirplane->GetStartLocation();
		FlightEndLocation = InTargetAirplane->GetEndLocation();
		FlightSpeed = InTargetAirplane->GetFlightSpeed();
		FlightStartTimeSeconds = InTargetAirplane->GetFlightStartTimeSeconds();
		bUseIndependentFlightMotion = true;
		
		const FVector Direction = (FlightEndLocation - FlightStartLocation).GetSafeNormal();
		SetActorRotation(Direction.Rotation());

		SetActorLocation(FlightStartLocation);
	}

	OrbitPivotComponent->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));
	
	if (!AirplaneSoundComponent)
	{
		if (USoundBase* BeginPlaySound = LoadObject<USoundBase>(nullptr, TEXT("/Game/WON/SFX/Airplane.Airplane")))
		{
			AirplaneSoundComponent = UGameplayStatics::SpawnSound2D(this, BeginPlaySound);
		}
	}
}

void ABG_AirplaneCameraRig::AddLookInput(const FVector2D& LookInput)
{
	CurrentYaw += LookInput.X * LookYawSpeed;
	CurrentPitch = FMath::Clamp(CurrentPitch + (LookInput.Y * LookPitchSpeed), MinPitch, MaxPitch);

	if (OrbitPivotComponent)
	{
		OrbitPivotComponent->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));
	}
}

void ABG_AirplaneCameraRig::UpdateFollowTransform()
{
	if (!bUseIndependentFlightMotion)
	{
		return;
	}

	const float TotalDistance = FVector::Distance(FlightStartLocation, FlightEndLocation);
	if (TotalDistance <= KINDA_SMALL_NUMBER)
	{
		SetActorLocation(FlightEndLocation);
		return;
	}

	const float CurrentWorldTime = GetWorld()->GetTimeSeconds();
	const float ElapsedTime = FMath::Max(0.0f, CurrentWorldTime - FlightStartTimeSeconds);
	const float TravelDistance = ElapsedTime * FlightSpeed;
	const float Alpha = FMath::Clamp(TravelDistance / TotalDistance, 0.0f, 1.0f);

	const FVector NewLocation = FMath::Lerp(FlightStartLocation, FlightEndLocation, Alpha);
	SetActorLocation(NewLocation);
}

void ABG_AirplaneCameraRig::InitializeFlightSnapshot(
	ABG_Airplane* InTargetAirplane,
	const FVector& InStartLocation,
	const FVector& InEndLocation,
	float InFlightSpeed,
	float InFlightStartTimeSeconds
)
{
	TargetAirplane = InTargetAirplane;
	FlightStartLocation = InStartLocation;
	FlightEndLocation = InEndLocation;
	FlightSpeed = InFlightSpeed;
	FlightStartTimeSeconds = InFlightStartTimeSeconds;
	bUseIndependentFlightMotion = true;

	SetActorLocation(FlightStartLocation);

	const FVector Direction = (FlightEndLocation - FlightStartLocation).GetSafeNormal();
	SetActorRotation(Direction.Rotation());

	if (OrbitPivotComponent)
	{
		OrbitPivotComponent->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));
	}
}