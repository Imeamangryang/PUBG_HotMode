#include "Player/BG_InteractionAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/BG_Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGInteractionAnimation, Log, All);

UBG_InteractionAnimationComponent::UBG_InteractionAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBG_InteractionAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ABG_Character>(GetOwner());
	if (!CachedCharacter)
	{
		UE_LOG(LogBGInteractionAnimation, Error, TEXT("%s: BeginPlay failed because owner was not ABG_Character."), *GetNameSafe(this));
	}
}

void UBG_InteractionAnimationComponent::PlayPickupMontage(EBG_ItemType ItemType)
{
	if (!CachedCharacter)
	{
		UE_LOG(LogBGInteractionAnimation, Error, TEXT("%s: PlayPickupMontage failed because CachedCharacter was null."), *GetNameSafe(this));
		return;
	}

	USkeletalMeshComponent* BodyMesh = CachedCharacter->GetBodyAnimationMesh();
	if (!BodyMesh)
	{
		UE_LOG(LogBGInteractionAnimation, Error, TEXT("%s: PlayPickupMontage failed because BodyMesh was null."), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInstance = BodyMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogBGInteractionAnimation, Error, TEXT("%s: PlayPickupMontage failed because AnimInstance was null."), *GetNameSafe(this));
		return;
	}

	UAnimMontage* MontageToPlay = ResolvePickupMontage(ItemType);
	if (!MontageToPlay)
	{
		UE_LOG(LogBGInteractionAnimation, Warning, TEXT("%s: PlayPickupMontage skipped because no montage was configured for ItemType=%s."),
			*GetNameSafe(this),
			*StaticEnum<EBG_ItemType>()->GetNameStringByValue(static_cast<int64>(ItemType)));
		return;
	}

	const float PlayRate = ResolvePickupMontagePlayRate(ItemType);
	const float StartTime = PlayRate < 0.f ? MontageToPlay->GetPlayLength() : 0.f;
	AnimInstance->Montage_Play(MontageToPlay, PlayRate, EMontagePlayReturnType::MontageLength, StartTime);
}

float UBG_InteractionAnimationComponent::GetPickupMontageDuration(EBG_ItemType ItemType) const
{
	if (UAnimMontage* MontageToPlay = ResolvePickupMontage(ItemType))
	{
		const float PlayRate = FMath::Abs(ResolvePickupMontagePlayRate(ItemType));
		if (PlayRate > KINDA_SMALL_NUMBER)
		{
			return MontageToPlay->GetPlayLength() / PlayRate;
		}
	}

	return 0.f;
}

UAnimMontage* UBG_InteractionAnimationComponent::ResolvePickupMontage(EBG_ItemType ItemType) const
{
	if (ItemType == EBG_ItemType::Weapon && WeaponPickupMontage)
	{
		return WeaponPickupMontage;
	}

	return DefaultPickupMontage;
}

float UBG_InteractionAnimationComponent::ResolvePickupMontagePlayRate(EBG_ItemType ItemType) const
{
	return ItemType == EBG_ItemType::Weapon ? -1.f : 1.f;
}
