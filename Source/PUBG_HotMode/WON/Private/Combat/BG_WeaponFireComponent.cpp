#include "Combat/BG_WeaponFireComponent.h"

#include "Combat/BG_DamageSystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Player/BG_Character.h"
#include "Components/SkeletalMeshComponent.h"

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

	PistolAmmoSettings.MaxMagazineAmmo = 12;
	PistolAmmoSettings.MaxReserveAmmo = 48;

	RifleAmmoSettings.MaxMagazineAmmo = 30;
	RifleAmmoSettings.MaxReserveAmmo = 90;

	ShotgunAmmoSettings.MaxMagazineAmmo = 8;
	ShotgunAmmoSettings.MaxReserveAmmo = 24;
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
}

void UBG_WeaponFireComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentMagazineAmmo);
	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentReserveAmmo);
	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentMaxMagazineAmmo);
	DOREPLIFETIME(UBG_WeaponFireComponent, CurrentWeaponPoseType);
}

void UBG_WeaponFireComponent::RequestFire()
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: RequestFire failed because CachedCharacter was null."), *GetNameSafe(this));
		return;
	}

	if (!CanFireWeapon())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: RequestFire failed because firing was blocked. Character=%s, WeaponEquipped=%d, CanFire=%d, Pose=%s, Ammo=%d"),
			*GetNameSafe(this),
			*GetNameSafe(CachedCharacter),
			CachedCharacter->IsWeaponEquipped() ? 1 : 0,
			CachedCharacter->CanFire() ? 1 : 0,
			*UEnum::GetValueAsString(CachedCharacter->GetEquippedWeaponPoseType()),
			CurrentMagazineAmmo);
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ExecuteFire();
		return;
	}

	Server_RequestFire();
}

bool UBG_WeaponFireComponent::CanFireWeapon() const
{
	if (!CachedCharacter)
	{
		return false;
	}

	const EBGWeaponPoseType WeaponPoseType = CachedCharacter->GetEquippedWeaponPoseType();
	return CachedCharacter->IsWeaponEquipped()
		&& CachedCharacter->CanFire()
		&& WeaponPoseType != EBGWeaponPoseType::None
		&& ResolveFireSettings(WeaponPoseType) != nullptr
		&& CurrentMagazineAmmo > 0;
}

void UBG_WeaponFireComponent::ApplyTemporaryWeaponProfile(EBGWeaponPoseType WeaponPoseType)
{
	const FBGWeaponAmmoSettings* AmmoSettings = ResolveAmmoSettings(WeaponPoseType);
	CurrentWeaponPoseType = WeaponPoseType;

	if (!AmmoSettings || WeaponPoseType == EBGWeaponPoseType::None)
	{
		CurrentMagazineAmmo = 0;
		CurrentReserveAmmo = 0;
		CurrentMaxMagazineAmmo = 0;
		BroadcastAmmoState();
		return;
	}

	CurrentMaxMagazineAmmo = AmmoSettings->MaxMagazineAmmo;
	CurrentMagazineAmmo = AmmoSettings->MaxMagazineAmmo;
	CurrentReserveAmmo = AmmoSettings->MaxReserveAmmo;
	BroadcastAmmoState();
}

void UBG_WeaponFireComponent::Server_RequestFire_Implementation()
{
	ExecuteFire();
}

bool UBG_WeaponFireComponent::Server_RequestFire_Validate()
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
	const FBGWeaponFireSettings* FireSettings = ResolveFireSettings(WeaponPoseType);
	if (!FireSettings)
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because no fire settings existed for pose %s."),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(WeaponPoseType));
		return false;
	}

	if (!CachedCharacter->CanFire())
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because character cannot fire right now. Character=%s, State=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CachedCharacter),
			*UEnum::GetValueAsString(CachedCharacter->GetCharacterState()));
		return false;
	}

	if (!ConsumeAmmo(1))
	{
		UE_LOG(LogBGWeaponFire, Error, TEXT("%s: ExecuteFire failed because ammo was empty. Pose=%s"), *GetNameSafe(this), *UEnum::GetValueAsString(WeaponPoseType));
		return false;
	}

	const float CurrentTime = World->TimeSeconds;
	if (CurrentTime - LastFireTime < FireSettings->FireCooldown)
	{
		return false;
	}

	LastFireTime = CurrentTime;

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	CachedCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	const FVector ShotStart = ViewLocation;
	const FVector BaseShotDirection = ViewRotation.Vector();

	const int32 PelletCount = FMath::Max(1, FireSettings->PelletCount);
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const float SpreadRadians = FMath::DegreesToRadians(FireSettings->SpreadAngleDegrees);
		const FVector ShotDirection = SpreadRadians > 0.f
			? FMath::VRandCone(BaseShotDirection, SpreadRadians)
			: BaseShotDirection;
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

	USkeletalMeshComponent* BodyMesh = CachedCharacter->GetMesh();
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
	default:
		break;
	}

	if (MontageToPlay)
	{
		BodyMesh->GetAnimInstance()->Montage_Play(MontageToPlay);
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
	default:
		return nullptr;
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

