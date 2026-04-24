#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "ParkourComponent.generated.h"

UENUM()
enum class EParkourState : uint8
{
	None,
	Hurdle,
	Climb
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PUBG_HOTMODE_API UParkourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UParkourComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category="Parkour|Animation")
	void TryParkour();
	
	UFUNCTION(BlueprintCallable, Category="Parkour|Animation")
	void EndParkour();
	
	// 파쿠르 시작 요청 (클라이언트 -> 서버)
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StartParkour(EParkourState NewState, FVector NetTargetLocation);

	// 파쿠르 종료 요청 (클라이언트 -> 서버)
	UFUNCTION(Server, Reliable)
	void Server_EndParkour();

	// 애니메이션 실행 (서버 -> 모든 클라이언트)
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayParkourMontage(EParkourState State);
	
	// 유틸리티: "Body" 메쉬를 안전하게 찾아주는 함수
	USkeletalMeshComponent* GetBodyMesh() const;

private:
	bool DetectObstacle(FHitResult& OutHit, float& OutHeight, bool& bOutThinObstacle, FVector& OutTopLocation) const;

private:
	UPROPERTY()
	class ACharacter* OwnerCharacter;

	UPROPERTY()
	class UCharacterMovementComponent* Movement;

	UPROPERTY(EditAnywhere, Category="Parkour")
	float TraceDistance = 100.f;

	UPROPERTY(EditAnywhere, Category="Parkour")
	float HurdleThreshold = 80.f;

	UPROPERTY(EditAnywhere, Category="Parkour")
	float ClimbThreshold = 200.f;
	
	UPROPERTY(Replicated)
	FVector TargetLocation;
	UPROPERTY(Replicated)
	EParkourState ParkourState = EParkourState::None;

	UPROPERTY(EditAnywhere, Category="Parkour|Animation")
	class UAnimMontage* HurdleMontage;

	UPROPERTY(EditAnywhere, Category="Parkour|Animation")
	class UAnimMontage* ClimbMontage;


};