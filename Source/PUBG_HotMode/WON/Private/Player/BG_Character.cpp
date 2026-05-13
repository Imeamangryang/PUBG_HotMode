#include "Player/BG_Character.h"
#include "Combat/BG_EquippedWeaponBase.h"
#include "Combat/BG_DamageSystem.h"
#include "Combat/BG_HealthComponent.h"
#include "Combat/BG_WeaponFireComponent.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemUseComponent.h"
#include "Inventory/BG_WorldItemBase.h"
#include "Player/BG_InteractionAnimationComponent.h"
#include "Player/BG_PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/BG_BattleGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Character/Components/ParkourComponent.h"
#include "TimerManager.h"
#include "UI/Components/CompassComponent.h"

DEFINE_LOG_CATEGORY(LogBGCharacter);

ABG_Character::ABG_Character()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	DamageSystem = CreateDefaultSubobject<UBG_DamageSystem>(TEXT("DamageSystem"));
	HealthComponent = CreateDefaultSubobject<UBG_HealthComponent>(TEXT("HealthComponent"));
	InventoryComponent = CreateDefaultSubobject<UBG_InventoryComponent>(TEXT("InventoryComponent"));
	EquipmentComponent = CreateDefaultSubobject<UBG_EquipmentComponent>(TEXT("EquipmentComponent"));
	ItemUseComponent = CreateDefaultSubobject<UBG_ItemUseComponent>(TEXT("ItemUseComponent"));
	WeaponFireComponent = CreateDefaultSubobject<UBG_WeaponFireComponent>(TEXT("WeaponFireComponent"));
	InteractionAnimationComponent = CreateDefaultSubobject<UBG_InteractionAnimationComponent>(TEXT("InteractionAnimationComponent"));

	WeaponAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponAttachPoint"));
	WeaponAttachPoint->SetupAttachment(GetMesh());

	HitCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitCollisionBox"));
	HitCollisionBox->SetupAttachment(RootComponent);
	HitCollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitCollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	HitCollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitCollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HitCollisionBox->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);

	InteractionRangeSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionRangeSphere"));
	InteractionRangeSphere->SetupAttachment(RootComponent);
	InteractionRangeSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionRangeSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionRangeSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionRangeSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	InteractionRangeSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	InteractionRangeSphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	InteractionRangeSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionRangeSphere->SetGenerateOverlapEvents(true);
	InteractionRangeSphere->InitSphereRadius(InteractionRangeRadius);
	
	// 파쿠르 컴포넌트
	ParkourComponent = CreateDefaultSubobject<UParkourComponent>(TEXT("ParkourComponent"));
	
	ParachuteMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParachuteMesh"));
	ParachuteMeshComponent->SetupAttachment(GetMesh());
	ParachuteMeshComponent->SetRelativeLocation(ParachuteRelativeLocation);
	ParachuteMeshComponent->SetRelativeRotation(ParachuteRelativeRotation);
	ParachuteMeshComponent->SetRelativeScale3D(ParachuteRelativeScale);
	ParachuteMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ParachuteMeshComponent->SetGenerateOverlapEvents(false);
	ParachuteMeshComponent->SetHiddenInGame(true);
	
	CompassComponent = CreateDefaultSubobject<UCompassComponent>(TEXT("CompassComponent"));

	bIsWeaponEquipped = false;
	EquippedWeaponPoseType = EBGWeaponPoseType::None;
	bIsAiming = false;
	GetCharacterMovement()->MaxWalkSpeed = StandingWalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchWalkSpeed;
	RefreshMovementState();
	UpdateDerivedState();
}


void ABG_Character::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnDeathStateChanged.AddDynamic(this, &ABG_Character::HandleHealthDeathStateChanged);
		HealthComponent->OnDamaged.AddDynamic(this, &ABG_Character::HandleDamaged);
		HandleHealthDeathStateChanged(HealthComponent->IsDead());
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: BeginPlay failed because HealthComponent was null."), *GetNameSafe(this));
	}

	if (!HitCollisionBox)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: BeginPlay failed because HitCollisionBox was null."), *GetNameSafe(this));
	}
	else if (UCapsuleComponent* CharacterCapsuleComponent = GetCapsuleComponent())
	{
		const float CapsuleRadius = CharacterCapsuleComponent->GetScaledCapsuleRadius();
		const float CapsuleHalfHeight = CharacterCapsuleComponent->GetScaledCapsuleHalfHeight();
		HitCollisionBox->SetBoxExtent(FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight));
		HitCollisionBox->SetRelativeLocation(FVector(0.f, 0.f, CapsuleHalfHeight));
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: BeginPlay failed because CapsuleComponent was null."), *GetNameSafe(this));
	}

	if (InteractionRangeSphere)
	{
		InteractionRangeSphere->SetSphereRadius(InteractionRangeRadius);
		InteractionRangeSphere->OnComponentBeginOverlap.AddDynamic(this, &ABG_Character::OnInteractionRangeBeginOverlap);
		InteractionRangeSphere->OnComponentEndOverlap.AddDynamic(this, &ABG_Character::OnInteractionRangeEndOverlap);
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: BeginPlay failed because InteractionRangeSphere was null."), *GetNameSafe(this));
	}

	// 변수명을 Children에서 ChildComps로 변경하여 충돌 방지
	if (USkeletalMeshComponent* Body = GetBodyAnimationMesh())
	{
		TArray<USceneComponent*> ChildComps;
		Body->GetChildrenComponents(true, ChildComps);

		for (USceneComponent* Child : ChildComps)
		{
			if (USkeletalMeshComponent* SkeletalChild = Cast<USkeletalMeshComponent>(Child))
			{
				// Body 애니메이션을 따라가게 함
				SkeletalChild->SetLeaderPoseComponent(Body);
			}
		}
	}

	RefreshMovementState();
	if (bIsFalling)
	{
		StartFallingStateMonitor();
	}
	
	SetCanReceiveBlueZoneDamage(false);
	SetIsOutsideBlueZone(false);
}

void ABG_Character::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: OnMovementModeChanged failed because CharacterMovement was null."), *GetNameSafe(this));
		return;
	}

	if (MovementComponent->IsFalling())
	{
		FallingStartZ = GetActorLocation().Z;
		bIsJumpFalling = true;
		bIsParachuteFalling = false;
		RefreshMovementState();
		StartFallingStateMonitor();
		return;
	}

	if (PrevMovementMode == MOVE_Falling)
	{
		HandleLandingFromAir();
	}

	StopFallingStateMonitor();
	RefreshMovementState();
}

void ABG_Character::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshMovementState();
	UpdateDerivedState();
}

void ABG_Character::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	RefreshMovementState();
	UpdateDerivedState();
}
// 스탠딩/크라우치/엎드리기 상태 변경 OnRep
void ABG_Character::OnRep_CharacterStance()
{
	UpdateDerivedState();
}

void ABG_Character::OnRep_IsProne()
{
	UpdateDerivedState();
}

void ABG_Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// 바인딩은 BG_PlayerController에서 수행하므로 비워둠
}

void ABG_Character::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	StopFallingStateMonitor();
	RefreshMovementState();
	
	if (!bIsSkyDiving)
	{
		return;
	}

	ResetAfterSkyDiveLanding();
}

bool ABG_Character::CanJumpInternal_Implementation() const
{
	const bool bCanJump = Super::CanJumpInternal_Implementation();
	return bCanJump && !bIsProne && !bIsDead;
}

void ABG_Character::MoveFromInput(const FInputActionValue& Value)
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: MoveFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!MovementVector.IsNearlyZero())
	{
		if (ItemUseComponent)
		{
			ItemUseComponent->NotifyMovementInput();
		}
		else
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: MoveFromInput could not notify item use movement because ItemUseComponent was null."), *GetNameSafe(this));
		}
	}

	if (Controller && !MovementVector.IsNearlyZero())
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}

	bIsAcceleration = !MovementVector.IsNearlyZero();
	UpdateActionAvailability();
}

void ABG_Character::LookFromInput(const FInputActionValue& Value)
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: LookFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ABG_Character::StartJumpFromInput()
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartJumpFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	if (bIsProne)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartJumpFromInput failed because character is prone."), *GetNameSafe(this));
		return;
	}

	if (ParkourComponent)
	{
		ParkourComponent->TryParkour();
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartJumpFromInput failed because ParkourComponent was null."), *GetNameSafe(this));
		return;
	}
}

void ABG_Character::StopJumpFromInput()
{
	StopJumping();
}

void ABG_Character::StartPrimaryActionFromInput()
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartPrimaryActionFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	if (bIsWeaponEquipped)
	{
		if (ItemUseComponent)
		{
			ItemUseComponent->NotifyFireInput();
		}
		else
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: StartPrimaryActionFromInput could not notify item use fire because ItemUseComponent was null."), *GetNameSafe(this));
		}

		if (!WeaponFireComponent)
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: StartPrimaryActionFromInput failed because WeaponFireComponent was null."), *GetNameSafe(this));
			return;
		}

		WeaponFireComponent->RequestStartFire();
		return;
	}

	Req_PrimaryAction();
}

void ABG_Character::StopPrimaryActionFromInput()
{
	if (!bIsWeaponEquipped)
	{
		return;
	}

	if (!WeaponFireComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StopPrimaryActionFromInput failed because WeaponFireComponent was null."), *GetNameSafe(this));
		return;
	}

	WeaponFireComponent->RequestStopFire();
}

void ABG_Character::InteractFromInput()
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: InteractFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	TryInteractWithCurrentTarget();
}

void ABG_Character::ReloadFromInput()
{
	if (bIsDead)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ReloadFromInput failed because character is dead."), *GetNameSafe(this));
		return;
	}

	if (!WeaponFireComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ReloadFromInput failed because WeaponFireComponent was null."), *GetNameSafe(this));
		return;
	}

	WeaponFireComponent->RequestReload();
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
	if (bIsDead || !bCanEnterCrouch)
	{
		return;
	}

	// 일반 크라우치는 언리얼 기본 Crouch 복제/예측 흐름을 그대로 사용한다.
	// Prone -> Crouch 전환에서만 서버 권한으로 stance를 정리해 서버/클라 불일치를 막는다.
	if (bIsProne)
	{
		if (!HasAuthority())
		{
			Server_SetCharacterStanceState(EBGCharacterStance::Crouching);
			return;
		}

		ApplyCharacterStanceState(EBGCharacterStance::Crouching);
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}

	RefreshMovementState();
	UpdateDerivedState();
}

void ABG_Character::ToggleProneFromInput()
{
	if (bIsDead || !bCanEnterProne)
	{
		return;
	}

	// 클라이언트에서만 호출되면 서버가 상태를 가지지 못하므로 서버로 전달
	const bool bNewIsProne = !bIsProne;
	if (!HasAuthority())
	{
		Server_SetProneState(bNewIsProne);
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}

	bIsProne = bNewIsProne;
	RefreshMovementState();
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
	if (ItemUseComponent)
	{
		ItemUseComponent->NotifyFireInput();
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Req_PrimaryAction could not notify item use fire because ItemUseComponent was null."), *GetNameSafe(this));
	}

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

	SetCharacterStateValue(EBGCharacterState::MeleeAttacking);

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
	SetCharacterStateValue(EBGCharacterState::Idle);
}

bool ABG_Character::Server_ExecuteMeleeAttack_Validate()
{
	return true;
}

void ABG_Character::Multicast_PlayAttackEffects_Implementation()
{
	// 'Body' 메쉬의 AnimInstance를 가져와 몽타주 재생
	if (USkeletalMeshComponent* Body = GetBodyAnimationMesh())
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

USkeletalMeshComponent* ABG_Character::GetBodyAnimationMesh()
{
	if (USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(GetDefaultSubobjectByName(TEXT("Body"))))
	{
		return Body;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: GetBodyAnimationMesh failed because GetMesh returned null."), *GetNameSafe(this));
		return nullptr;
	}

	return MeshComponent;
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
		bIsParachuteFalling = true;
		bIsJumpFalling = false;
		bHasParachute = false; // 한 번 펴면 소모됨
		// 수직 속도 제한
		// 수평 속도도 줄임
		// 공중 제어력 증가
		// falling 마찰/감속 증가
		// 카메라 약간 더 안정적인 거리로 변경
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->AirControl = 0.8f;
			MovementComponent->GravityScale = 0.2f;
			MovementComponent->BrakingDecelerationFalling = 2048.0f;
			MovementComponent->FallingLateralFriction = 1.5f;

			FVector Velocity = MovementComponent->Velocity;

			// 낙하산 전개 순간 즉시 감속
			Velocity.X *= 0.25f;
			Velocity.Y *= 0.25f;
			Velocity.Z = -150.0f;

			MovementComponent->Velocity = Velocity;
		}
		
		OnRep_IsParachuteOpen();
	}
}

void ABG_Character::BeginAirplaneDrop(const FVector& DropLocation, const FVector& DropForwardVector)
{
	bIsSkyDiving = true;
	bIsParachuteOpen = false;
	bHasParachute = true;

	SetActorLocation(DropLocation);
	SetActorRotation(DropForwardVector.Rotation());

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(MOVE_Falling);
		MovementComponent->Velocity = FVector::ZeroVector;
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: BeginAirplaneDrop failed because CharacterMovement was null."), *GetNameSafe(this));
		return;
	}

	LaunchCharacter(DropForwardVector.GetSafeNormal() * 600.0f + FVector(0.0f, 0.0f, -200.0f), true, true);
	ApplySkyDiveCamera();
	UpdateDerivedState();
	SetCanReceiveBlueZoneDamage(true);
	
	if (HasAuthority() && GetWorld())
	{
		FTimerHandle AutoParachuteTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			AutoParachuteTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!IsValid(this))
				{
					return;
				}

				if (!bIsSkyDiving)
				{
					return;
				}

				if (bIsParachuteOpen)
				{
					return;
				}

				if (!bIsFalling)
				{
					return;
				}

				OnParachuteAction();
				Client_ApplyParachuteCamera();
			}),
			5.0f,
			false
		);
	}
}

void ABG_Character::OnRep_IsParachuteOpen()
{
	UpdateParachuteVisibility();
}

void ABG_Character::Server_BeginAirplaneDrop_Implementation(const FVector& DropLocation, const FVector& DropForwardVector)
{
	BeginAirplaneDrop(DropLocation, DropForwardVector);
}

void ABG_Character::Server_OpenParachute_Implementation()
{
	OnParachuteAction();
}

void ABG_Character::SetWeaponState(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	const bool bNewEffectiveWeaponEquipped = bNewWeaponEquipped
		&& NewWeaponPoseType != EBGWeaponPoseType::None
		&& EquippedWeapon != nullptr;
	const bool bWeaponStateChanging = EquippedWeaponPoseType != NewWeaponPoseType
		|| bIsWeaponEquipped != bNewEffectiveWeaponEquipped;

	if (bWeaponStateChanging)
	{
		if (ItemUseComponent)
		{
			ItemUseComponent->NotifyWeaponSwitch();
		}
		else
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: SetWeaponState could not notify item use weapon switch because ItemUseComponent was null."), *GetNameSafe(this));
		}
	}

	EquippedWeaponPoseType = NewWeaponPoseType;
	bIsWeaponEquipped = bNewEffectiveWeaponEquipped;

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
}

void ABG_Character::Server_SetWeaponState_Implementation(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	SetWeaponState(NewWeaponPoseType, bNewWeaponEquipped);
}

bool ABG_Character::Server_SetWeaponState_Validate(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	return true;
}

bool ABG_Character::SetInfiniteAmmo(bool bNewUseInfiniteAmmo)
{
	if (!EquipmentComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: SetInfiniteAmmo failed because EquipmentComponent was null."), *GetNameSafe(this));
		return false;
	}

	ABG_EquippedWeaponBase* ActiveWeapon = EquipmentComponent->GetActiveEquippedWeaponActor();
	if (!ActiveWeapon)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: SetInfiniteAmmo failed because active equipped weapon was null."), *GetNameSafe(this));
		return false;
	}

	if (!HasAuthority())
	{
		Server_SetInfiniteAmmo(bNewUseInfiniteAmmo);
		return true;
	}

	ActiveWeapon->SetUseInfiniteDebugAmmo(bNewUseInfiniteAmmo);

	if (WeaponFireComponent)
	{
		WeaponFireComponent->RefreshAmmoStateFromEquipment();
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: SetInfiniteAmmo could not refresh ammo because WeaponFireComponent was null."), *GetNameSafe(this));
	}

	UE_LOG(LogBGCharacter, Log, TEXT("%s: Set active weapon infinite debug ammo to %s for %s."),
		*GetNameSafe(this),
		bNewUseInfiniteAmmo ? TEXT("true") : TEXT("false"),
		*GetNameSafe(ActiveWeapon));
	return true;
}

void ABG_Character::Server_SetInfiniteAmmo_Implementation(bool bNewUseInfiniteAmmo)
{
	SetInfiniteAmmo(bNewUseInfiniteAmmo);
}

bool ABG_Character::Server_SetInfiniteAmmo_Validate(bool bNewUseInfiniteAmmo)
{
	return true;
}

void ABG_Character::SetCurrentInteractableWeapon(AActor* NewInteractableWeapon)
{
	CurrentInteractableWeapon = NewInteractableWeapon;
}

ABG_WorldItemBase* ABG_Character::GetCurrentWorldItem() const
{
	return Cast<ABG_WorldItemBase>(CurrentInteractableWeapon);
}

TArray<ABG_WorldItemBase*> ABG_Character::GetNearbyWorldItems() const
{
	TArray<ABG_WorldItemBase*> WorldItems;
	for (AActor* CandidateActor : NearbyInteractableWeapons)
	{
		if (ABG_WorldItemBase* WorldItem = Cast<ABG_WorldItemBase>(CandidateActor))
		{
			WorldItems.Add(WorldItem);
		}
	}

	return WorldItems;
}

void ABG_Character::NotifySuccessfulPickup(EBG_ItemType PickedUpItemType)
{
	if (!HasAuthority())
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: NotifySuccessfulPickup failed because character had no authority. ItemType=%s"),
			*GetNameSafe(this),
			*StaticEnum<EBG_ItemType>()->GetNameStringByValue(static_cast<int64>(PickedUpItemType)));
		return;
	}

	Multicast_PlayPickupMontage(PickedUpItemType);
}

void ABG_Character::SetCharacterStateValue(EBGCharacterState NewCharacterState)
{
	if (CharacterState == EBGCharacterState::Dead && NewCharacterState != EBGCharacterState::Dead)
	{
		return;
	}

	CharacterState = NewCharacterState;
	UpdateDerivedState();
}

void ABG_Character::StartTimedCharacterState(EBGCharacterState NewCharacterState, float Duration)
{
	SetCharacterStateValue(NewCharacterState);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartTimedCharacterState failed because World was null."), *GetNameSafe(this));
		return;
	}

	World->GetTimerManager().ClearTimer(CharacterStateTimerHandle);
	if (Duration > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			CharacterStateTimerHandle,
			FTimerDelegate::CreateUObject(this, &ABG_Character::FinishTimedCharacterState, NewCharacterState),
			Duration,
			false);
	}
}

void ABG_Character::SetEquippedWeapon(ABG_EquippedWeaponBase* NewEquippedWeapon)
{
	EquippedWeapon = NewEquippedWeapon;
	bIsWeaponEquipped = EquippedWeapon != nullptr && EquippedWeaponPoseType != EBGWeaponPoseType::None;
	UpdateDerivedState();
}

void ABG_Character::FinishTimedCharacterState(EBGCharacterState ExpectedCharacterState)
{
	if (CharacterState != ExpectedCharacterState || CharacterState == EBGCharacterState::Dead)
	{
		return;
	}

	SetCharacterStateValue(EBGCharacterState::Idle);
}

FName ABG_Character::GetDesiredWeaponSocketName(EBG_EquipmentSlot WeaponSlot) const
{
	if (EquipmentComponent && EquipmentComponent->GetActiveWeaponSlot() == WeaponSlot)
	{
		return WeaponHandSocketName;
	}

	return WeaponBackSocketName;
}

void ABG_Character::PickupWorldItem(ABG_WorldItemBase* WorldItem, int32 Quantity)
{
	if (HasAuthority())
	{
		EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
		if (!IsValid(WorldItem))
		{
			Client_ReceiveInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::None, FGameplayTag());
			return;
		}

		if (!WorldItem->TryPickup(this, Quantity, FailReason))
		{
			Client_ReceiveInventoryFailure(FailReason, WorldItem->GetItemType(), WorldItem->GetItemTag());
		}
		return;
	}

	Server_PickupWorldItem(WorldItem, Quantity);
}

void ABG_Character::UseInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag)
{
	if (!ensureMsgf(ItemUseComponent, TEXT("%s: UseInventoryItem failed because ItemUseComponent was null."),
	                *GetNameSafe(this)))
	{
		return;
	}

	ItemUseComponent->UseItem(ItemType, ItemTag);
}

bool ABG_Character::GiveInventoryItem(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity,
	int32& OutAddedQuantity)
{
	OutAddedQuantity = 0;

	if (!InventoryComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: GiveInventoryItem failed because InventoryComponent was null."), *GetNameSafe(this));
		return false;
	}

	if (!HasAuthority())
	{
		Server_GiveInventoryItem(ItemType, ItemTag, Quantity);
		return true;
	}

	if (!InventoryComponent->Auth_AddItem(ItemType, ItemTag, Quantity, OutAddedQuantity))
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: GiveInventoryItem failed for %s %s x%d."),
			*GetNameSafe(this),
			*StaticEnum<EBG_ItemType>()->GetNameStringByValue(static_cast<int64>(ItemType)),
			*ItemTag.ToString(),
			Quantity);
		return false;
	}

	UE_LOG(LogBGCharacter, Log, TEXT("%s: Gave inventory item %s %s x%d/%d."),
		*GetNameSafe(this),
		*StaticEnum<EBG_ItemType>()->GetNameStringByValue(static_cast<int64>(ItemType)),
		*ItemTag.ToString(),
		OutAddedQuantity,
		Quantity);
	return true;
}

void ABG_Character::Server_PickupWorldItem_Implementation(
	ABG_WorldItemBase* WorldItem,
	int32 Quantity)
{
	if (!IsValid(WorldItem))
	{
		Client_ReceiveInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::None, FGameplayTag());
		return;
	}

	EBGInventoryFailReason FailReason = EBGInventoryFailReason::None;
	if (!WorldItem->TryPickup(this, Quantity, FailReason))
	{
		Client_ReceiveInventoryFailure(FailReason, WorldItem->GetItemType(), WorldItem->GetItemTag());
	}
}

void ABG_Character::Server_GiveInventoryItem_Implementation(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity)
{
	int32 AddedQuantity = 0;
	GiveInventoryItem(ItemType, ItemTag, Quantity, AddedQuantity);
}

bool ABG_Character::Server_GiveInventoryItem_Validate(
	EBG_ItemType ItemType,
	FGameplayTag ItemTag,
	int32 Quantity)
{
	return true;
}

void ABG_Character::Client_ReceiveInventoryFailure_Implementation(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	FGameplayTag ItemTag)
{
	const UEnum* FailReasonEnum = StaticEnum<EBGInventoryFailReason>();
	const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
	const FString FailReasonName = FailReasonEnum
		? FailReasonEnum->GetNameStringByValue(static_cast<int64>(FailReason))
		: TEXT("<Unknown>");
	const FString ItemTypeName = ItemTypeEnum
		? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(ItemType))
		: TEXT("<Unknown>");

	const FString FailureMessage = FString::Printf(
		TEXT("%s: Inventory request failed. Reason=%s, ItemType=%s, ItemTag=%s."),
		*GetNameSafe(this),
		*FailReasonName,
		*ItemTypeName,
		*ItemTag.ToString());

	UE_LOG(LogBGCharacter, Warning, TEXT("%s"), *FailureMessage);
	if (ABG_PlayerController* BGPlayerController = Cast<ABG_PlayerController>(GetController()))
	{
		BGPlayerController->NotifyInventoryFailure(FailReason, ItemType, ItemTag);
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Client_ReceiveInventoryFailure could not notify ViewModel because controller was not ABG_PlayerController."), *GetNameSafe(this));
	}

	if (GEngine && GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FailureMessage);
	}
}

void ABG_Character::OnInteractionRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OverlappedComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: OnInteractionRangeBeginOverlap failed because OverlappedComponent was null."), *GetNameSafe(this));
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	if (!OtherComp)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: OnInteractionRangeBeginOverlap failed because OtherComp was null. OtherActor=%s"), *GetNameSafe(this), *GetNameSafe(OtherActor));
		return;
	}

	if (!IsWeaponInteractableActor(OtherActor))
	{
		return;
	}

	NearbyInteractableWeapons.AddUnique(OtherActor);
	RefreshCurrentInteractableWeapon();
	OnNearbyWorldItemsChanged.Broadcast();
}

void ABG_Character::OnInteractionRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OverlappedComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: OnInteractionRangeEndOverlap failed because OverlappedComponent was null."), *GetNameSafe(this));
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	NearbyInteractableWeapons.Remove(OtherActor);
	RefreshCurrentInteractableWeapon();
	OnNearbyWorldItemsChanged.Broadcast();
}

void ABG_Character::RefreshCurrentInteractableWeapon()
{
	NearbyInteractableWeapons.RemoveAll([](const TObjectPtr<AActor>& CandidateActor)
	{
		return !IsValid(CandidateActor);
	});

	AActor* ClosestInteractable = nullptr;
	float ClosestDistanceSq = TNumericLimits<float>::Max();

	for (AActor* CandidateActor : NearbyInteractableWeapons)
	{
		if (!IsValid(CandidateActor))
		{
			continue;
		}

		const float CandidateDistanceSq = FVector::DistSquared(GetActorLocation(), CandidateActor->GetActorLocation());
		if (CandidateDistanceSq < ClosestDistanceSq)
		{
			ClosestDistanceSq = CandidateDistanceSq;
			ClosestInteractable = CandidateActor;
		}
	}

	SetCurrentInteractableWeapon(ClosestInteractable);
}

bool ABG_Character::IsWeaponInteractableActor(const AActor* CandidateActor) const
{
	if (!IsValid(CandidateActor))
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: IsWeaponInteractableActor failed because CandidateActor was invalid."), *GetNameSafe(this));
		return false;
	}

	if (CandidateActor->IsA<ABG_WorldItemBase>())
	{
		return true;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	CandidateActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: IsWeaponInteractableActor found a null PrimitiveComponent on %s."), *GetNameSafe(this), *GetNameSafe(CandidateActor));
			continue;
		}

		if (PrimitiveComponent->GetCollisionProfileName() == WeaponInteractableCollisionProfileName)
		{
			return true;
		}
	}

	return false;
}

void ABG_Character::TryInteractWithCurrentTarget()
{
	if (CurrentInteractableWeapon)
	{
		if (ABG_WorldItemBase* WorldItem = Cast<ABG_WorldItemBase>(CurrentInteractableWeapon))
		{
			PickupWorldItem(WorldItem, WorldItem->GetQuantity());
			return;
		}

		const FString DebugMessage = FString::Printf(TEXT("%s: Interact detected with %s."),
			*GetNameSafe(this),
			*GetNameSafe(CurrentInteractableWeapon));

		UE_LOG(LogBGCharacter, Warning, TEXT("%s"), *DebugMessage);

		if (GEngine && GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, DebugMessage);
		}

		return;
	}

	if (bIsFalling && bHasParachute && !bIsParachuteOpen)
	{
		if (!HasAuthority())
		{
			ApplyParachuteCamera();
			Server_OpenParachute();
		}
		else
		{
			OnParachuteAction();
		}
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: TryInteractWithCurrentTarget failed because there was no interactable target in range."), *GetNameSafe(this));
}

void ABG_Character::ApplySkyDiveCamera()
{
	if (!CameraBoom)
	{
		return;
	}

	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 250.0f);
}

void ABG_Character::ApplyParachuteCamera()
{
	if (!CameraBoom)
	{
		return;
	}

	CameraBoom->TargetArmLength = 1800.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 180.0f);
}

void ABG_Character::Client_RestoreDefaultCamera_Implementation()
{
	RestoreDefaultCamera();
}

void ABG_Character::Client_ApplyParachuteCamera_Implementation()
{
	ApplyParachuteCamera();
}

void ABG_Character::RestoreDefaultCamera()
{
	if (!CameraBoom)
	{
		return;
	}

	CameraBoom->TargetArmLength = DefaultCameraBoomLength;
	CameraBoom->SocketOffset = DefaultCameraBoomSocketOffset;
}

void ABG_Character::ResetAfterSkyDiveLanding()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->GravityScale = 1.0f;
		MovementComponent->AirControl = 0.05f;
		MovementComponent->BrakingDecelerationFalling = 0.0f;
		MovementComponent->FallingLateralFriction = 0.0f;
	}

	bIsSkyDiving = false;
	ResetAirStateTracking();

	if (HasAuthority())
	{
		Client_RestoreDefaultCamera();
	}
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

void ABG_Character::Multicast_PlayPickupMontage_Implementation(EBG_ItemType PickedUpItemType)
{
	if (!InteractionAnimationComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Multicast_PlayPickupMontage failed because InteractionAnimationComponent was null."), *GetNameSafe(this));
		return;
	}

	const float PickupDuration = InteractionAnimationComponent->GetPickupMontageDuration(PickedUpItemType);
	if (PickupDuration > KINDA_SMALL_NUMBER && !bIsDead)
	{
		StartTimedCharacterState(EBGCharacterState::Equipping, PickupDuration);
	}

	InteractionAnimationComponent->PlayPickupMontage(PickedUpItemType);
}

void ABG_Character::HandleHealthDeathStateChanged(bool bNewIsDead)
{
	if (bNewIsDead)
	{
		ClearTimedCharacterState();
		SetCharacterStateValue(EBGCharacterState::Dead);
		bIsAiming = false;
		bIsProne = false;
		LeanDirection = EBGLeanDirection::None;

		if (WeaponFireComponent && (HasAuthority() || IsLocallyControlled()))
		{
			WeaponFireComponent->RequestStopFire();
		}

		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->DisableMovement();
		}
		else
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: HandleHealthDeathStateChanged failed because CharacterMovement was null."), *GetNameSafe(this));
		}

		if (ABG_PlayerController* BGPlayerController = Cast<ABG_PlayerController>(GetController()))
		{
			BGPlayerController->HandleControlledCharacterDeath(this);
		}

		if (HasAuthority())
		{
			if (ABG_BattleGameMode* BattleGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ABG_BattleGameMode>() : nullptr)
			{
				BattleGameMode->HandlePlayerDeath(GetController(), this);
			}
			else
			{
				UE_LOG(LogBGCharacter, Error, TEXT("%s: HandleHealthDeathStateChanged could not notify BattleGameMode."), *GetNameSafe(this));
			}
		}
	}
	else if (CharacterState == EBGCharacterState::Dead)
	{
		CharacterState = EBGCharacterState::Idle;

		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
		else
		{
			UE_LOG(LogBGCharacter, Error, TEXT("%s: HandleHealthDeathStateChanged failed because CharacterMovement was null during revive."), *GetNameSafe(this));
		}

		if (ABG_PlayerController* BGPlayerController = Cast<ABG_PlayerController>(GetController()))
		{
			BGPlayerController->HandleControlledCharacterRevived(this);
		}
	}

	UpdateDerivedState();
}

void ABG_Character::HandleDamaged(float DamageAmount, float CurrentHP, float MaxHP, bool bNewIsDead)
{
	if (DamageAmount <= 0.f)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: HandleDamaged failed because DamageAmount %.2f was not positive."), *GetNameSafe(this), DamageAmount);
		return;
	}

	if (bNewIsDead)
	{
		return;
	}

	Multicast_PlayHitReactMontage();
}

void ABG_Character::Multicast_PlayHitReactMontage_Implementation()
{
	if (!HitReactMontage)
	{
		return;
	}

	USkeletalMeshComponent* BodyMesh = GetBodyAnimationMesh();
	if (!BodyMesh)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Multicast_PlayHitReactMontage failed because BodyMesh was null."), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInstance = BodyMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: Multicast_PlayHitReactMontage failed because AnimInstance was null."), *GetNameSafe(this));
		return;
	}

	AnimInstance->Montage_Play(HitReactMontage);
}

void ABG_Character::ApplyWeaponMovementState()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		bUseControllerRotationYaw = true;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: ApplyWeaponMovementState failed because CharacterMovement was null."), *GetNameSafe(this));
}

void ABG_Character::UpdateDerivedState()
{
	// Character animation state always follows the replicated active weapon actor so temporary attachment mismatches do not break ABP.
	if (EquipmentComponent)
	{
		EquippedWeapon = EquipmentComponent->GetActiveEquippedWeaponActor();
	}

	bHasWeapon = EquippedWeapon != nullptr && EquippedWeaponPoseType != EBGWeaponPoseType::None;
	bIsWeaponEquipped = bHasWeapon;
	bIsCrouchingState = bIsCrouched;

	UpdateCharacterStance();
	ApplyMovementSpeedForStance();
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
	bCanFire = bHasWeapon && !bIsBusy;
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

void ABG_Character::ApplyCharacterStanceState(EBGCharacterStance NewCharacterStance)
{
	switch (NewCharacterStance)
	{
	case EBGCharacterStance::Standing:
		if (bIsCrouched)
		{
			UnCrouch();
		}
		bIsProne = false;
		break;

	case EBGCharacterStance::Crouching:
		bIsProne = false;
		if (!bIsCrouched)
		{
			Crouch();
		}
		break;

	case EBGCharacterStance::Prone:
		if (bIsCrouched)
		{
			UnCrouch();
		}
		bIsProne = true;
		break;

	default:
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ApplyCharacterStanceState failed because stance %s was invalid."), *GetNameSafe(this), *UEnum::GetValueAsString(NewCharacterStance));
		return;
	}

	RefreshMovementState();
	UpdateDerivedState();
}

void ABG_Character::ApplyMovementSpeedForStance()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ApplyMovementSpeedForStance failed because CharacterMovement was null."), *GetNameSafe(this));
		return;
	}
	
	MovementComponent->MaxWalkSpeedCrouched = CrouchWalkSpeed;
	MovementComponent->MaxWalkSpeed = bIsProne ? ProneWalkSpeed : StandingWalkSpeed;
}

void ABG_Character::Server_SetProneState_Implementation(bool bNewIsProne)
{
	if (bIsDead || !bCanEnterProne)
	{
		return;
	}

	ApplyCharacterStanceState(bNewIsProne ? EBGCharacterStance::Prone : EBGCharacterStance::Standing);
}

bool ABG_Character::Server_SetProneState_Validate(bool bNewIsProne)
{
	return true;
}

void ABG_Character::Server_SetCharacterStanceState_Implementation(EBGCharacterStance NewCharacterStance)
{
	if (bIsDead)
	{
		return;
	}

	if (NewCharacterStance == EBGCharacterStance::Prone && !bCanEnterProne)
	{
		return;
	}

	if (NewCharacterStance == EBGCharacterStance::Crouching && !bCanEnterCrouch)
	{
		return;
	}

	ApplyCharacterStanceState(NewCharacterStance);
}

bool ABG_Character::Server_SetCharacterStanceState_Validate(EBGCharacterStance NewCharacterStance)
{
	return NewCharacterStance == EBGCharacterStance::Standing
		|| NewCharacterStance == EBGCharacterStance::Crouching
		|| NewCharacterStance == EBGCharacterStance::Prone;
}

void ABG_Character::Client_ApplyRecoil_Implementation(float PitchDelta, float YawDelta, TSubclassOf<UCameraShakeBase> CameraShakeClass, float CameraShakeScale)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		FRotator NewControlRotation = PlayerController->GetControlRotation();
		NewControlRotation.Pitch = FMath::ClampAngle(NewControlRotation.Pitch + PitchDelta, -89.0f, 89.0f);
		NewControlRotation.Yaw = FMath::Fmod(NewControlRotation.Yaw + YawDelta + 360.0f, 360.0f);
		PlayerController->SetControlRotation(NewControlRotation);

		if (CameraShakeClass)
		{
			PlayerController->ClientStartCameraShake(CameraShakeClass, CameraShakeScale, ECameraShakePlaySpace::CameraLocal);
		}
	}
}

void ABG_Character::ClearTimedCharacterState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CharacterStateTimerHandle);
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: ClearTimedCharacterState failed because World was null."), *GetNameSafe(this));
}

void ABG_Character::RefreshMovementState()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: RefreshMovementState failed because CharacterMovement was null."), *GetNameSafe(this));
		bIsFalling = false;
		bIsAcceleration = false;
		return;
	}

	bIsFalling = MovementComponent->IsFalling();
	bIsAcceleration = MovementComponent->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;
	UpdateActionAvailability();
}

void ABG_Character::StartFallingStateMonitor()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: StartFallingStateMonitor failed because World was null."), *GetNameSafe(this));
		return;
	}

	World->GetTimerManager().SetTimer(
		FallingStateMonitorTimerHandle,
		this,
		&ABG_Character::EvaluateFallingState,
		0.05f,
		true);
}

void ABG_Character::StopFallingStateMonitor()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallingStateMonitorTimerHandle);
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: StopFallingStateMonitor failed because World was null."), *GetNameSafe(this));
}

void ABG_Character::EvaluateFallingState()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: EvaluateFallingState failed because CharacterMovement was null."), *GetNameSafe(this));
		StopFallingStateMonitor();
		bIsFalling = false;
		bIsAcceleration = false;
		UpdateActionAvailability();
		return;
	}

	if (!MovementComponent->IsFalling())
	{
		StopFallingStateMonitor();
		RefreshMovementState();
		return;
	}

	RefreshMovementState();

	const float FallingDistance = FallingStartZ - GetActorLocation().Z;
	const bool bReachedParachuteThreshold = FallingDistance >= ParachuteFallThreshold;
	if (bReachedParachuteThreshold && !bIsParachuteFalling && ParachuteFallMontage)
	{
		if (USkeletalMeshComponent* BodyMesh = GetBodyAnimationMesh())
		{
			if (UAnimInstance* AnimInstance = BodyMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(ParachuteFallMontage);
			}
		}
	}

	bIsParachuteFalling = bReachedParachuteThreshold;
	bIsJumpFalling = !bReachedParachuteThreshold;
}

bool ABG_Character::CanStartAim() const
{
	return bCanAim;
}

void ABG_Character::ResetAirStateTracking()
{
	StopFallingStateMonitor();
	bIsParachuteFalling = false;
	bIsJumpFalling = false;
	bIsParachuteOpen = false;
	FallingStartZ = 0.f;
	
	UpdateParachuteVisibility();
	
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->AirControl = 0.05f;
		MovementComponent->GravityScale = 1.0f;
	}
	else
	{
		UE_LOG(LogBGCharacter, Error, TEXT("%s: ResetAirStateTracking failed because CharacterMovement was null."), *GetNameSafe(this));
	}
}

void ABG_Character::HandleLandingFromAir()
{
	if (bIsParachuteFalling && ParachuteLandingMontage)
	{
		if (USkeletalMeshComponent* BodyMesh = GetBodyAnimationMesh())
		{
			if (UAnimInstance* AnimInstance = BodyMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(ParachuteLandingMontage);
			}
		}
	}

	ResetAirStateTracking();
}

void ABG_Character::UpdateParachuteVisibility()
{
	if (!ParachuteMeshComponent)
	{
		return;
	}
	ParachuteMeshComponent->SetHiddenInGame(!bIsParachuteOpen);
}

void ABG_Character::SetCanReceiveBlueZoneDamage(bool bNewCanReceiveBlueZoneDamage)
{
	bCanReceiveBlueZoneDamage = bNewCanReceiveBlueZoneDamage;
}

void ABG_Character::SetIsOutsideBlueZone(bool bNewIsOutsideBlueZone)
{
	if (bIsOutsideBlueZone == bNewIsOutsideBlueZone)
	{
		return;
	}

	bIsOutsideBlueZone = bNewIsOutsideBlueZone;
	OnRep_IsOutsideBlueZone();
}

void ABG_Character::OnRep_IsOutsideBlueZone()
{
	UE_LOG(LogBGCharacter, Log, TEXT("%s: BlueZone outside state changed to %s"),
		*GetNameSafe(this),
		bIsOutsideBlueZone ? TEXT("true") : TEXT("false"));
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
	DOREPLIFETIME(ABG_Character, bIsParachuteOpen);
	DOREPLIFETIME(ABG_Character, bIsOutsideBlueZone);
}
