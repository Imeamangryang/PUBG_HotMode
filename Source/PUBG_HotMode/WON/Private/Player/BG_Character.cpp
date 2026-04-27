#include "Player/BG_Character.h"
#include "Combat/BG_DamageSystem.h"
#include "Combat/BG_HealthComponent.h"
#include "Combat/BG_WeaponFireComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Character/Components/ParkourComponent.h"

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
	HealthComponent = CreateDefaultSubobject<UBG_HealthComponent>(TEXT("HealthComponent"));
	WeaponFireComponent = CreateDefaultSubobject<UBG_WeaponFireComponent>(TEXT("WeaponFireComponent"));

	WeaponAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponAttachPoint"));
	WeaponAttachPoint->SetupAttachment(GetMesh());
	
	// 파쿠르 컴포넌트
	ParkourComponent = CreateDefaultSubobject<UParkourComponent>(TEXT("ParkourComponent"));

	bIsWeaponEquipped = false;
	EquippedWeaponPoseType = EBGWeaponPoseType::None;
	bIsAiming = false;
	UpdateDerivedState();
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

void ABG_Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		bIsFalling = MovementComponent->IsFalling();
		bIsAcceleration = MovementComponent->GetCurrentAcceleration().Size() > 0.f;
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Tick failed because CharacterMovement was null."), *GetNameSafe(this));
		bIsFalling = false;
		bIsAcceleration = false; 
	}

	UpdateDerivedState();
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

void ABG_Character::StartJumpFromInput()
{
	if (ParkourComponent)
	{
		ParkourComponent -> TryParkour();
	}
	else
	{
		return;
	}
}

void ABG_Character::StopJumpFromInput()
{
	StopJumping();
}

void ABG_Character::StartAimFromInput()
{
	if (!CanStartAim())
	{
		bIsAiming = false;
		UpdateDerivedState();
		return;
	}

	bIsAiming = true;
	UpdateDerivedState();
}

void ABG_Character::StopAimFromInput()
{
	bIsAiming = false;
	UpdateDerivedState();
}

void ABG_Character::ToggleCrouchFromInput()
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ToggleCrouchFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	if (!bCanEnterCrouch)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ToggleCrouchFromInput failed because bCanEnterCrouch is false."), *GetNameSafe(this));
		return;
	}

	if (bIsProne)
	{
		bIsProne = false;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}

	UpdateDerivedState();
}

void ABG_Character::ToggleProneFromInput()
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ToggleProneFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	if (!bCanEnterProne)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ToggleProneFromInput failed because bCanEnterProne is false."), *GetNameSafe(this));
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}

	bIsProne = !bIsProne;
	UpdateDerivedState();
}

void ABG_Character::StartLeanLeftFromInput()
{
	if (!bCanLean)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartLeanLeftFromInput failed because bCanLean is false."), *GetNameSafe(this));
		return;
	}

	LeanDirection = EBGLeanDirection::Left;
}

void ABG_Character::StopLeanLeftFromInput()
{
	if (LeanDirection == EBGLeanDirection::Left)
	{
		LeanDirection = EBGLeanDirection::None;
	}
}

void ABG_Character::StartLeanRightFromInput()
{
	if (!bCanLean)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartLeanRightFromInput failed because bCanLean is false."), *GetNameSafe(this));
		return;
	}

	LeanDirection = EBGLeanDirection::Right;
}

void ABG_Character::StopLeanRightFromInput()
{
	if (LeanDirection == EBGLeanDirection::Right)
	{
		LeanDirection = EBGLeanDirection::None;
	}
}

void ABG_Character::Req_PrimaryAction()
{
	// 총이 장착되어 있으면 근접 공격 대신 발사 경로로 보낸다.
	if (bIsWeaponEquipped)
	{
		if (!bCanFire)
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: Req_PrimaryAction failed because firing is blocked by character state."), *GetNameSafe(this));
			return;
		}

		if (!WeaponFireComponent)
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: Req_PrimaryAction failed because WeaponFireComponent was null."), *GetNameSafe(this));
			return;
		}

		WeaponFireComponent->RequestFire();
		return;
	}

	if (!bCanUseMeleeAttack)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Req_PrimaryAction failed because melee attack is blocked by character state."), *GetNameSafe(this));
		return;
	}

	if (!GetWorld())
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Req_PrimaryAction failed because World was null."), *GetNameSafe(this));
		return;
	}

	if (GetWorld()->TimeSeconds - LastMeleeAttackTime < MeleeAttackCooldown) return;

	CharacterState = EBGCharacterState::MeleeAttacking;
	UpdateDerivedState();

	if (HasAuthority()) Server_ExecuteMeleeAttack_Implementation();
	else Server_ExecuteMeleeAttack();
}

void ABG_Character::Server_ExecuteMeleeAttack_Implementation()
{
	if (!GetWorld())
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Server_ExecuteMeleeAttack_Implementation failed because World was null."), *GetNameSafe(this));
		return;
	}

	if (GetWorld()->TimeSeconds - LastMeleeAttackTime < MeleeAttackCooldown) return;
	LastMeleeAttackTime = GetWorld()->TimeSeconds;

	ProcessMeleeHitDetection();
	Multicast_PlayAttackEffects();
	CharacterState = EBGCharacterState::Idle;
	UpdateDerivedState();
}

bool ABG_Character::Server_ExecuteMeleeAttack_Validate()
{
	return true;
}

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
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Multicast_PlayAttackEffects failed because Body mesh was null."), *GetNameSafe(this));
	}
}

void ABG_Character::ProcessMeleeHitDetection()
{
	if (!DamageSystem)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ProcessMeleeHitDetection failed because DamageSystem was null."), *GetNameSafe(this));
		return;
	}

	if (!GetWorld())
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ProcessMeleeHitDetection failed because World was null."), *GetNameSafe(this));
		return;
	}

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



bool ABG_Character::IsDeadState_Implementation()
{
	return bIsDead;
}

bool ABG_Character::CanFire_Implementation()
{
	return bCanFire;
}

bool ABG_Character::CanLean_Implementation()
{
	return bCanLean;
}

bool ABG_Character::IsCrouchingState_Implementation()
{
	return bIsCrouchingState;
}

bool ABG_Character::CanEnterCrouch_Implementation()
{
	return bCanEnterCrouch;
}

bool ABG_Character::IsProneState_Implementation()
{
	return bIsProne;
}

bool ABG_Character::CanEnterProne_Implementation()
{
	return bCanEnterProne;
}

bool ABG_Character::IsReloading_Implementation()
{
	return bIsReloading;
}

bool ABG_Character::CanReload_Implementation()
{	
	return bCanReload;
}

bool ABG_Character::CanAim_Implementation()
{
	return bCanAim;
}

bool ABG_Character::IsAiming_Implementation()
{
	return bIsAiming;
}


void ABG_Character::OnParachuteAction_Implementation()
{
	// 공중이고(bIsFalling), 낙하산을 가지고 있다면(bHasParachute)
	if (bIsFalling && bHasParachute && !bIsParachuteOpen)
	{
		bIsParachuteOpen = true;
		bHasParachute = false; // 한 번 펴면 소모됨
        
		// 낙하산이 펴지면 하강 속도를 물리적으로 제한
		GetCharacterMovement()->AirControl = 1.0f; // 공중 제어력 증가
		GetCharacterMovement()->GravityScale = 0.1f; // 천천히 떨어짐
	}
}

void ABG_Character::SetWeaponState(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	if (!HasAuthority())
	{
		Server_SetWeaponState(NewWeaponPoseType, bNewWeaponEquipped);
	}

	EquippedWeaponPoseType = NewWeaponPoseType;
	bIsWeaponEquipped = bNewWeaponEquipped && NewWeaponPoseType != EBGWeaponPoseType::None;

	if (!bIsWeaponEquipped)
	{
		bIsAiming = false;
		EquippedWeaponPoseType = EBGWeaponPoseType::None;
	}

	if (WeaponFireComponent)
	{
		WeaponFireComponent->ApplyTemporaryWeaponProfile(EquippedWeaponPoseType);
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: SetWeaponState failed because WeaponFireComponent was null."), *GetNameSafe(this));
	}

	UpdateDerivedState();
	ApplyWeaponMovementState();
	OnRep_EquippedWeaponPoseType();
}

void ABG_Character::Server_SetWeaponState_Implementation(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	SetWeaponState(NewWeaponPoseType, bNewWeaponEquipped);
}

bool ABG_Character::Server_SetWeaponState_Validate(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	return true;
}

void ABG_Character::SetCurrentInteractableWeapon(AActor* NewInteractableWeapon)
{
	CurrentInteractableWeapon = NewInteractableWeapon;
}


void ABG_Character::OnRep_EquippedWeaponPoseType()
{
	if (WeaponFireComponent)
	{
		WeaponFireComponent->ApplyTemporaryWeaponProfile(EquippedWeaponPoseType);
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: OnRep_EquippedWeaponPoseType failed because WeaponFireComponent was null."), *GetNameSafe(this));
	}

	UpdateDerivedState();
	ApplyWeaponMovementState();
}

void ABG_Character::ApplyWeaponMovementState()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (bIsWeaponEquipped)
		{
			bUseControllerRotationYaw = true;
			MovementComponent->bOrientRotationToMovement = false;
			return;
		}

		bUseControllerRotationYaw = false;
		MovementComponent->bOrientRotationToMovement = true;
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: ApplyWeaponMovementState failed because CharacterMovement was null."), *GetNameSafe(this));
}

void ABG_Character::UpdateDerivedState()
{
	bHasWeapon = bIsWeaponEquipped && EquippedWeaponPoseType != EBGWeaponPoseType::None;
	bIsCrouchingState = bIsCrouched;

	UpdateCharacterStance();
	UpdateActionAvailability();
}

void ABG_Character::UpdateActionAvailability()
{
	const bool bIsBusy = CharacterState == EBGCharacterState::Reloading
		|| CharacterState == EBGCharacterState::Equipping
		|| CharacterState == EBGCharacterState::Dead;

	bIsDead = CharacterState == EBGCharacterState::Dead;
	bIsReloading = CharacterState == EBGCharacterState::Reloading;

	bCanAim = bHasWeapon && !bIsReloading && !bIsDead;
	bCanReload = bHasWeapon && !bIsReloading && !bIsDead;
	bCanFire = !bIsBusy;
	bCanUseMeleeAttack = !bHasWeapon && !bIsBusy;
	bCanEnterCrouch = !bIsDead;
	bCanEnterProne = !bIsDead && !bIsFalling;
	bCanLean = bHasWeapon && !bIsReloading && !bIsDead;

	if (!bCanAim && bIsAiming)
	{
		bIsAiming = false;
	}

	if (!bCanLean)
	{
		LeanDirection = EBGLeanDirection::None;
	}
}

void ABG_Character::UpdateCharacterStance()
{
	if (bIsProne)
	{
		CharacterStance = EBGCharacterStance::Prone;
		return;
	}

	if (bIsCrouched)
	{
		CharacterStance = EBGCharacterStance::Crouching;
		return;
	}

	CharacterStance = EBGCharacterStance::Standing;
}

bool ABG_Character::CanStartAim() const
{
	return bCanAim;
}

void ABG_Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABG_Character, bIsWeaponEquipped);
	DOREPLIFETIME(ABG_Character, EquippedWeaponPoseType);
	DOREPLIFETIME(ABG_Character, bIsAiming);
	DOREPLIFETIME(ABG_Character, CharacterState);
	DOREPLIFETIME(ABG_Character, CharacterStance);
	DOREPLIFETIME(ABG_Character, LeanDirection);
	DOREPLIFETIME(ABG_Character, bCanAim);
	DOREPLIFETIME(ABG_Character, bCanReload);
	DOREPLIFETIME(ABG_Character, bIsReloading);
	DOREPLIFETIME(ABG_Character, bCanEnterProne);
	DOREPLIFETIME(ABG_Character, bIsProne);
	DOREPLIFETIME(ABG_Character, bCanEnterCrouch);
	DOREPLIFETIME(ABG_Character, bIsCrouchingState);
	DOREPLIFETIME(ABG_Character, bCanLean);
	DOREPLIFETIME(ABG_Character, bCanFire);
	DOREPLIFETIME(ABG_Character, bCanUseMeleeAttack);
	DOREPLIFETIME(ABG_Character, bIsDead);
}
