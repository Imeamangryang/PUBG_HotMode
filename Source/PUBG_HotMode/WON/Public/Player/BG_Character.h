#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "BG_Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UBG_DamageSystem;
class UBG_EquipmentComponent;
class UBG_HealthComponent;
class UBG_WeaponFireComponent;
class UAnimMontage;
class USceneComponent;
class UBoxComponent;
class USphereComponent;
class UPrimitiveComponent;
class AActor;
class UParkourComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogBGCharacter, Log, All);

UENUM(BlueprintType)
enum class EBGWeaponPoseType : uint8
{
	None UMETA(DisplayName = "None"),
	Pistol UMETA(DisplayName = "Pistol"),
	Rifle UMETA(DisplayName = "Rifle"),
	Shotgun UMETA(DisplayName = "Shotgun")
};

UENUM(BlueprintType)
enum class EBGCharacterState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Reloading   UMETA(DisplayName = "Reloading"), // 장전 중
 	Equipping   UMETA(DisplayName = "Equipping"), // 총 꺼내는 중
	MeleeAttacking UMETA(DisplayName = "MeleeAttacking"), // 근접 공격 중
	Dead        UMETA(DisplayName = "Dead")       // 사망
};

UENUM(BlueprintType)
enum class EBGCharacterStance : uint8
{
	Standing  UMETA(DisplayName = "Standing"),
	Crouching UMETA(DisplayName = "Crouching"),
	Prone     UMETA(DisplayName = "Prone")
};

UENUM(BlueprintType)
enum class EBGLeanDirection : uint8
{
	None  UMETA(DisplayName = "None"),
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};

UCLASS()
class PUBG_HOTMODE_API ABG_Character : public ACharacter
{
	GENERATED_BODY()

public:
	ABG_Character();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 공격 네트워크 로직
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ExecuteMeleeAttack();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackEffects();

	void ProcessMeleeHitDetection();

	UFUNCTION()
	void OnRep_EquippedWeaponPoseType();

	UFUNCTION()
	void HandleHealthDeathStateChanged(bool bNewIsDead);

	void ApplyWeaponMovementState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** 컨트롤러(BG_PlayerController)에서 호출하는 실행 함수들 */
	void MoveFromInput(const FInputActionValue& Value);
	void LookFromInput(const FInputActionValue& Value);
	void StartJumpFromInput();
	void StopJumpFromInput();
	void StartPrimaryActionFromInput();
	void StopPrimaryActionFromInput();
	void InteractFromInput();
	void Req_PrimaryAction();
	void StartAimFromInput();
	void StopAimFromInput();
	void ToggleCrouchFromInput();
	void ToggleProneFromInput();
	void StartLeanLeftFromInput();
	void StopLeanLeftFromInput();
	void StartLeanRightFromInput();
	void StopLeanRightFromInput();

	/** 공중에 떠 있는가? (점프/낙하) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsFalling;
	
	/** 이동 중인가? (속도가 0보다 큰가) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsAcceleration;
	
	/** 현재 무기를 소지하고 있는가? */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	bool bHasWeapon;
	
	/** 현재 캐릭터의 행위 상태 (장전, 사망 등) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	EBGCharacterState CharacterState = EBGCharacterState::Idle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	EBGCharacterStance CharacterStance = EBGCharacterStance::Standing;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	EBGLeanDirection LeanDirection = EBGLeanDirection::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanAim = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanReload = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bIsReloading = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanEnterProne = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bIsProne = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanEnterCrouch = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bIsCrouchingState = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanLean = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanFire = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bCanUseMeleeAttack = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "State")
	bool bIsDead = false;
	
	
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWeaponEquipped() const { return bIsWeaponEquipped; }

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	bool IsAiming();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	EBGWeaponPoseType GetEquippedWeaponPoseType() const { return EquippedWeaponPoseType; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AActor* GetCurrentInteractableWeapon() const { return CurrentInteractableWeapon; }

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool CanAim();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool CanReload();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool IsReloading();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool CanEnterProne();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool IsProneState();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool CanEnterCrouch();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool IsCrouchingState();
	
	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool CanLean();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool CanFire();

	UFUNCTION(BlueprintNativeEvent, Category = "State")
	bool IsDeadState();
	
	// 낙하산 보유 여부
	// --- ABG_Character.h ---
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State|Parachute")
	bool bHasParachute = true; // 시작할 때 가지고 있다고 가정

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State|Parachute")
	bool bIsParachuteOpen = false;

	// 상호작용으로 호출될 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "State|Parachute")
	void OnParachuteAction();

	UFUNCTION(BlueprintPure, Category = "State")
	EBGCharacterState GetCharacterState() const { return CharacterState; }

	UFUNCTION(BlueprintPure, Category = "State")
	EBGCharacterStance GetCharacterStance() const { return CharacterStance; }

	UFUNCTION(BlueprintPure, Category = "State")
	EBGLeanDirection GetLeanDirection() const { return LeanDirection; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponState(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetWeaponState(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetCurrentInteractableWeapon(AActor* NewInteractableWeapon);

	UFUNCTION(BlueprintPure, Category = "Health")
	class UBG_HealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	class UBG_InventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	class UBG_EquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

private:
	UFUNCTION()
	void OnInteractionRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void UpdateDerivedState();
	void UpdateActionAvailability();
	void UpdateCharacterStance();
	bool CanStartAim() const;
	void RefreshCurrentInteractableWeapon();
	bool IsWeaponInteractableActor(const AActor* CandidateActor) const;
	void TryInteractWithCurrentTarget();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_DamageSystem> DamageSystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_HealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UBG_InventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_EquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_WeaponFireComponent> WeaponFireComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> WeaponAttachPoint;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> HitCollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionRangeSphere;
	
	//파쿠르 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UParkourComponent> ParkourComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	float InteractionRangeRadius = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	FName WeaponInteractableCollisionProfileName = TEXT("Weapon");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AActor>> NearbyInteractableWeapons;

public:
	/** 공격 설정 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	TObjectPtr<UAnimMontage> MeleePunchMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	float MeleeDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	float MeleeRange = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	float MeleeRadius = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	float MeleeAttackCooldown = 1.5f;

	float LastMeleeAttackTime = -2.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Combat")
	bool bIsWeaponEquipped = false;
 
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedWeaponPoseType, Category = "Weapon")
	EBGWeaponPoseType EquippedWeaponPoseType = EBGWeaponPoseType::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Weapon")
	bool bIsAiming = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AActor> CurrentInteractableWeapon = nullptr;

	//UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	//TArray<TObjectPtr<AActor>> NearbyWeapons;
	
	
	
	virtual void Tick(float DeltaSeconds) override;
};
