#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BG_PlayerController.generated.h"

class ABG_Character;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

USTRUCT(BlueprintType)
struct FBGPlayerInputConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Core")
	TObjectPtr<UInputAction> MoveAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Core")
	TObjectPtr<UInputAction> LookAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Core")
	TObjectPtr<UInputAction> JumpAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Core")
	TObjectPtr<UInputAction> AttackAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Stance")
	TObjectPtr<UInputAction> CrouchAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Stance")
	TObjectPtr<UInputAction> ProneAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Lean")
	TObjectPtr<UInputAction> LeanLeftAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Lean")
	TObjectPtr<UInputAction> LeanRightAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Aim")
	TObjectPtr<UInputAction> AimAction = nullptr;
};

UCLASS()
class PUBG_HOTMODE_API ABG_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	FBGPlayerInputConfig InputConfig;

	void BindPawnInput();
	void RefreshMappingContext();

	void OnMoveInputTriggered(const FInputActionValue& Value);
	void OnLookInputTriggered(const FInputActionValue& Value);
	void OnJumpInputStarted();
	void OnJumpInputCompleted();
	void OnAttackInputStarted();

	// Future extension points. Bind an action asset and implement behavior when ready.
	void OnCrouchInputStarted();
	void OnProneInputStarted();
	void OnLeanLeftInputStarted();
	void OnLeanLeftInputCompleted();
	void OnLeanRightInputStarted();
	void OnLeanRightInputCompleted();
	void OnAimInputStarted();
	void OnAimInputCompleted();

	ABG_Character* GetBGCharacter() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> LastBoundInputComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABG_Character> LastBoundCharacter = nullptr;
};
