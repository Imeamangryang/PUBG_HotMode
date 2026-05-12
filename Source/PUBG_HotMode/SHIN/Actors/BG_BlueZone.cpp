#include "BG_BlueZone.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/BG_Character.h"
#include "Combat/BG_HealthComponent.h"
#include "Utils/BG_LogHelper.h"

ABG_BlueZone::ABG_BlueZone()
{
	PrimaryActorTick.bCanEverTick = true;

	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	SetRootComponent(ZoneMesh);

	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ZoneMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void ABG_BlueZone::BeginPlay()
{
	Super::BeginPlay();

	ZoneCenter = GetActorLocation();
	CurrentRadius = InitialRadius;

	if (ZoneMesh)
	{
		ZoneMID = ZoneMesh->CreateDynamicMaterialInstance(0);
	}
}

#if WITH_EDITOR
void ABG_BlueZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 1. 어떤 변수가 바뀌었는지 이름을 가져옵니다.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) 
		? PropertyChangedEvent.Property->GetFName() 
		: NAME_None;

	// 2. 특정 변수가 수정되었을 때만 스케일 업데이트 로직 실행
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABG_BlueZone, InitialRadius) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ABG_BlueZone, CylinderHalfHeight))
	{
		if (ZoneMesh)
		{
			CurrentRadius = InitialRadius;
			ZoneCenter = GetActorLocation();

			// 2. 비주얼 업데이트 호출
			UpdateVisuals();
		}
	}
}
#endif

void ABG_BlueZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateVisuals();

	if (!bIsActive)
	{
		return;
	}

	UpdateZone(DeltaTime);
}

// Zone Update
void ABG_BlueZone::UpdateZone(float DeltaTime)
{
	ElapsedTime += DeltaTime;

	if (ZoneState == EZoneState::Waiting && ElapsedTime >= WaitTime)
	{
		StartShrink();
	}

	if (ZoneState == EZoneState::Shrinking)
	{
		float Alpha = GetShrinkAlpha();
		CurrentRadius = FMath::Lerp(InitialRadius, MinRadius, Alpha);
	}
}

void ABG_BlueZone::StartShrink()
{
	ZoneState = EZoneState::Shrinking;
	ElapsedTime = 0.f;
}

float ABG_BlueZone::GetShrinkAlpha() const
{
	return FMath::Clamp(ElapsedTime / ShrinkTime, 0.f, 1.f);
}

// Visual Update (핵심)
void ABG_BlueZone::UpdateVisuals()
{
	if (!ZoneMesh)
		return;

	// 1. 물리적 크기 조절
	float XYScale = CurrentRadius / 50.f;
	float ZScale = (CylinderHalfHeight * 2.f) / 100.f;
	ZoneMesh->SetWorldScale3D(FVector(XYScale, XYScale, ZScale));

	// 2. 머티리얼 데이터 전달 (에디터/런타임 공용)
	// 게임 시작 전(MID 없음)에는 기본 머티리얼의 Parameter를 수정하도록 시도
	UMaterialInstanceDynamic* TargetMID = ZoneMID;
    
	// 에디터 모드에서 MID가 없다면 임시로 가져오기
	if (!TargetMID)
	{
		TargetMID = Cast<UMaterialInstanceDynamic>(ZoneMesh->GetMaterial(0));
	}

	if (TargetMID)
	{
		TargetMID->SetScalarParameterValue(TEXT("Radius"), CurrentRadius);
		TargetMID->SetVectorParameterValue(TEXT("Center"), ZoneCenter);
	}
}

// Player Check
void ABG_BlueZone::CheckPlayersOutside()
{
	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Players);

	for (AActor* Actor : Players)
	{
		ACharacter* Character = Cast<ACharacter>(Actor);
		if (!Character)
			continue;

		float Dist = FVector::Dist2D(Character->GetActorLocation(), ZoneCenter);

		if (Dist > CurrentRadius)
		{
			// TODO: 블루존 데미지 처리
		}
	}
}

void ABG_BlueZone::SetZoneActive(bool bNewActive)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsActive == bNewActive)
	{
		return;
	}

	bIsActive = bNewActive;

	if (bIsActive)
	{
		ElapsedTime = 0.f;
		ZoneState = EZoneState::Waiting;

		GetWorldTimerManager().SetTimer(
			DamageTimerHandle,
			this,
			&ABG_BlueZone::ApplyBlueZoneDamageTick,
			DamageTickInterval,
			true
		);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	}
}

void ABG_BlueZone::ApplyBlueZoneDamageTick()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> Players;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABG_Character::StaticClass(), Players);

	for (AActor* Actor : Players)
	{
		ABG_Character* Character = Cast<ABG_Character>(Actor);
		if (!Character)
		{
			continue;
		}

		if (Character->GetCharacterState() == EBGCharacterState::Dead)
		{
			Character->SetIsOutsideBlueZone(false);
			continue;
		}

		const float Dist = FVector::Dist2D(Character->GetActorLocation(), ZoneCenter);
		const bool bIsOutsideBlueZone = Dist > CurrentRadius;

		Character->SetIsOutsideBlueZone(bIsOutsideBlueZone);

		if (!Character->CanReceiveBlueZoneDamage())
		{
			continue;
		}

		if (!bIsOutsideBlueZone)
		{
			continue;
		}

		UBG_HealthComponent* HealthComponent = Character->GetHealthComponent();

		HealthComponent->ApplyDamage(DamagePerTick);
	}
}