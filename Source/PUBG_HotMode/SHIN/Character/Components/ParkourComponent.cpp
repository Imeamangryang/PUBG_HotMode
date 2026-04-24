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
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UParkourComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(UParkourComponent, ParkourState);
    DOREPLIFETIME(UParkourComponent, TargetLocation);
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
        if (Height >= HurdleThreshold)
        {
            // 높은 벽 -> 클라임
            DesiredState = EParkourState::Climb;
            FVector TempTarget = Hit.ImpactPoint;
            TempTarget.Z = TopLocation.Z + 50.f;
            CalculatedTarget = TempTarget + (OwnerCharacter->GetActorForwardVector() * 60.f);
        }
        else if (Height < HurdleThreshold && bIsThin)
        {
            // 낮고 얇은 벽 -> 허들
            DesiredState = EParkourState::Hurdle;
            CalculatedTarget = Hit.ImpactPoint + (OwnerCharacter->GetActorForwardVector() * 80.f);
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
    if (!OwnerCharacter || !Movement) return;

    OwnerCharacter->SetActorLocation(TargetLocation);
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Movement->SetMovementMode(MOVE_Walking);
    ParkourState = EParkourState::None;
}

void UParkourComponent::Server_StartParkour_Implementation(EParkourState NewState, FVector NetTargetLocation)
{
    ParkourState = NewState;
    TargetLocation = NetTargetLocation;

    if (Movement) Movement->DisableMovement();
    OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 모든 클라이언트에게 몽타주 재생 명령
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
