#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/BG_Character.h"
#include "BG_WeaponFireComponent.generated.h"

class UAnimMontage;
class UBG_DamageSystem;

UENUM(BlueprintType)
enum class EBGWeaponFireMode : uint8
{
	SemiAuto,
	FullAuto
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGWeaponAmmoChanged, int32, CurrentAmmo, int32, MaxAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBGWeaponHitIndicatorChanged, bool, bDidHit, FVector, ImpactLocation);

USTRUCT(BlueprintType)
struct FBGWeaponFireSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Range = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 PelletCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float SpreadAngleDegrees = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	float DebugSphereRadius = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	float DebugDrawDuration = 1.0f;
};

USTRUCT(BlueprintType)
struct FBGWeaponAmmoSettings
{
	GENERATED_BODY()

	// 임시 장탄 수다. 나중에 아이템/인벤토리 데이터가 들어오면 이 값만 교체하면 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 MaxMagazineAmmo = 0;

	// 임시 예비 탄약 수다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	int32 MaxReserveAmmo = 0;
};

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UBG_WeaponFireComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBG_WeaponFireComponent();

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
	void RequestFire();

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
	void RequestStartFire();

	UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
	void RequestStopFire();

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	bool CanFireWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	int32 GetCurrentMagazineAmmo() const { return CurrentMagazineAmmo; }

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	int32 GetMaxMagazineAmmo() const { return CurrentMaxMagazineAmmo; }

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	int32 GetCurrentReserveAmmo() const { return CurrentReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "Combat|Weapon")
	EBGWeaponPoseType GetCurrentWeaponPoseType() const { return CurrentWeaponPoseType; }

	// 임시 장착 시스템이 바뀌면 여기서 초기 탄약 프로파일도 함께 받아온다.
	UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
	void ApplyTemporaryWeaponProfile(EBGWeaponPoseType WeaponPoseType);

	UPROPERTY(BlueprintAssignable, Category = "Combat|Weapon")
	FOnBGWeaponAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Weapon")
	FOnBGWeaponHitIndicatorChanged OnHitIndicatorChanged;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestSingleFire();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StartFire();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StopFire();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayWeaponFireDebug(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, FVector_NetQuantize ImpactPoint, bool bDidHit, EBGWeaponPoseType WeaponPoseType);

	UFUNCTION()
	void OnRep_AmmoState();

private:
	const FBGWeaponFireSettings* ResolveFireSettings(EBGWeaponPoseType WeaponPoseType) const;
	const FBGWeaponAmmoSettings* ResolveAmmoSettings(EBGWeaponPoseType WeaponPoseType) const;
	EBGWeaponFireMode ResolveFireMode(EBGWeaponPoseType WeaponPoseType) const;
	bool ExecuteFire();
	bool TraceSingleShot(const FBGWeaponFireSettings& Settings, const FVector& TraceStart, const FVector& ShotDirection, FHitResult& OutHit) const;
	void ApplyHitResult(const FHitResult& HitResult, const FBGWeaponFireSettings& Settings) const;
	void DrawFireDebug(const FVector& TraceStart, const FVector& TraceEnd, const FVector& ImpactLocation, bool bDidHit, const FBGWeaponFireSettings& Settings, EBGWeaponPoseType WeaponPoseType) const;
	void PlayWeaponFireAnimation(EBGWeaponPoseType WeaponPoseType);
	void BroadcastAmmoState() const;
	void BroadcastHitIndicator(bool bDidHit, const FVector& ImpactLocation) const;
	bool ConsumeAmmo(int32 AmmoCost);
	void StartAutomaticFire();
	void StopAutomaticFire();
	void HandleAutomaticFire();

private:
	UPROPERTY()
	TObjectPtr<ABG_Character> CachedCharacter = nullptr;

	UPROPERTY()
	TObjectPtr<UBG_DamageSystem> DamageSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Weapon", meta = (AllowPrivateAccess = "true"))
	FBGWeaponFireSettings PistolFireSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Weapon", meta = (AllowPrivateAccess = "true"))
	FBGWeaponFireSettings RifleFireSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Weapon", meta = (AllowPrivateAccess = "true"))
	FBGWeaponFireSettings ShotgunFireSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	FBGWeaponAmmoSettings PistolAmmoSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	FBGWeaponAmmoSettings RifleAmmoSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Weapon|Ammo", meta = (AllowPrivateAccess = "true"))
	FBGWeaponAmmoSettings ShotgunAmmoSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapon|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> PistolFireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapon|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> RifleFireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapon|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> ShotgunFireMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Weapon|Animation", meta = (AllowPrivateAccess = "true"))
	FName FireMontageSlotName = TEXT("UpperBody");

	UPROPERTY(ReplicatedUsing = OnRep_AmmoState)
	int32 CurrentMagazineAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AmmoState)
	int32 CurrentReserveAmmo = 0;

	UPROPERTY(ReplicatedUsing = OnRep_AmmoState)
	int32 CurrentMaxMagazineAmmo = 0;

	UPROPERTY(Replicated)
	EBGWeaponPoseType CurrentWeaponPoseType = EBGWeaponPoseType::None;

	float LastFireTime = -1000.f;
	bool bIsHoldingFireInput = false;
	FTimerHandle AutomaticFireTimerHandle;
};
