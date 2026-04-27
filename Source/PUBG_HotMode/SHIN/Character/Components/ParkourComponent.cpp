#include "ParkourComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

UParkourComponent::UParkourComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void UParkourComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UParkourComponent, ParkourState);
    DOREPLIFETIME(UParkourComponent, TargetLocation);
}

void UParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bParkourMoving || ParkourState == EParkourState::None || !OwnerCharacter)
    {
        return;
    }

    UpdateParkourMovement(DeltaTime);
}

void UParkourComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        Movement = OwnerCharacter->GetCharacterMovement();
    }
}

void UParkourComponent::TryParkour()
{
    if (ParkourState != EParkourState::None || !OwnerCharacter) return;

    FHitResult Hit;
    float Height;
    bool bIsThin;
    FVector TopLocation;

    // 1. 장애물 감지
    if (DetectObstacle(Hit, Height, bIsThin, TopLocation))
    {
        EParkourState DesiredState = EParkourState::None;
        FVector CalculatedTarget = FVector::ZeroVector;

        // [Logic] DecideAndStart의 내용을 여기서 판단
        if (Height <= HurdleThreshold && bIsThin)
        {
            DesiredState = EParkourState::Hurdle;
            CalculatedTarget = Hit.ImpactPoint + (OwnerCharacter->GetActorForwardVector() * 100.f);
            CalculatedTarget.Z = OwnerCharacter->GetActorLocation().Z;
        }
        else if (Height > HurdleThreshold && Height <= ClimbThreshold)
        {
            DesiredState = EParkourState::Climb;

            FVector TempTarget = TopLocation;
            TempTarget += OwnerCharacter->GetActorForwardVector() * 50.f;
            TempTarget.Z = TopLocation.Z + 5.f;
            CalculatedTarget = TempTarget;
        }

        // 2. 파쿠르 조건이 맞으면 서버에 RPC 요청
        if (DesiredState != EParkourState::None)
        {
            Server_StartParkour(DesiredState, CalculatedTarget);
            return; // 파쿠르 실행 시 함수 종료
        }
    }
    OwnerCharacter->Jump();
}

bool UParkourComponent::DetectObstacle(FHitResult& OutHit, float& OutHeight, bool& bOutThinObstacle, FVector& TopLocation) const
{
    if (!OwnerCharacter) return false;

    UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
    if (!Capsule) return false;

    const FVector ActorLocation = OwnerCharacter->GetActorLocation();
    const FVector Forward = OwnerCharacter->GetActorForwardVector();

    const float CapsuleRadius = DetectCapsuleRadius;
    const float CapsuleHalfHeight = DetectCapsuleHalfHeight;

    const FVector Start = ActorLocation + FVector(0.f, 0.f, CapsuleHalfHeight);
    const FVector End = Start + Forward * TraceDistance;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourCapsuleTrace), false, OwnerCharacter);
    FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

    const bool bHit = GetWorld()->SweepSingleByChannel(
        OutHit,
        Start,
        End,
        FQuat::Identity,
        ECC_Visibility,
        CapsuleShape,
        Params
    );

    if (!bHit || !OutHit.GetActor())
    {
        return false;
    }

    // =========================
    // 🔥 두께 측정 (Reverse Trace 방식)
    // =========================
    
    const float MaxCheckDistance = MaxObstacleThickness + 40.f;
    FVector ReverseStart = OutHit.ImpactPoint + Forward * MaxCheckDistance;
    FVector ReverseEnd = OutHit.ImpactPoint;

    FHitResult ReverseHit;
    bool bReverseHit = GetWorld()->LineTraceSingleByChannel(
        ReverseHit,
        ReverseStart,
        ReverseEnd,
        ECC_Visibility,
        Params
    );

    if (bReverseHit && ReverseHit.GetActor() == OutHit.GetActor())
    {
        const float Thickness = FVector::Distance(OutHit.ImpactPoint, ReverseHit.ImpactPoint);
        bOutThinObstacle = Thickness <= MaxObstacleThickness;
    }
    else
    {
        bOutThinObstacle = false;
    }
    
    // =========================
    // 🔥 높이 측정 (기존)
    // =========================

    FVector TopTraceStart = OutHit.ImpactPoint + FVector(0.f, 0.f, ClimbThreshold + 50.f);
    FVector TopTraceEnd   = OutHit.ImpactPoint - FVector(0.f, 0.f, 50.f);

    FHitResult TopHit;
    bool bTopHit = GetWorld()->LineTraceSingleByChannel(
        TopHit,
        TopTraceStart,
        TopTraceEnd,
        ECC_Visibility,
        Params
    );

    if (!bTopHit)
    {
        return false;
    }

    const float CharacterBaseZ = OwnerCharacter->GetActorLocation().Z;
    OutHeight = TopHit.ImpactPoint.Z - CharacterBaseZ;
    TopLocation = TopHit.ImpactPoint;

    return OutHeight > 0.f && OutHeight <= ClimbThreshold;
}

void UParkourComponent::EndParkour()
{
    // 로컬 클라이언트에서만 서버에 종료 요청 (중복 호출 방지)
    if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
    {
        Server_EndParkour();

        // 카메라 랙 (로컬 전용)
        if (USpringArmComponent* SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>())
        {
            SpringArm->bEnableCameraLag = true;
            TWeakObjectPtr<USpringArmComponent> WeakSpringArm = SpringArm;
            FTimerHandle TimerHandle;
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakSpringArm]()
            {
                if (WeakSpringArm.IsValid()) WeakSpringArm->bEnableCameraLag = false;
            }, 0.5f, false);
        }
    }
}

void UParkourComponent::Server_EndParkour_Implementation()
{
    FinishParkourMovement();
}

void UParkourComponent::Server_StartParkour_Implementation(EParkourState NewState, FVector NetTargetLocation)
{
    if (!OwnerCharacter || !Movement) return;

    ParkourState = NewState;
    TargetLocation = NetTargetLocation;

    Movement->DisableMovement();
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    StartParkourMovement();
    Multicast_PlayParkourMontage(NewState);
}

bool UParkourComponent::Server_StartParkour_Validate(EParkourState NewState, FVector NetTargetLocation)
{
    return true;
}

void UParkourComponent::Multicast_PlayParkourMontage_Implementation(EParkourState State)
{
    UAnimMontage* MontageToPlay = (State == EParkourState::Hurdle) ? HurdleMontage : ClimbMontage;
    USkeletalMeshComponent* BodyMesh = GetBodyMesh();

    if (BodyMesh && MontageToPlay)
    {
        UAnimInstance* AnimInst = BodyMesh->GetAnimInstance();
        if (AnimInst)
        {
            AnimInst->Montage_Play(MontageToPlay);
            UE_LOG(LogTemp, Warning, TEXT("Playing Montage on Body Mesh"));
        }
    }
}

USkeletalMeshComponent* UParkourComponent::GetBodyMesh() const
{
    if (!OwnerCharacter) return nullptr;
    
    USkeletalMeshComponent* BodyMesh = Cast<USkeletalMeshComponent>(
        OwnerCharacter->GetDefaultSubobjectByName(TEXT("Body"))
    );
    
    return BodyMesh;
}

void UParkourComponent::StartParkourMovement()
{
    if (!OwnerCharacter) return;

    ParkourStartLocation = OwnerCharacter->GetActorLocation();
    ParkourElapsedTime = 0.f;
    bParkourMoving = true;

    CurrentParkourDuration = (ParkourState == EParkourState::Hurdle)
        ? HurdleDuration
        : ClimbDuration;

    ParkourMidLocation = FVector(
        ParkourStartLocation.X,
        ParkourStartLocation.Y,
        TargetLocation.Z
    );
}

FVector UParkourComponent::CalculateHurdleLocation(float Alpha) const
{
    FVector BaseLocation = FMath::Lerp(ParkourStartLocation, TargetLocation, Alpha);

    const float ArcOffset = FMath::Sin(Alpha * PI) * HurdleArcHeight;
    BaseLocation.Z += ArcOffset;

    return BaseLocation;
}

FVector UParkourComponent::CalculateClimbLocation(float Alpha) const
{
    FVector NewLocation = ParkourStartLocation;

    if (Alpha < 0.75f)
    {
        const float VerticalAlpha = Alpha / 0.75f;
        NewLocation.Z = FMath::Lerp(ParkourStartLocation.Z, TargetLocation.Z, VerticalAlpha);
    }
    else
    {
        const float FinalAlpha = (Alpha - 0.75f) / 0.25f;
        NewLocation = FMath::Lerp(
            FVector(ParkourStartLocation.X, ParkourStartLocation.Y, TargetLocation.Z),
            TargetLocation,
            FinalAlpha
        );
    }

    return NewLocation;
}

void UParkourComponent::UpdateParkourMovement(float DeltaTime)
{
    if (!OwnerCharacter || CurrentParkourDuration <= 0.f)
    {
        FinishParkourMovement();
        return;
    }

    ParkourElapsedTime += DeltaTime;
    const float Alpha = FMath::Clamp(ParkourElapsedTime / CurrentParkourDuration, 0.f, 1.f);

    FVector NewLocation = OwnerCharacter->GetActorLocation();

    switch (ParkourState)
    {
    case EParkourState::Hurdle:
        NewLocation = CalculateHurdleLocation(Alpha);
        break;

    case EParkourState::Climb:
        NewLocation = CalculateClimbLocation(Alpha);
        break;

    default:
        break;
    }

    OwnerCharacter->SetActorLocation(NewLocation, false);

    if (Alpha >= 1.f)
    {
        FinishParkourMovement();
    }
}

void UParkourComponent::FinishParkourMovement()
{
    if (!OwnerCharacter || !Movement) return;

    bParkourMoving = false;
    OwnerCharacter->SetActorLocation(TargetLocation, false);
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Movement->SetMovementMode(MOVE_Walking);
    ParkourState = EParkourState::None;
}