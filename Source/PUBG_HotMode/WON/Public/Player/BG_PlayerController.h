#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BG_PlayerController.generated.h"

class ABG_Character;
class UBG_HealthViewModel;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
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

	// 상호작용 키
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Core")
	TObjectPtr<UInputAction> InteractAction;
};

UCLASS()
class PUBG_HOTMODE_API ABG_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABG_PlayerController();
	virtual void SetupInputComponent() override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	UBG_HealthViewModel* GetHUDViewModel() const { return HUDViewModel; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	FBGPlayerInputConfig InputConfig;

	void BindPawnInput();
	void RefreshMappingContext();
	void RefreshViewModelBinding();
	void EnsureGameHUDWidget();

	void OnMoveInputTriggered(const FInputActionValue& Value);
	void OnLookInputTriggered(const FInputActionValue& Value);
	void OnJumpInputStarted();
	void OnJumpInputCompleted();
	void OnAttackInputStarted();
	void OnAttackInputCompleted();
	void OnEquipPistolInputStarted();
	void OnEquipRifleInputStarted();
	void OnUnequipWeaponInputStarted();

	// Future extension points. Bind an action asset and implement behavior when ready.
	void OnCrouchInputStarted();
	void OnProneInputStarted();
	void OnLeanLeftInputStarted();
	void OnLeanLeftInputCompleted();
	void OnLeanRightInputStarted();
	void OnLeanRightInputCompleted();
	void OnAimInputStarted();
	void OnAimInputCompleted();
	void OnInteractInputStarted(); //상호작용 키

	ABG_Character* GetBGCharacter() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> LastBoundInputComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABG_Character> LastBoundCharacter = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_HealthViewModel> HUDViewModel = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> GameHUDWidgetClass = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GameHUDWidget = nullptr;
};
