#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_ItemTypes.h"
#include "Net/UnrealNetwork.h"
#include "BG_Character.generated.h"
 
class ABG_WorldItemBase;
class ABG_EquippedItemBase;
class ABG_EquippedWeaponBase;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UCameraShakeBase;
class UBG_DamageSystem;
class UBG_EquipmentComponent;
class UBG_HealthComponent;
class UBG_ItemUseComponent;
class UBG_InteractionAnimationComponent;
class UBG_WeaponFireComponent;
class UAnimMontage;
class USceneComponent;
class UBoxComponent;
class USphereComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class AActor;
class UParkourComponent;
class UCompassComponent;
enum class EBG_EquipmentSlot : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogBGCharacter, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBGNearbyWorldItemsChanged);

UENUM(BlueprintType)
enum class EBGWeaponPoseType : uint8
{
	None UMETA(DisplayName = "None"),
	Pistol UMETA(DisplayName = "Pistol"),
	Rifle UMETA(DisplayName = "Rifle"),
	Shotgun UMETA(DisplayName = "Shotgun"),
	Sniper UMETA(DisplayName = "Sniper")
};

UENUM(BlueprintType)
enum class EBGCharacterState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Reloading   UMETA(DisplayName = "Reloading"), // 장전 중
	Equipping   UMETA(DisplayName = "Equipping"), // 총 꺼내는 중
	Scoped      UMETA(DisplayName = "Scoped"), // 스코프 UI 및 정조준 상태
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual bool CanJumpInternal_Implementation() const override;

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

	UFUNCTION()
	void HandleDamaged(float DamageAmount, float CurrentHP, float MaxHP, bool bNewIsDead);

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
	void ReloadFromInput();
	void Req_PrimaryAction();
	void StartAimFromInput();
	void StopAimFromInput();
	void ToggleCrouchFromInput();
	void ToggleProneFromInput();
	void RefreshMovementSpeed();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetProneState(bool bNewIsProne);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetCharacterStanceState(EBGCharacterStance NewCharacterStance);

	UFUNCTION(Client, Unreliable)
	void Client_ApplyRecoil(float PitchDelta, float YawDelta, TSubclassOf<UCameraShakeBase> CameraShakeClass, float CameraShakeScale);

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
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CharacterStance, Category = "State", meta = (AllowPrivateAccess = "true"))
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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_IsProne, Category = "State", meta = (AllowPrivateAccess = "true"))
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
	ABG_EquippedWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AActor* GetCurrentInteractableWeapon() const { return CurrentInteractableWeapon; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	ABG_WorldItemBase* GetCurrentWorldItem() const;

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
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_IsParachuteOpen, Category = "State|Parachute")
	bool bIsParachuteOpen = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State|Parachute")
	bool bIsParachuteFalling = false;

	bool bIsJumpFalling = false;
	
	UFUNCTION(Server, Reliable)
	void Server_BeginAirplaneDrop(const FVector& DropLocation, const FVector& DropForwardVector);

	void BeginAirplaneDrop(const FVector& DropLocation, const FVector& DropForwardVector);
	
	UFUNCTION(Server, Reliable)
	void Server_OpenParachute();
	
	UFUNCTION(Client, Reliable)
	void Client_RestoreDefaultCamera();
	
	UFUNCTION(Client, Reliable)
	void Client_ApplyParachuteCamera();
	
	UFUNCTION()
	void OnRep_IsParachuteOpen();
	
	UFUNCTION(BlueprintPure, Category = "BlueZone")
	bool IsOutsideBlueZone() const { return bIsOutsideBlueZone; }

	void SetIsOutsideBlueZone(bool bNewIsOutsideBlueZone);
	
	UFUNCTION()
	void OnRep_IsOutsideBlueZone();
	
	UFUNCTION(BlueprintPure, Category = "BlueZone")
	bool CanReceiveBlueZoneDamage() const { return bCanReceiveBlueZoneDamage; }

	void SetCanReceiveBlueZoneDamage(bool bNewCanReceiveBlueZoneDamage);
	
	UFUNCTION(BlueprintPure, Category = "State|SkyDive")
	bool IsSkyDiving() const { return bIsSkyDiving; }

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

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetEquippedWeapon(ABG_EquippedWeaponBase* NewEquippedWeapon);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetWeaponState(EBGWeaponPoseType NewWeaponPoseType, bool bNewWeaponEquipped);

	bool SetInfiniteAmmo(bool bNewUseInfiniteAmmo);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetInfiniteAmmo(bool bNewUseInfiniteAmmo);

	bool SetHealthPercent(float NewHealthPercent);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetHealthPercent(float NewHealthPercent);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetCurrentInteractableWeapon(AActor* NewInteractableWeapon);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void PickupWorldItem(ABG_WorldItemBase* WorldItem, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
	bool GiveInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity, int32& OutAddedQuantity);

	UFUNCTION(Server, Reliable)
	void Server_PickupWorldItem(ABG_WorldItemBase* WorldItem, int32 Quantity);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_GiveInventoryItem(EBG_ItemType ItemType, FGameplayTag ItemTag, int32 Quantity);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveInventoryFailure(EBGInventoryFailReason FailReason, EBG_ItemType ItemType,
	                                    FGameplayTag ItemTag);

	UFUNCTION(BlueprintPure, Category = "Health")
	class UBG_HealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	class UBG_InventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	class UBG_EquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	class UBG_ItemUseComponent* GetItemUseComponent() const { return ItemUseComponent; }

	UFUNCTION(BlueprintPure, Category = "Animation")
	UBG_WeaponFireComponent* GetWeaponFireComponent() const { return WeaponFireComponent; }
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCompassComponent> CompassComponent;

	UFUNCTION(BlueprintPure, Category = "Animation")
	UBG_InteractionAnimationComponent* GetInteractionAnimationComponent() const { return InteractionAnimationComponent; }

	UFUNCTION(BlueprintPure, Category = "Animation")
	USkeletalMeshComponent* GetBodyAnimationMesh();

	UFUNCTION(BlueprintPure, Category = "Equipment|Backpack")
	ABG_EquippedItemBase* GetBackpackVisualActor() const { return BackpackVisualActor; }

	UFUNCTION(BlueprintCallable, Category = "Equipment|Backpack")
	void ApplyBackpackVisual(TSubclassOf<ABG_EquippedItemBase> BackpackEquippedItemClass,
	                         FGameplayTag BackpackItemTag);

	UFUNCTION(BlueprintCallable, Category = "Equipment|Backpack")
	void ClearBackpackVisual();

	UFUNCTION(BlueprintCallable, Category = "Equipment|Backpack")
	void RefreshBackpackVisualAttachment();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<ABG_WorldItemBase*> GetNearbyWorldItems() const;

	void NotifySuccessfulPickup(EBG_ItemType PickedUpItemType);

	UFUNCTION(BlueprintCallable, Category = "State")
	void SetCharacterStateValue(EBGCharacterState NewCharacterState);

	UFUNCTION(BlueprintCallable, Category = "State")
	void StartTimedCharacterState(EBGCharacterState NewCharacterState, float Duration);

	UFUNCTION(BlueprintCallable, Category = "State")
	void FinishTimedCharacterState(EBGCharacterState ExpectedCharacterState);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FName GetDesiredWeaponSocketName(EBG_EquipmentSlot WeaponSlot) const;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnBGNearbyWorldItemsChanged OnNearbyWorldItemsChanged;
	

private:
	UFUNCTION()
	void OnInteractionRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractionRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void UpdateDerivedState();
	void UpdateActionAvailability();
	void UpdateCharacterStance();
	void ApplyMovementSpeedForStance();
	void ApplyCharacterStanceState(EBGCharacterStance NewCharacterStance);
	void ClearTimedCharacterState();
	bool CanStartAim() const;
	void RefreshCurrentInteractableWeapon();
	bool IsWeaponInteractableActor(const AActor* CandidateActor) const;
	void TryInteractWithCurrentTarget();
	void ApplySkyDiveCamera();
	void ApplyParachuteCamera();
	void RestoreDefaultCamera();
	void ResetAfterSkyDiveLanding();	
	void ResetAirStateTracking();
	void HandleLandingFromAir();
	void UpdateParachuteVisibility();
	void RefreshMovementState();
	void StartFallingStateMonitor();
	void StopFallingStateMonitor();
	void EvaluateFallingState();
	ABG_EquippedItemBase* SpawnBackpackVisualActor(TSubclassOf<ABG_EquippedItemBase> BackpackEquippedItemClass,
	                                                FGameplayTag BackpackItemTag);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_ItemUseComponent> ItemUseComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_WeaponFireComponent> WeaponFireComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_InteractionAnimationComponent> InteractionAnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> WeaponAttachPoint;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	FName WeaponHandSocketName = TEXT("hand_r_weapon_socket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	FName WeaponBackSocketName = TEXT("spine_weapon_socket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Backpack", meta = (AllowPrivateAccess = "true"))
	FName BackpackSocketName = TEXT("spine_backpack_socket");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Equipment|Backpack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ABG_EquippedItemBase> BackpackVisualActor;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> HitCollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionRangeSphere;
	
	// 파쿠르 시스템
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UParkourComponent> ParkourComponent;
	
	// 낙하산 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parachute", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ParachuteMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parachute", meta = (AllowPrivateAccess = "true"))
	FVector ParachuteRelativeLocation = FVector(0.0f, 0.0f, 680.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parachute", meta = (AllowPrivateAccess = "true"))
	FRotator ParachuteRelativeRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Parachute", meta = (AllowPrivateAccess = "true"))
	FVector ParachuteRelativeScale = FVector(0.5);
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State|SkyDive", meta = (AllowPrivateAccess = "true"))
	bool bIsSkyDiving = false;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "BlueZone", meta = (AllowPrivateAccess = "true"))
	bool bCanReceiveBlueZoneDamage = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	float InteractionRangeRadius = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float StandingWalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float CrouchWalkSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ProneWalkSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float ItemUseMovementSpeedMultiplier = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	FName WeaponInteractableCollisionProfileName = TEXT("Weapon");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AActor>> NearbyInteractableWeapons;

public:
	/** 공격 설정 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	TObjectPtr<UAnimMontage> MeleePunchMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactMontage;

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
	TObjectPtr<ABG_EquippedWeaponBase> EquippedWeapon = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AActor> CurrentInteractableWeapon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Parachute", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ParachuteFallThreshold = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Parachute", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ParachuteFallMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Parachute", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ParachuteDeployMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State|Parachute", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ParachuteLandingMontage = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State|Parachute", meta = (AllowPrivateAccess = "true"))
	float FallingStartZ = 0.f;

	FTimerHandle CharacterStateTimerHandle;
	FTimerHandle FallingStateMonitorTimerHandle;

	//UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	//TArray<TObjectPtr<AActor>> NearbyWeapons;
	
	UPROPERTY(Transient)
	float DefaultCameraBoomLength = 150.0f;

	UPROPERTY(Transient)
	FVector DefaultCameraBoomSocketOffset = FVector(0.0f, 40.0f, 60.0f);
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_IsOutsideBlueZone, Category = "BlueZone", meta = (AllowPrivateAccess = "true"))
	bool bIsOutsideBlueZone = false;
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPickupMontage(EBG_ItemType PickedUpItemType);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHitReactMontage();
	
	// 스탠딩/크라우치/엎드리기 상태 변경 OnRep
	UFUNCTION()
	void OnRep_CharacterStance();
	
	UFUNCTION()
	void OnRep_IsProne();
	
};
