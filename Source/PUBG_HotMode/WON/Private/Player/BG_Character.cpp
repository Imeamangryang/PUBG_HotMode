#include "Player/BG_Character.h"
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

DEFINE_LOG_CATEGORY(LogBGCharacter);

ABG_Character::ABG_Character()
{
	PrimaryActorTick.bCanEverTick = true;
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

	bIsWeaponEquipped = false;
	EquippedWeaponPoseType = EBGWeaponPoseType::None;
	bIsAiming = false;
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
	if (bIsDead || !bCanEnterCrouch) return;

	// 1. 만약 엎드려 있었다면 엎드리기 해제 (즉시)
	if (bIsProne) 
	{
		bIsProne = false;
		// 엎드려 있다가 Crouch 키를 누르면 바로 앉은 상태로 가게 하거나, 
		// 혹은 서게 한 뒤 Crouch를 수행하게 할 수 있습니다.
	}

	// 2. 언리얼 기본 Crouch 상태 토글
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
	if (bIsDead || !bCanEnterProne) return;

	// 1. 만약 앉아 있었다면 앉기 해제 (중요!)
	if (bIsCrouched)
	{
		UnCrouch();
	}

	// 2. 엎드리기 토글
	bIsProne = !bIsProne;

	// 3. 만약 엎드리기로 들어갔다면, 앉기 변수도 확실히 False 처리
	if (bIsProne)
	{
		// UnCrouch()가 내부적으로 bIsCrouched를 false로 만들지만, 
		// 애니메이션 블루프린트 전달용 변수가 있다면 여기서 명시적 정리
	}

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
		bHasParachute = false; // 한 번 펴면 소모됨
        
		// 낙하산이 펴지면 하강 속도를 물리적으로 제한
		GetCharacterMovement()->AirControl = 1.0f; // 공중 제어력 증가
		GetCharacterMovement()->GravityScale = 0.1f; // 천천히 떨어짐
	}
}

void ABG_Character::SetWeaponState(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped)
{
	const bool bNewEffectiveWeaponEquipped = bNewWeaponEquipped && NewWeaponPoseType != EBGWeaponPoseType::None;
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

void ABG_Character::FinishTimedCharacterState(EBGCharacterState ExpectedCharacterState)
{
	if (CharacterState != ExpectedCharacterState || CharacterState == EBGCharacterState::Dead)
	{
		return;
	}

	CharacterState = EBGCharacterState::Idle;
	UpdateDerivedState();
}

FName ABG_Character::GetDesiredWeaponSocketName(EBG_EquipmentSlot WeaponSlot) const
{
	if (EquipmentComponent && EquipmentComponent->GetActiveWeaponSlot() == WeaponSlot)
	{
		return WeaponHandSocketName;
	}

	return WeaponBackSocketName;
}

void ABG_Character::RequestPickupWorldItem(ABG_WorldItemBase* WorldItem, int32 Quantity)
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

	Server_RequestPickupWorldItem(WorldItem, Quantity);
}

void ABG_Character::Server_RequestPickupWorldItem_Implementation(
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
			RequestPickupWorldItem(WorldItem, WorldItem->GetQuantity());
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
		OnParachuteAction();
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: TryInteractWithCurrentTarget failed because there was no interactable target in range."), *GetNameSafe(this));
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
		CharacterState = EBGCharacterState::Dead;
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

void ABG_Character::ClearTimedCharacterState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CharacterStateTimerHandle);
		return;
	}

	UE_LOG(LogBGCharacter, Error, TEXT("%s: ClearTimedCharacterState failed because World was null."), *GetNameSafe(this));
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
