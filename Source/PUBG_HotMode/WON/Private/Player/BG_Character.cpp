#include "Player/BG_Character.h"
#include "Combat/BG_DamageSystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogBGCharacter);

ABG_Character::ABG_Character()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

    DamageSystem = CreateDefaultSubobject<UBG_DamageSystem>(TEXT("DamageSystem"));
}

void ABG_Character::BeginPlay()
{
    Super::BeginPlay();

    // 변수명을 Children에서 ChildComps로 변경하여 충돌 방지
    if (USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(GetDefaultSubobjectByName(TEXT("Body"))))
    {
        TArray<USceneComponent*> ChildComps;
        Body->GetChildrenComponents(true, ChildComps);
        
        for (USceneComponent* Child : ChildComps)
        {
            if (USkeletalMeshComponent* SkeletalChild = Cast<USkeletalMeshComponent>(Child))
            {
                // 대장(Body)의 애니메이션을 따라하게 함
                SkeletalChild->SetLeaderPoseComponent(Body);
            }
        }
    }
}

void ABG_Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // 바인딩은 BG_PlayerController에서 수행하므로 비워둠
}

void ABG_Character::MoveFromInput(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    if (Controller && !MovementVector.IsNearlyZero())
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void ABG_Character::LookFromInput(const FInputActionValue& Value)
{
    const FVector2D LookAxisVector = Value.Get<FVector2D>();
    if (Controller)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void ABG_Character::StartJumpFromInput() { Jump(); }
void ABG_Character::StopJumpFromInput() { StopJumping(); }

void ABG_Character::Req_PrimaryAction()
{
    if (bIsWeaponEquipped) return;
    if (GetWorld()->TimeSeconds - LastMeleeAttackTime < MeleeAttackCooldown) return;

    if (HasAuthority()) Server_ExecuteMeleeAttack_Implementation();
    else Server_ExecuteMeleeAttack();
}

void ABG_Character::Server_ExecuteMeleeAttack_Implementation()
{
    if (GetWorld()->TimeSeconds - LastMeleeAttackTime < MeleeAttackCooldown) return;
    LastMeleeAttackTime = GetWorld()->TimeSeconds;

    ProcessMeleeHitDetection();
    Multicast_PlayAttackEffects();
}

bool ABG_Character::Server_ExecuteMeleeAttack_Validate() { return true; }

void ABG_Character::Multicast_PlayAttackEffects_Implementation()
{
    // 'Body' 메쉬의 AnimInstance를 가져와 몽타주 재생
    if (USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(GetDefaultSubobjectByName(TEXT("Body"))))
    {
        if (MeleePunchMontage && Body->GetAnimInstance())
        {
            Body->GetAnimInstance()->Montage_Play(MeleePunchMontage);
        }
    }
}

void ABG_Character::ProcessMeleeHitDetection()
{
    if (!DamageSystem) return;
    const FVector Start = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight * 0.5f);
    const FVector End = Start + (GetActorForwardVector() * MeleeRange);

    TArray<FHitResult> HitResults;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeAttack), false, this);
    if (GetWorld()->SweepMultiByChannel(HitResults, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(MeleeRadius), Params))
    {
        TSet<AActor*> HitActors;
        for (auto& Hit : HitResults)
        {
            if (AActor* Target = Hit.GetActor())
            {
                if (Target != this && !HitActors.Contains(Target))
                {
                    HitActors.Add(Target);
                    DamageSystem->ApplyDamageToActor(Target, MeleeDamage);
                }
            }
        }
    }
}