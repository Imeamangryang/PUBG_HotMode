#include "AnimNotify_EndParkour.h"
#include "Character/Components/ParkourComponent.h"

void UAnimNotify_EndParkour::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	// 1. Owner 캐릭터 가져오기
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// 2. 파쿠르 컴포넌트 찾아오기
	UParkourComponent* ParkourComp = Owner->FindComponentByClass<UParkourComponent>();

	// 3. EndParkour 실행
	if (ParkourComp)
	{
		ParkourComp->EndParkour();
	}
}
