#include "Combat/BG_WeaponFireComponent.h"

#include "Combat/BG_DamageSystem.h"
#include "Combat/BG_EquippedWeaponBase.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "Inventory/BG_InventoryComponent.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemDataRow.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Player/BG_Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"


DEFINE_LOG_CATEGORY_STATIC(LogBGWeaponFire, Log, All);

UBG_WeaponFireComponent::UBG_WeaponFireComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// 임시 무기 스펙이다. 다른 팀원의 아이템/무기 데이터가 들어오면 이 블록을 교체하면 된다.
	PistolFireSettings.Damage = 25.f;
	PistolFireSettings.Range = 2500.f;
	PistolFireSettings.FireCooldown = 0.20f;
	PistolFireSettings.PelletCount = 1;
	PistolFireSettings.SpreadAngleDegrees = 0.25f;

	RifleFireSettings.Damage = 35.f;
	RifleFireSettings.Range = 3500.f;
	RifleFireSettings.FireCooldown = 0.10f;
	RifleFireSettings.PelletCount = 1;
	RifleFireSettings.SpreadAngleDegrees = 0.5f;

	// 샷건은 아직 연동 전이므로 일단 더미 값만 둔다.
	ShotgunFireSettings.Damage = 9.f;
	ShotgunFireSettings.Range = 1600.f;
	ShotgunFireSettings.FireCooldown = 0.85f;
	ShotgunFireSettings.PelletCount = 8;
	ShotgunFireSettings.SpreadAngleDegrees = 6.0f;

	SniperFireSettings.Damage = 95.f;
	SniperFireSettings.Range = 12000.f;
	SniperFireSettings.FireCooldown = 1.25f;
	SniperFireSettings.PelletCount = 1;
	SniperFireSettings.SpreadAngleDegrees = 0.02f;

	PistolAmmoSettings.MaxMagazineAmmo = 12;
	PistolAmmoSettings.MaxReserveAmmo = 48;

	RifleAmmoSettings.MaxMagazineAmmo = 30;
	RifleAmmoSettings.MaxReserveAmmo = 90;

	ShotgunAmmoSettings.MaxMagazineAmmo = 8;
	ShotgunAmmoSettings.MaxReserveAmmo = 24;

	SniperAmmoSettings.MaxMagazineAmmo = 5;
	SniperAmmoSettings.MaxReserveAmmo = 25;
}

void UBG_WeaponFireComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ABG_Character>(GetOwner());
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: BeginPlay failed because owner was not ABG_Character."), *GetNameSafe(this));
		return;
	}

	DamageSystem = CachedCharacter->FindComponentByClass<UBG_DamageSystem>();
	if (!DamageSystem)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: BeginPlay failed because DamageSystem was not found on %s."), *GetNameSafe(this), *GetNameSafe(CachedCharacter));
	}

	if (CachedCharacter->IsWeaponEquipped())
	{
		SyncAmmoFromEquipment();
	}
}

void UBG_WeaponFireComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentMagazineAmmo);
	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentReserveAmmo);
	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentMaxMagazineAmmo);
	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentWeaponPoseType);
	DOREPLIFETIME(UBG_WeaponFireComponent, FireAnimationSequence);
}

void UBG_WeaponFireComponent::RequestFire()
{
	RequestStartFire();
}

void UBG_WeaponFireComponent::RequestStartFire()
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: RequestStartFire failed because CachedCharacter was null."), *GetNameSafe(this));
		return;
	}

	bIsHoldingFireInput = true;

	const EBGWeaponPoseType WeaponPoseType = CachedCharacter->GetEquippedWeaponPoseType();
	const EBGWeaponFireMode FireMode = ResolveFireMode(WeaponPoseType);

	if (FireMode == EBGWeaponFireMode::FullAuto)
	{
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			StartAutomaticFire();
			return;
		}

		Server_StartFire();
		return;
	}

	if (!CanFireWeapon())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: RequestStartFire failed because firing was blocked. Character=%s, WeaponEquipped=%d, CanFire=%d, Pose=%s, Ammo=%d"),
			*GetNameSafe(this),
			*GetNameSafe(CachedCharacter),
			CachedCharacter->IsWeaponEquipped() ? 1 : 0,
			CachedCharacter->CanFire() ? 1 : 0,
			*UEnum::GetValueAsString(WeaponPoseType),
			CurrentMagazineAmmo);
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ExecuteFire();
		return;
	}

	Server_RequestSingleFire();
}

void UBG_WeaponFireComponent::RequestStopFire()
{
	bIsHoldingFireInput = false;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		StopAutomaticFire();
		return;
	}

	Server_StopFire();
}

void UBG_WeaponFireComponent::RequestReload()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TryStartReload();
		return;
	}

	Server_RequestReload();
}

bool UBG_WeaponFireComponent::CanFireWeapon() const
{
	if (!CachedCharacter)
	{
		return false;
	}

	const ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("CanFireWeapon"));
	if (!EquippedWeapon)
	{
		return false;
	}

	const EBGWeaponPoseType WeaponPoseType = CachedCharacter->GetEquippedWeaponPoseType();
	return CachedCharacter->IsWeaponEquipped()
		&& CachedCharacter->CanFire()
		&& WeaponPoseType != EBGWeaponPoseType::None
		&& ResolveFireSettings(WeaponPoseType) != nullptr
		&& EquippedWeapon->CanFire();
}

bool UBG_WeaponFireComponent::CanReloadWeapon() const
{
	if (!CachedCharacter || !CachedCharacter->CanReload())
	{
		UE_LOG(LogBGWeaponFire, Warning, TEXT("%s: CanReloadWeapon false because character cannot reload. bHasWeapon=%d, bIsReloading=%d, bIsDead=%d"),
			*GetNameSafe(this), CachedCharacter ? static_cast<int32>(CachedCharacter->IsWeaponEquipped()) : 0,
			CachedCharacter ? static_cast<int32>(CachedCharacter->IsReloading()) : 0,
			CachedCharacter ? static_cast<int32>(CachedCharacter->IsDeadState()) : 0);
		return false;
	}

	const ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("CanReloadWeapon"));
	if (!EquippedWeapon)
	{
		return false;
	}

	FGameplayTag WeaponItemTag;
	const FBG_WeaponItemDataRow* WeaponRow = nullptr;
	EBG_EquipmentSlot WeaponSlot = EBG_EquipmentSlot::None;
	if (!GetActiveWeaponContext(WeaponItemTag, WeaponRow, WeaponSlot, TEXT("CanReloadWeapon")) || !WeaponRow)
	{
		return EquippedWeapon->CanReloadWithInventoryAmmo(0);
	}

	const UBG_InventoryComponent* InventoryComponent = GetInventoryComponent(TEXT("CanReloadWeapon"));
	const int32 InventoryAmmoCount = (InventoryComponent && WeaponRow->AmmoItemTag.IsValid())
		? InventoryComponent->GetQuantity(EBG_ItemType::Ammo, WeaponRow->AmmoItemTag)
		: 0;
	return EquippedWeapon->CanReloadWithInventoryAmmo(InventoryAmmoCount);
}

float UBG_WeaponFireComponent::GetTimeSinceLastFireAnimation() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: GetTimeSinceLastFireAnimation failed because World was null."), *GetNameSafe(this));
		return TNumericLimits<float>::Max();
	}

	if (LastFireAnimationWorldTime < 0.f)
	{
		return TNumericLimits<float>::Max();
	}

	return FMath::Max(0.f, World->GetTimeSeconds() - LastFireAnimationWorldTime);
}

bool UBG_WeaponFireComponent::HasRecentFireAnimation(float MaxAgeSeconds) const
{
	if (MaxAgeSeconds <= 0.f)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: HasRecentFireAnimation failed because MaxAgeSeconds %.3f was not positive."), *GetNameSafe(this), MaxAgeSeconds);
		return false;
	}

	return GetTimeSinceLastFireAnimation() <= MaxAgeSeconds;
}

void UBG_WeaponFireComponent::ApplyTemporaryWeaponProfile(EBGWeaponPoseType WeaponPoseType)
{
	bIsHoldingFireInput = false;
	StopAutomaticFire();
	CancelReload();
	CurrentWeaponPoseType = WeaponPoseType;

	if (WeaponPoseType == EBGWeaponPoseType::None)
	{
		CurrentMagazineAmmo = 0;
		CurrentReserveAmmo = 0;
		CurrentMaxMagazineAmmo = 0;
		BroadcastAmmoState();
		return;
	}

	SyncAmmoFromEquipment();
}

void UBG_WeaponFireComponent::Server_RequestSingleFire_Implementation()
{
	ExecuteFire();
}

bool UBG_WeaponFireComponent::Server_RequestSingleFire_Validate()
{
	return true;
}

void UBG_WeaponFireComponent::Server_StartFire_Implementation()
{
	bIsHoldingFireInput = true;
	StartAutomaticFire();
}

bool UBG_WeaponFireComponent::Server_StartFire_Validate()
{
	return true;
}

void UBG_WeaponFireComponent::Server_StopFire_Implementation()
{
	bIsHoldingFireInput = false;
	StopAutomaticFire();
}

bool UBG_WeaponFireComponent::Server_StopFire_Validate()
{
	return true;
}

void UBG_WeaponFireComponent::Server_RequestReload_Implementation()
{
	TryStartReload();
}

bool UBG_WeaponFireComponent::Server_RequestReload_Validate()
{
	return true;
}

bool UBG_WeaponFireComponent::ExecuteFire()
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because CachedCharacter was null."), *GetNameSafe(this));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because World was null."), *GetNameSafe(this));
		return false;
	}

	const EBGWeaponPoseType WeaponPoseType = CachedCharacter->GetEquippedWeaponPoseType();
	ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("ExecuteFire"));
	if (!EquippedWeapon)
	{
		return false;
	}

	const FBGWeaponFireSettings* FireSettings = ResolveFireSettings(WeaponPoseType);
	if (!FireSettings)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because no fire settings existed for pose %s."),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(WeaponPoseType));
		return false;
	}

	SyncAmmoFromEquipment();

	if (!CachedCharacter->CanFire())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because character cannot fire right now. Character=%s, State=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CachedCharacter),
			*UEnum::GetValueAsString(CachedCharacter->GetCharacterState()));
		return false;
	}

	const float CurrentTime = World->TimeSeconds;
	if (CurrentTime - LastFireTime < FireSettings->FireCooldown)
	{
		return false;
	}

	// Weapon actor owns the authoritative loaded-ammo state, while this component only drives effects and input timing.
	int32 AmmoCost = 1;
	if (const FBG_WeaponFireSpecRow* FireSpecRow = GetActiveWeaponFireSpecRow(TEXT("ExecuteFire")))
	{
		AmmoCost = FMath::Max(1, FireSpecRow->AmmoCost);
	}

	if (!EquippedWeapon->ConsumeLoadedAmmo(AmmoCost))
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because ammo was empty. Pose=%s"), *GetNameSafe(this), *UEnum::GetValueAsString(WeaponPoseType));
		return false;
	}

	LastFireTime = CurrentTime;

	if (UBG_EquipmentComponent* EquipmentComponent = GetEquipmentComponent(TEXT("ExecuteFire")))
	{
		const EBG_EquipmentSlot ActiveWeaponSlot = EquipmentComponent->GetActiveWeaponSlot();
		if (ActiveWeaponSlot != EBG_EquipmentSlot::None)
		{
			EquipmentComponent->Auth_LoadAmmo(ActiveWeaponSlot, EquippedWeapon->GetLoadedAmmo());
		}
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	CachedCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector ShotStart = ViewLocation;
	const FVector BaseShotDirection = ViewRotation.Vector();

	FTransform MuzzleTransform;
	if (EquippedWeapon->GetMuzzleTransform(MuzzleTransform))
	{
		EquippedWeapon->NotifyFireTriggered(MuzzleTransform, FireAnimationSequence + 1);
	}

	const float RecoilPitchOffset = FireSettings->RecoilPitchDegrees > 0.f
		? FMath::RandRange(0.0f, FireSettings->RecoilPitchDegrees)
		: 0.f;
	const float RecoilYawOffset = FireSettings->RecoilYawDegrees > 0.f
		? FMath::RandRange(-FireSettings->RecoilYawDegrees, FireSettings->RecoilYawDegrees)
		: 0.f;

	const FVector RecoiledBaseDirection = FRotator(RecoilPitchOffset, RecoilYawOffset, 0.f).RotateVector(BaseShotDirection);

	if (CachedCharacter && CachedCharacter->GetLocalRole() == ROLE_Authority)
	{
		CachedCharacter->Client_ApplyRecoil(
			RecoilPitchOffset * 0.75f,
			RecoilYawOffset * 0.75f,
			FireSettings->CameraShakeClass,
			FireSettings->CameraShakeIntensity);
	}

	const int32 PelletCount = FMath::Max(1, FireSettings->PelletCount);
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		// This temporary debug path keeps hitscan firing enabled until projectile-based weapon actors are introduced.
		const float SpreadRadians = FMath::DegreesToRadians(FireSettings->SpreadAngleDegrees);
		const FVector ShotDirection = SpreadRadians > 0.f
			? FMath::VRandCone(RecoiledBaseDirection, SpreadRadians)
			: RecoiledBaseDirection;
		const FVector ShotEnd = ShotStart + (ShotDirection * FireSettings->Range);

		FHitResult HitResult;
		const bool bDidHit = TraceSingleShot(*FireSettings, ShotStart, ShotDirection, HitResult);
		if (bDidHit)
		{
			ApplyHitResult(HitResult, *FireSettings);
		}

		Multicast_PlayWeaponFireDebug(ShotStart, ShotEnd, bDidHit ? HitResult.ImpactPoint : ShotEnd, bDidHit, WeaponPoseType);
	}

	PlayWeaponFireAnimation(WeaponPoseType);
	MarkFireAnimationTriggered();
	SyncAmmoFromEquipment();
	BroadcastAmmoState();
	return true;
}

bool UBG_WeaponFireComponent::TraceSingleShot(const FBGWeaponFireSettings& Settings, const FVector& TraceStart, const FVector& ShotDirection, FHitResult& OutHit) const
{
	if (!CachedCharacter || !GetWorld())
	{
		return false;
	}

	const FVector TraceEnd = TraceStart + (ShotDirection * Settings.Range);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponFire), false, CachedCharacter);
	Params.bReturnPhysicalMaterial = false;

	return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, Params);
}

void UBG_WeaponFireComponent::ApplyHitResult(const FHitResult& HitResult, const FBGWeaponFireSettings& Settings) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!DamageSystem)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ApplyHitResult failed because DamageSystem was null."), *GetNameSafe(this));
		return;
	}

	AActor* TargetActor = HitResult.GetActor();
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogBGWeaponFire, Log, TEXT("%s: Shot hit world geometry or a non-actor surface. No damage was applied."), *GetNameSafe(this));
		return;
	}

	const bool bApplied = DamageSystem->ApplyDamageToActor(TargetActor, Settings.Damage);
	UE_LOG(LogBGWeaponFire, Warning, TEXT("%s: Server hit %s for %.1f damage. Applied=%d"),
		*GetNameSafe(CachedCharacter),
		*GetNameSafe(TargetActor),
		Settings.Damage,
		bApplied ? 1 : 0);
}

void UBG_WeaponFireComponent::DrawFireDebug(const FVector& TraceStart, const FVector& TraceEnd, const FVector& ImpactLocation, bool bDidHit, const FBGWeaponFireSettings& Settings, EBGWeaponPoseType WeaponPoseType) const
{
	if (!GetWorld())
	{
		return;
	}

	const FColor DebugColor = bDidHit ? FColor::Red : FColor::Yellow;
	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, DebugColor, false, Settings.DebugDrawDuration, 0, 1.5f);
	DrawDebugSphere(GetWorld(), ImpactLocation, Settings.DebugSphereRadius, 12, DebugColor, false, Settings.DebugDrawDuration);

	if (GEngine && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		const FString Message = FString::Printf(TEXT("%s %s %s"),
			*GetNameSafe(CachedCharacter),
			*UEnum::GetValueAsString(WeaponPoseType),
			bDidHit ? TEXT("Hit") : TEXT("Miss"));
		GEngine->AddOnScreenDebugMessage(-1, Settings.DebugDrawDuration, DebugColor, Message);
	}

	UE_LOG(LogBGWeaponFire, Warning, TEXT("%s: %s fired. Result=%s, Start=%s, End=%s"),
		*GetNameSafe(CachedCharacter),
		*UEnum::GetValueAsString(WeaponPoseType),
		bDidHit ? TEXT("Hit") : TEXT("Miss"),
		*TraceStart.ToString(),
		*TraceEnd.ToString());
}

void UBG_WeaponFireComponent::PlayWeaponFireAnimation(EBGWeaponPoseType WeaponPoseType)
{
	if (!CachedCharacter)
	{
		return;
	}

	USkeletalMeshComponent* BodyMesh = CachedCharacter->GetBodyAnimationMesh();
	if (!BodyMesh || !BodyMesh->GetAnimInstance())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: PlayWeaponFireAnimation failed because body mesh or anim instance was null."), *GetNameSafe(this));
		return;
	}

	UAnimMontage* MontageToPlay = nullptr;
	switch (WeaponPoseType)
	{
	case EBGWeaponPoseType::Pistol:
		MontageToPlay = PistolFireMontage;
		break;
	case EBGWeaponPoseType::Rifle:
		MontageToPlay = RifleFireMontage;
		break;
	case EBGWeaponPoseType::Shotgun:
		MontageToPlay = ShotgunFireMontage;
		break;
	case EBGWeaponPoseType::Sniper:
		MontageToPlay = SniperFireMontage;
		break;
	default:
		break;
	}

	if (MontageToPlay)
	{
		BodyMesh->GetAnimInstance()->Montage_Play(MontageToPlay, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
	}
}

void UBG_WeaponFireComponent::BroadcastAmmoState() const
{
	OnAmmoChanged.Broadcast(CurrentMagazineAmmo, CurrentMaxMagazineAmmo);
}

void UBG_WeaponFireComponent::BroadcastHitIndicator(bool bDidHit, const FVector& ImpactLocation) const
{
	OnHitIndicatorChanged.Broadcast(bDidHit, ImpactLocation);
}

bool UBG_WeaponFireComponent::ConsumeAmmo(int32 AmmoCost)
{
	if (AmmoCost <= 0)
	{
		return true;
	}

	if (CurrentMagazineAmmo < AmmoCost)
	{
		return false;
	}

	CurrentMagazineAmmo = FMath::Max(0, CurrentMagazineAmmo - AmmoCost);
	return true;
}

void UBG_WeaponFireComponent::MarkFireAnimationTriggered()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: MarkFireAnimationTriggered failed because World was null."), *GetNameSafe(this));
		return;
	}

	LastFireAnimationWorldTime = World->GetTimeSeconds();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		++FireAnimationSequence;
	}
}

void UBG_WeaponFireComponent::StartAutomaticFire()
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: StartAutomaticFire failed because CachedCharacter was null."), *GetNameSafe(this));
		return;
	}

	if (ResolveFireMode(CachedCharacter->GetEquippedWeaponPoseType()) != EBGWeaponFireMode::FullAuto)
	{
		if (!ExecuteFire())
		{
			UE_LOG(LogBGWeaponFire, Error, TEXT("%s: StartAutomaticFire fallback single shot failed."), *GetNameSafe(this));
		}
		return;
	}

	if (!GetWorld())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: StartAutomaticFire failed because World was null."), *GetNameSafe(this));
		return;
	}

	const FBGWeaponFireSettings* FireSettings = ResolveFireSettings(CachedCharacter->GetEquippedWeaponPoseType());
	if (!FireSettings)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: StartAutomaticFire failed because fire settings were null."), *GetNameSafe(this));
		return;
	}

	ExecuteFire();

	GetWorld()->GetTimerManager().SetTimer(
		AutomaticFireTimerHandle,
		this,
		&UBG_WeaponFireComponent::HandleAutomaticFire,
		FireSettings->FireCooldown,
		true);
}

void UBG_WeaponFireComponent::StopAutomaticFire()
{
	if (!GetWorld())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: StopAutomaticFire failed because World was null."), *GetNameSafe(this));
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
}

void UBG_WeaponFireComponent::HandleAutomaticFire()
{
	if (!bIsHoldingFireInput)
	{
		StopAutomaticFire();
		return;
	}

	if (!CanFireWeapon())
	{
		StopAutomaticFire();
		return;
	}

	if (!ExecuteFire())
	{
		StopAutomaticFire();
	}
}

UBG_EquipmentComponent* UBG_WeaponFireComponent::GetEquipmentComponent(const TCHAR* OperationName) const
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because CachedCharacter was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_EquipmentComponent* EquipmentComponent = CachedCharacter->GetEquipmentComponent();
	if (!EquipmentComponent)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because EquipmentComponent was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	return EquipmentComponent;
}

UBG_InventoryComponent* UBG_WeaponFireComponent::GetInventoryComponent(const TCHAR* OperationName) const
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because CachedCharacter was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_InventoryComponent* InventoryComponent = CachedCharacter->GetInventoryComponent();
	if (!InventoryComponent)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because InventoryComponent was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	return InventoryComponent;
}

ABG_EquippedWeaponBase* UBG_WeaponFireComponent::GetActiveEquippedWeapon(const TCHAR* OperationName) const
{
	UBG_EquipmentComponent* EquipmentComponent = GetEquipmentComponent(OperationName);
	if (!EquipmentComponent)
	{
		return nullptr;
	}

	ABG_EquippedWeaponBase* EquippedWeapon = EquipmentComponent->GetActiveEquippedWeaponActor();
	if (!EquippedWeapon)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because active EquippedWeapon was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	return EquippedWeapon;
}

UBG_ItemDataRegistrySubsystem* UBG_WeaponFireComponent::GetItemDataRegistrySubsystem(const TCHAR* OperationName) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because World was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because GameInstance was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because UBG_ItemDataRegistrySubsystem was null."), *GetNameSafe(this), OperationName);
		return nullptr;
	}

	return RegistrySubsystem;
}

const FBG_WeaponFireSpecRow* UBG_WeaponFireComponent::GetActiveWeaponFireSpecRow(const TCHAR* OperationName) const
{
	FGameplayTag WeaponItemTag;
	const FBG_WeaponItemDataRow* WeaponRow = nullptr;
	EBG_EquipmentSlot WeaponSlot = EBG_EquipmentSlot::None;
	if (!GetActiveWeaponContext(WeaponItemTag, WeaponRow, WeaponSlot, OperationName))
	{
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	if (!RegistrySubsystem)
	{
		return nullptr;
	}

	const FBG_WeaponFireSpecRow* FireSpecRow = RegistrySubsystem->FindWeaponFireSpecRow(WeaponItemTag);
	if (!FireSpecRow)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because weapon fire spec row was not found for %s."),
			*GetNameSafe(this),
			OperationName,
			*WeaponItemTag.ToString());
		return nullptr;
	}

	return FireSpecRow;
}

const FBG_WeaponItemDataRow* UBG_WeaponFireComponent::GetActiveWeaponItemRow(const TCHAR* OperationName) const
{
	FGameplayTag WeaponItemTag;
	const FBG_WeaponItemDataRow* WeaponRow = nullptr;
	EBG_EquipmentSlot WeaponSlot = EBG_EquipmentSlot::None;
	return GetActiveWeaponContext(WeaponItemTag, WeaponRow, WeaponSlot, OperationName) ? WeaponRow : nullptr;
}

bool UBG_WeaponFireComponent::GetActiveWeaponContext(
	FGameplayTag& OutWeaponItemTag,
	const FBG_WeaponItemDataRow*& OutWeaponRow,
	EBG_EquipmentSlot& OutWeaponSlot,
	const TCHAR* OperationName) const
{
	OutWeaponItemTag = FGameplayTag();
	OutWeaponRow = nullptr;
	OutWeaponSlot = EBG_EquipmentSlot::None;

	const UBG_EquipmentComponent* EquipmentComponent = GetEquipmentComponent(OperationName);
	if (!EquipmentComponent)
	{
		return false;
	}

	OutWeaponSlot = EquipmentComponent->GetActiveWeaponSlot();
	if (OutWeaponSlot == EBG_EquipmentSlot::None)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because no active weapon slot was selected."), *GetNameSafe(this), OperationName);
		return false;
	}

	OutWeaponItemTag = EquipmentComponent->GetActiveWeaponItemTag();
	if (!OutWeaponItemTag.IsValid())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because active weapon tag was invalid."), *GetNameSafe(this), OperationName);
		return false;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(OperationName);
	if (!RegistrySubsystem)
	{
		return false;
	}

	OutWeaponRow = RegistrySubsystem->FindTypedItemRow<FBG_WeaponItemDataRow>(EBG_ItemType::Weapon, OutWeaponItemTag);
	if (!OutWeaponRow)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because weapon row was not found for %s."), *GetNameSafe(this), OperationName, *OutWeaponItemTag.ToString());
		return false;
	}

	return true;
}

void UBG_WeaponFireComponent::SyncAmmoFromEquipment()
{
	if (ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("SyncAmmoFromEquipment")))
	{
		CurrentWeaponPoseType = EquippedWeapon->GetWeaponPoseType();
		CurrentMaxMagazineAmmo = EquippedWeapon->GetMagazineCapacity();
		CurrentMagazineAmmo = EquippedWeapon->GetLoadedAmmo();
		RefreshReserveAmmo();
		BroadcastAmmoState();
		return;
	}

	FGameplayTag WeaponItemTag;
	const FBG_WeaponItemDataRow* WeaponRow = nullptr;
	EBG_EquipmentSlot WeaponSlot = EBG_EquipmentSlot::None;
	if (!GetActiveWeaponContext(WeaponItemTag, WeaponRow, WeaponSlot, TEXT("SyncAmmoFromEquipment")) || !WeaponRow)
	{
		ApplyTemporaryAmmoProfileIfNeeded(TEXT("SyncAmmoFromEquipment"));
		BroadcastAmmoState();
		return;
	}

	if (UBG_EquipmentComponent* EquipmentComponent = GetEquipmentComponent(TEXT("SyncAmmoFromEquipment")))
	{
		CurrentMaxMagazineAmmo = FMath::Max(0, WeaponRow->MagazineSize);
		CurrentMagazineAmmo = FMath::Clamp(EquipmentComponent->GetLoadedAmmo(WeaponSlot), 0, CurrentMaxMagazineAmmo);
		RefreshReserveAmmo();
		BroadcastAmmoState();
	}
}

void UBG_WeaponFireComponent::RefreshReserveAmmo()
{
	if (const ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("RefreshReserveAmmo")))
	{
		if (EquippedWeapon->UsesInfiniteDebugAmmo())
		{
			// Comment out this block later when reserve ammo should come only from the replicated inventory count.
			CurrentReserveAmmo = 999;
			return;
		}
	}

	const FBG_WeaponItemDataRow* WeaponRow = GetActiveWeaponItemRow(TEXT("RefreshReserveAmmo"));
	if (!WeaponRow || !WeaponRow->AmmoItemTag.IsValid())
	{
		CurrentReserveAmmo = 0;
		return;
	}

	if (UBG_InventoryComponent* InventoryComponent = GetInventoryComponent(TEXT("RefreshReserveAmmo")))
	{
		CurrentReserveAmmo = FMath::Max(0, InventoryComponent->GetQuantity(EBG_ItemType::Ammo, WeaponRow->AmmoItemTag));
		return;
	}

	CurrentReserveAmmo = 0;
}

bool UBG_WeaponFireComponent::ApplyTemporaryAmmoProfileIfNeeded(const TCHAR* OperationName)
{
	const FBGWeaponAmmoSettings* AmmoSettings = ResolveAmmoSettings(CurrentWeaponPoseType);
	if (!AmmoSettings || CurrentWeaponPoseType == EBGWeaponPoseType::None)
	{
		CurrentMagazineAmmo = 0;
		CurrentReserveAmmo = 0;
		CurrentMaxMagazineAmmo = 0;
		return false;
	}

	const int32 NewMaxMagazineAmmo = FMath::Max(0, AmmoSettings->MaxMagazineAmmo);
	if (NewMaxMagazineAmmo <= 0)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: %s failed because temporary magazine size was invalid. Pose=%s"),
			*GetNameSafe(this),
			OperationName,
			*UEnum::GetValueAsString(CurrentWeaponPoseType));
		CurrentMagazineAmmo = 0;
		CurrentReserveAmmo = 0;
		CurrentMaxMagazineAmmo = 0;
		return false;
	}

	if (CurrentMaxMagazineAmmo != NewMaxMagazineAmmo)
	{
		CurrentMaxMagazineAmmo = NewMaxMagazineAmmo;
		CurrentMagazineAmmo = NewMaxMagazineAmmo;
		CurrentReserveAmmo = FMath::Max(0, AmmoSettings->MaxReserveAmmo);
	}

	return true;
}

bool UBG_WeaponFireComponent::TryStartTemporaryReload()
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: TryStartTemporaryReload failed because CachedCharacter was null."), *GetNameSafe(this));
		return false;
	}

	if (CurrentWeaponPoseType == EBGWeaponPoseType::None || CurrentMagazineAmmo >= CurrentMaxMagazineAmmo)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: TryStartTemporaryReload failed because temporary reload was not possible. CurrentMagazineAmmo=%d CurrentMaxMagazineAmmo=%d"), *GetNameSafe(this), CurrentMagazineAmmo, CurrentMaxMagazineAmmo);
		return false;
	}

	bIsHoldingFireInput = false;
	StopAutomaticFire();
	const float ReloadDuration = FMath::Max(0.f, TemporaryReloadDuration);
	CachedCharacter->StartTimedCharacterState(EBGCharacterState::Reloading, ReloadDuration);
	Multicast_PlayReloadAnimation(CachedCharacter->GetEquippedWeaponPoseType());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
		if (ReloadDuration <= KINDA_SMALL_NUMBER)
		{
			CompleteTemporaryReload();
			return true;
		}

		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UBG_WeaponFireComponent::CompleteTemporaryReload, ReloadDuration, false);
		return true;
	}

	UE_LOG(LogBGWeaponFire, Error, TEXT("%s: TryStartTemporaryReload failed because World was null."), *GetNameSafe(this));
	return false;
}

void UBG_WeaponFireComponent::CompleteTemporaryReload()
{
	const int32 AmmoNeeded = FMath::Max(0, CurrentMaxMagazineAmmo - CurrentMagazineAmmo);
	const int32 AmmoToLoad = FMath::Min(AmmoNeeded, CurrentReserveAmmo);
	if (AmmoToLoad <= 0)
	{
		if (AmmoNeeded > 0)
		{
			UE_LOG(LogBGWeaponFire, Warning, TEXT("%s: CompleteTemporaryReload using code-only refill because no reserve ammo is available."), *GetNameSafe(this));
			CurrentMagazineAmmo = CurrentMaxMagazineAmmo;
			BroadcastAmmoState();
			if (CachedCharacter)
			{
				CachedCharacter->FinishTimedCharacterState(EBGCharacterState::Reloading);
			}
			return;
		}

		CancelReload();
		return;
	}

	CurrentMagazineAmmo += AmmoToLoad;
	CurrentReserveAmmo -= AmmoToLoad;
	BroadcastAmmoState();

	if (CachedCharacter)
	{
		CachedCharacter->FinishTimedCharacterState(EBGCharacterState::Reloading);
	}
}

bool UBG_WeaponFireComponent::TryStartReload()
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: TryStartReload failed because CachedCharacter was null."), *GetNameSafe(this));
		return false;
	}

	if (!CanReloadWeapon())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: TryStartReload failed because reload was not currently possible. CurrentWeaponPoseType=%s, CurrentMagazineAmmo=%d, CurrentMaxMagazineAmmo=%d, CurrentReserveAmmo=%d"),
			*GetNameSafe(this), *UEnum::GetValueAsString(CurrentWeaponPoseType), CurrentMagazineAmmo, CurrentMaxMagazineAmmo, CurrentReserveAmmo);
		return false;
	}

	FGameplayTag WeaponItemTag;
	const FBG_WeaponItemDataRow* WeaponRow = nullptr;
	EBG_EquipmentSlot WeaponSlot = EBG_EquipmentSlot::None;
	if (!GetActiveWeaponContext(WeaponItemTag, WeaponRow, WeaponSlot, TEXT("TryStartReload")) || !WeaponRow)
	{
		UE_LOG(LogBGWeaponFire, Warning, TEXT("%s: TryStartReload active weapon context failed, falling back to temporary reload."), *GetNameSafe(this));
		return TryStartTemporaryReload();
	}

	ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("TryStartReload"));
	if (!EquippedWeapon || !EquippedWeapon->BeginWeaponReload())
	{
		return false;
	}

	bIsHoldingFireInput = false;
	StopAutomaticFire();
	CachedCharacter->StartTimedCharacterState(EBGCharacterState::Reloading, WeaponRow->ReloadDuration);
	EquippedWeapon->NotifyReloadStarted(WeaponRow->ReloadDuration);
	Multicast_PlayReloadAnimation(CachedCharacter->GetEquippedWeaponPoseType());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
		if (WeaponRow->ReloadDuration <= KINDA_SMALL_NUMBER)
		{
			CompleteReload();
			return true;
		}

		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UBG_WeaponFireComponent::CompleteReload, WeaponRow->ReloadDuration, false);
		return true;
	}

	UE_LOG(LogBGWeaponFire, Error, TEXT("%s: TryStartReload failed because World was null."), *GetNameSafe(this));
	return false;
}

void UBG_WeaponFireComponent::CompleteReload()
{
	FGameplayTag WeaponItemTag;
	const FBG_WeaponItemDataRow* WeaponRow = nullptr;
	EBG_EquipmentSlot WeaponSlot = EBG_EquipmentSlot::None;
	if (!GetActiveWeaponContext(WeaponItemTag, WeaponRow, WeaponSlot, TEXT("CompleteReload")) || !WeaponRow)
	{
		CancelReload();
		return;
	}

	UBG_EquipmentComponent* EquipmentComponent = GetEquipmentComponent(TEXT("CompleteReload"));
	UBG_InventoryComponent* InventoryComponent = GetInventoryComponent(TEXT("CompleteReload"));
	if (!EquipmentComponent || !InventoryComponent)
	{
		CancelReload();
		return;
	}

	ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("CompleteReload"));
	if (!EquippedWeapon)
	{
		CancelReload();
		return;
	}

	const int32 InventoryAmmoCount = WeaponRow->AmmoItemTag.IsValid()
		? FMath::Max(0, InventoryComponent->GetQuantity(EBG_ItemType::Ammo, WeaponRow->AmmoItemTag))
		: 0;
	const int32 AmmoToLoad = EquippedWeapon->ResolveReloadAmount(InventoryAmmoCount);
	if (AmmoToLoad <= 0)
	{
		EquippedWeapon->CancelWeaponReload();
		SyncAmmoFromEquipment();
		if (CachedCharacter)
		{
			CachedCharacter->FinishTimedCharacterState(EBGCharacterState::Reloading);
		}
		return;
	}

	// Reload still starts from controller input, but the server resolves the final ammo amount through the weapon actor.
	int32 ConsumedInventoryAmmo = 0;
	if (!EquippedWeapon->UsesInfiniteDebugAmmo() && WeaponRow->AmmoItemTag.IsValid())
	{
		// Comment out this block later when inventory ammo should no longer be consumed through the debug reload path.
		if (!InventoryComponent->Auth_RemoveItem(EBG_ItemType::Ammo, WeaponRow->AmmoItemTag, AmmoToLoad, ConsumedInventoryAmmo)
			|| ConsumedInventoryAmmo <= 0)
		{
			UE_LOG(LogBGWeaponFire, Error, TEXT("%s: CompleteReload failed because ammo removal from inventory did not succeed for %s."), *GetNameSafe(this), *WeaponRow->AmmoItemTag.ToString());
			EquippedWeapon->CancelWeaponReload();
			CancelReload();
			return;
		}
	}
	else
	{
		ConsumedInventoryAmmo = AmmoToLoad;
	}

	EquippedWeapon->FinishWeaponReload(ConsumedInventoryAmmo);
	EquippedWeapon->NotifyReloadFinished(true);
	EquipmentComponent->Auth_LoadAmmo(WeaponSlot, EquippedWeapon->GetLoadedAmmo());
	SyncAmmoFromEquipment();

	if (CachedCharacter)
	{
		CachedCharacter->FinishTimedCharacterState(EBGCharacterState::Reloading);
	}
}

void UBG_WeaponFireComponent::CancelReload()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	if (ABG_EquippedWeaponBase* EquippedWeapon = GetActiveEquippedWeapon(TEXT("CancelReload")))
	{
		EquippedWeapon->CancelWeaponReload();
		EquippedWeapon->NotifyReloadFinished(false);
	}

	if (CachedCharacter)
	{
		CachedCharacter->FinishTimedCharacterState(EBGCharacterState::Reloading);
	}
}

void UBG_WeaponFireComponent::PlayReloadAnimation(EBGWeaponPoseType WeaponPoseType)
{
	if (!CachedCharacter)
	{
		return;
	}

	USkeletalMeshComponent* BodyMesh = CachedCharacter->GetBodyAnimationMesh();
	if (!BodyMesh || !BodyMesh->GetAnimInstance())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: PlayReloadAnimation failed because body mesh or anim instance was null."), *GetNameSafe(this));
		return;
	}

	UAnimMontage* MontageToPlay = nullptr;
	switch (WeaponPoseType)
	{
	case EBGWeaponPoseType::Pistol:
		MontageToPlay = PistolReloadMontage;
		break;
	case EBGWeaponPoseType::Rifle:
		MontageToPlay = RifleReloadMontage;
		break;
	case EBGWeaponPoseType::Shotgun:
		MontageToPlay = ShotgunReloadMontage;
		break;
	case EBGWeaponPoseType::Sniper:
		MontageToPlay = SniperReloadMontage;
		break;
	default:
		break;
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogBGWeaponFire, Warning, TEXT("%s: PlayReloadAnimation could not play montage because reload montage is not assigned for pose type %s."), *GetNameSafe(this), *UEnum::GetValueAsString(WeaponPoseType));
		return;
	}

	BodyMesh->GetAnimInstance()->Montage_Play(MontageToPlay);
}

void UBG_WeaponFireComponent::Multicast_PlayReloadAnimation_Implementation(EBGWeaponPoseType WeaponPoseType)
{
	PlayReloadAnimation(WeaponPoseType);
}

const FBGWeaponFireSettings* UBG_WeaponFireComponent::ResolveFireSettings(EBGWeaponPoseType WeaponPoseType) const
{
	switch (WeaponPoseType)
	{
	case EBGWeaponPoseType::Pistol:
		return &PistolFireSettings;
	case EBGWeaponPoseType::Rifle:
		return &RifleFireSettings;
	case EBGWeaponPoseType::Shotgun:
		return &ShotgunFireSettings;
	case EBGWeaponPoseType::Sniper:
		return &SniperFireSettings;
	default:
		return nullptr;
	}
}

const FBGWeaponAmmoSettings* UBG_WeaponFireComponent::ResolveAmmoSettings(EBGWeaponPoseType WeaponPoseType) const
{
	switch (WeaponPoseType)
	{
	case EBGWeaponPoseType::Pistol:
		return &PistolAmmoSettings;
	case EBGWeaponPoseType::Rifle:
		return &RifleAmmoSettings;
	case EBGWeaponPoseType::Shotgun:
		return &ShotgunAmmoSettings;
	case EBGWeaponPoseType::Sniper:
		return &SniperAmmoSettings;
	default:
		return nullptr;
	}
}

EBGWeaponFireMode UBG_WeaponFireComponent::ResolveFireMode(EBGWeaponPoseType WeaponPoseType) const
{
	if (const FBG_WeaponFireSpecRow* FireSpecRow = GetActiveWeaponFireSpecRow(TEXT("ResolveFireMode")))
	{
		switch (FireSpecRow->FireMode)
		{
		case EBG_WeaponFireMode::FullAuto:
			return EBGWeaponFireMode::FullAuto;
		case EBG_WeaponFireMode::SemiAuto:
		case EBG_WeaponFireMode::BoltAction:
		case EBG_WeaponFireMode::Melee:
			return EBGWeaponFireMode::SemiAuto;
		case EBG_WeaponFireMode::None:
		default:
			UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ResolveFireMode received invalid fire mode %s for weapon %s. Falling back to pose-based mode."),
				*GetNameSafe(this),
				*UEnum::GetValueAsString(FireSpecRow->FireMode),
				*FireSpecRow->WeaponItemTag.ToString());
			break;
		}
	}

	switch (WeaponPoseType)
	{
	case EBGWeaponPoseType::Rifle:
		return EBGWeaponFireMode::FullAuto;
	case EBGWeaponPoseType::Pistol:
	case EBGWeaponPoseType::Shotgun:
	case EBGWeaponPoseType::Sniper:
	default:
		return EBGWeaponFireMode::SemiAuto;
	}
}

void UBG_WeaponFireComponent::Multicast_PlayWeaponFireDebug_Implementation(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, FVector_NetQuantize ImpactPoint, bool bDidHit, EBGWeaponPoseType WeaponPoseType)
{
	const FBGWeaponFireSettings* FireSettings = ResolveFireSettings(WeaponPoseType);
	const float DebugDuration = FireSettings ? FireSettings->DebugDrawDuration : 1.0f;
	const float DebugRadius = FireSettings ? FireSettings->DebugSphereRadius : 12.f;

	FBGWeaponFireSettings DebugSettings;
	DebugSettings.DebugSphereRadius = DebugRadius;
	DebugSettings.DebugDrawDuration = DebugDuration;

	DrawFireDebug(TraceStart, TraceEnd, bDidHit ? ImpactPoint : TraceEnd, bDidHit, DebugSettings, WeaponPoseType);
	BroadcastHitIndicator(bDidHit, bDidHit ? ImpactPoint : TraceEnd);
}

void UBG_WeaponFireComponent::OnRep_AmmoState()
{
	BroadcastAmmoState();
}

void UBG_WeaponFireComponent::OnRep_FireAnimationSequence()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: OnRep_FireAnimationSequence failed because World was null."), *GetNameSafe(this));
		return;
	}

	LastFireAnimationWorldTime = World->GetTimeSeconds();
}
