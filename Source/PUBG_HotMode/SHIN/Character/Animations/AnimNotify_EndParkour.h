#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_EndParkour.generated.h"

UCLASS()
class PUBG_HOTMODE_API UAnimNotify_EndParkour : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Notify가 실행될 때 호출되는 함수입니다.
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};