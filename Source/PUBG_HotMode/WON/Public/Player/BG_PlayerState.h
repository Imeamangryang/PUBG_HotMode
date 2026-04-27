// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BG_PlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnBGHealthChanged, float, NewHealth, float, MaxHealth, bool, bNewIsDead);

UCLASS()
class PUBG_HOTMODE_API ABG_PlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABG_PlayerState();

	// 현재 HP는 서버 판정 결과를 모든 클라이언트가 동일하게 보도록 복제한다.
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHP, BlueprintReadOnly, Category = "Combat")
	float CurrentHP;

	// 사망 여부를 HP와 분리해 두면 UI, 매치 종료 판정, 입력 차단에서 바로 참조하기 쉽다.
	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Combat")
	bool bIsDead;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float MaxHP;

	// HP 감소는 서버만 처리해야 하므로 데미지 적용 경로를 이 함수로 모아 둔다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool ApplyDamage(float DamageAmount);

	// 캐릭터 공통 HP 컴포넌트가 서버에서 계산한 값을 PlayerState로 복사할 때 사용한다.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetHealthSnapshot(float NewCurrentHP, bool bNewIsDead, float NewMaxHP);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetCurrentHP() const { return CurrentHP; }

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnBGHealthChanged OnHealthChanged;

protected:
	// HP가 바뀌었을 때 클라이언트 전용 반응(UI, 피격 표시 등)을 붙일 지점이다.
	UFUNCTION()
	void OnRep_CurrentHP();

	// 사망 상태 변경 시 사망 UI, 관전 전환 같은 후속 처리를 연결하기 좋다.
	UFUNCTION()
	void OnRep_IsDead();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
  
