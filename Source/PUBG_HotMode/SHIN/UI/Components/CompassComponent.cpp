#include "CompassComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UCompassComponent::UCompassComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCompassComponent::BeginPlay()
{
    Super::BeginPlay();

    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassComponent::BeginPlay - Owner is not a Pawn on %s."), *GetNameSafe(GetOwner()));
        return;
    }

    if (bUseControlRotation && !OwnerPawn->GetController())
    {
        UE_LOG(LogTemp, Warning, TEXT("UCompassComponent::BeginPlay - Pawn %s has no Controller yet."), *GetNameSafe(OwnerPawn));
    }
}

float UCompassComponent::GetCompassYaw() const
{
    return GetNormalizedControlYaw();
}

float UCompassComponent::GetNormalizedControlYaw() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!OwnerPawn)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassComponent::GetNormalizedControlYaw - Owner is not a Pawn on %s."), *GetNameSafe(GetOwner()));
        return 0.0f;
    }

    if (bUseControlRotation)
    {
        const AController* Controller = OwnerPawn->GetController();
        if (!Controller)
        {
            UE_LOG(LogTemp, Error, TEXT("UCompassComponent::GetNormalizedControlYaw - Controller is null for %s."), *GetNameSafe(OwnerPawn));
            return 0.0f;
        }

        return Normalize360(Controller->GetControlRotation().Yaw);
    }

    return Normalize360(OwnerPawn->GetActorRotation().Yaw);
}

float UCompassComponent::GetDeltaToWorldYaw(float TargetWorldYaw) const
{
    const float CurrentYaw = GetCompassYaw();
    return FMath::FindDeltaAngleDegrees(CurrentYaw, Normalize360(TargetWorldYaw));
}

float UCompassComponent::GetDeltaToWorldLocation(const FVector& WorldLocation) const
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        UE_LOG(LogTemp, Error, TEXT("UCompassComponent::GetDeltaToWorldLocation - Owner is null."));
        return 0.0f;
    }

    const FVector ToTarget = WorldLocation - OwnerActor->GetActorLocation();
    if (ToTarget.IsNearlyZero())
    {
        UE_LOG(LogTemp, Warning, TEXT("UCompassComponent::GetDeltaToWorldLocation - Target location is too close to owner %s."), *GetNameSafe(OwnerActor));
        return 0.0f;
    }

    const float TargetYaw = ToTarget.Rotation().Yaw;
    return FMath::FindDeltaAngleDegrees(GetCompassYaw(), Normalize360(TargetYaw));
}

float UCompassComponent::Normalize360(float Degree)
{
    float Result = FMath::Fmod(Degree, 360.0f);
    if (Result < 0.0f)
    {
        Result += 360.0f;
    }
    return Result;
}