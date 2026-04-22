#include "ParkourComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/SpringArmComponent.h"

UParkourComponent::UParkourComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
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
    UE_LOG(LogTemp, Warning, TEXT("TryParkour Called"));

    if (ParkourState != EParkourState::None) return;
    if (!OwnerCharacter) return;

    FHitResult Hit;
    float Height;
    bool bIsThin;
    FVector TopLocation;

    if (DetectObstacle(Hit, Height, bIsThin, TopLocation))
    {
        DecideAndStart(Hit, Height, bIsThin, TopLocation);
    }
}

bool UParkourComponent::DetectObstacle(FHitResult& OutHit, float& OutHeight, bool& bOutThinObstacle, FVector& TopLocation) const
{
    FVector Start = OwnerCharacter->GetActorLocation();
    FVector Forward = OwnerCharacter->GetActorForwardVector();

    // 1️⃣ Forward Trace
    FVector End = Start + Forward * TraceDistance;

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        OutHit,
        Start,
        End,
        ECC_Visibility
    );

    if (!bHit) return false;

    // =========================
    // 🔥 두께 측정 (Reverse Trace 방식)
    // =========================
    
    float MaxCheckDistance = 200.f; // 최대 탐색 거리 (이보다 두꺼우면 Hurdle 불가)
    float ThicknessThreshold = 100.f; // 얇은 장애물 기준치
    
    // 앞에서 맞은 지점에서 최대 탐색 거리만큼 뚫고 지나간 뒤쪽 위치
    FVector ReverseStart = OutHit.ImpactPoint + (Forward * MaxCheckDistance);
    
    // 뒤에서 원래 맞았던 앞면(ImpactPoint)을 향해 거꾸로 쏩니다.
    FVector ReverseEnd = OutHit.ImpactPoint;

    FHitResult ReverseHit;
    bool bReverseHit = GetWorld()->LineTraceSingleByChannel(
        ReverseHit,
        ReverseStart,
        ReverseEnd,
        ECC_Visibility
    );

    // 거꾸로 쏴서 맞은 액터가 처음에 맞은 액터와 동일한지 확인
    if (bReverseHit && ReverseHit.GetActor() == OutHit.GetActor())
    {
        // 뒷면을 맞췄다면, 앞면과 뒷면 사이의 거리가 곧 '두께'가 됩니다.
        float Thickness = FVector::Distance(OutHit.ImpactPoint, ReverseHit.ImpactPoint);
        bOutThinObstacle = (Thickness <= ThicknessThreshold);
    }
    else
    {
        // 최대 거리(200) 안에서 뒷면을 찾지 못했다면 너무 두꺼운 벽입니다.
        bOutThinObstacle = false;
    }

    // =========================
    // 🔥 높이 측정 (기존)
    // =========================

    FVector TopTraceStart = OutHit.ImpactPoint + FVector(0, 0, 180.f);
    FVector TopTraceEnd   = OutHit.ImpactPoint - FVector(0, 0, 180.f);

    FHitResult TopHit;
    bool bTopHit = GetWorld()->LineTraceSingleByChannel(
        TopHit,
        TopTraceStart,
        TopTraceEnd,
        ECC_Visibility
    );

    if (!bTopHit) return false;

    float CharacterBaseZ = OwnerCharacter->GetActorLocation().Z;
    OutHeight = TopHit.ImpactPoint.Z - CharacterBaseZ;
    
    TopLocation = TopHit.ImpactPoint;

    return true;
}

void UParkourComponent::DecideAndStart(const FHitResult& Hit, float Height, bool bIsThin, const FVector& TopLocation)
{
    if (Height < HurdleThreshold)
    {
        // 낮은데 얇기까지 하면 허들 실행
        if (bIsThin)
        {
            StartHurdle(Hit);
        }
        else
        {
            // 낮지만 두꺼운 경우: 그냥 벽에 막힌 상태 -> 점프 실행
            UE_LOG(LogTemp, Warning, TEXT("Low but Thick -> No Parkour"));
        }
    }
    // 2. 높이가 높은 경우 (클라임 범위)
    else
    {
        // 클라임은 두께와 상관없이 높이만 되면 실행
        StartClimb(Hit, TopLocation);
    }
}

void UParkourComponent::StartHurdle(const FHitResult& Hit)
{
    if (!Movement || !HurdleMontage) return;

    USkeletalMeshComponent* BodyMesh = Cast<USkeletalMeshComponent>(
        OwnerCharacter->GetDefaultSubobjectByName(TEXT("Body"))
    );

    if (!BodyMesh) return;

    UE_LOG(LogTemp, Warning, TEXT("Start Hurdle"));

    ParkourState = EParkourState::Hurdle;

    Movement->DisableMovement();
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 🔥 목표 위치 계산 (장애물 "뒤쪽"으로 이동)
    FVector Forward = OwnerCharacter->GetActorForwardVector();

    TargetLocation = Hit.ImpactPoint
        + Forward * 80.f   // 장애물 넘어가기
        + FVector(0, 0, 30.f); // 살짝 띄우기

    BodyMesh->GetAnimInstance()->Montage_Play(HurdleMontage);
}

void UParkourComponent::StartClimb(const FHitResult& Hit, const FVector& TopLocation)
{
    if (!Movement || !ClimbMontage) return;

    USkeletalMeshComponent* BodyMesh = Cast<USkeletalMeshComponent>(
        OwnerCharacter->GetDefaultSubobjectByName(TEXT("Body"))
    );

    if (!BodyMesh) return;

    UE_LOG(LogTemp, Warning, TEXT("Start Climb"));

    ParkourState = EParkourState::Climb;

    Movement->DisableMovement();
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FVector Forward = OwnerCharacter->GetActorForwardVector();

    // 🔥 XY는 벽 위치 기준
    FVector Target = Hit.ImpactPoint;

    // 🔥 Z는 정확한 Top 높이 사용
    Target.Z = TopLocation.Z;
    
    Target.Z += 50;

    // 살짝 앞으로
    Target += Forward * 60.f;

    TargetLocation = Target;

    BodyMesh->GetAnimInstance()->Montage_Play(ClimbMontage);
}

void UParkourComponent::EndParkour()
{
    if (!OwnerCharacter || !Movement) return;

    UE_LOG(LogTemp, Warning, TEXT("End Parkour"));

    // 1. 스프링 암 가져오기 및 안전 검사
    USpringArmComponent* SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
    
    if (SpringArm)
    {
        // 위치 이동 직전에 랙(Lag) 활성화
        SpringArm->bEnableCameraLag = true;
        SpringArm->CameraLagSpeed = 3.0f; // 낮을수록 부드러움
    }

    // 2. 캐릭터 위치 이동 (순간이동)
    OwnerCharacter->SetActorLocation(TargetLocation);

    // 3. 충돌 및 상태 복구
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ParkourState = EParkourState::None;
    Movement->SetMovementMode(MOVE_Walking);

    // 4. 안전한 타이머 처리 (람다 캡처 주의)
    if (SpringArm)
    {
        // 0.5초 뒤에 카메라 랙을 다시 끕니다. 
        // 캐릭터가 파괴되었을 경우를 대비해 TWeakObjectPtr를 사용하면 더 안전합니다.
        TWeakObjectPtr<USpringArmComponent> WeakSpringArm = SpringArm;
        
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [WeakSpringArm]()
        {
            if (WeakSpringArm.IsValid())
            {
                WeakSpringArm->bEnableCameraLag = false;
            }
        }, 0.5f, false);
    }
}