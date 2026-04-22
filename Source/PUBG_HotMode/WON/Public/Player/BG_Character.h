#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "BG_Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UBG_DamageSystem;
class UAnimMontage;

DECLARE_LOG_CATEGORY_EXTERN(LogBGCharacter, Log, All);

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

public:
    /** 컨트롤러(BG_PlayerController)에서 호출하는 실행 함수들 */
    void MoveFromInput(const FInputActionValue& Value);
    void LookFromInput(const FInputActionValue& Value);
    void StartJumpFromInput();
    void StopJumpFromInput();
    void Req_PrimaryAction();

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBG_DamageSystem> DamageSystem;

public:
    /** 공격 설정 */
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    TObjectPtr<UAnimMontage> MeleePunchMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    float MeleeDamage = 20.f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    float MeleeRange = 150.f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    float MeleeRadius = 60.f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
    float MeleeAttackCooldown = 1.5f;

    float LastMeleeAttackTime = -2.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
    bool bIsWeaponEquipped = false;
    
    // 총을 자식으로 붙일 컴포넌트
    UPROPERTY(VisibleAnywhere)
    class USceneComponent* GunComp;
    
    // 필요 속성 : 총 소유 여부, 소유중인 총, 총 검색 범위
    bool bHasPistol = false;
    

    
    
    
};
