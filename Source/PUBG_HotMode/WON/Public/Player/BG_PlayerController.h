#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BG_PlayerController.generated.h"

class ABG_Character;
class UBG_HealthViewModel;
class UBG_InventoryViewModel;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
struct FInputActionValue;
enum class EBG_ItemType : uint8;
enum class EBGInventoryFailReason : uint8;
struct FGameplayTag;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Combat")
	TObjectPtr<UInputAction> ReloadAction = nullptr;

	// 상호작용 키
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Core")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|UI")
	TObjectPtr<UInputAction> InventoryAction = nullptr;
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

	UFUNCTION(BlueprintPure, Category = "HUD")
	UBG_HealthViewModel* GetHUDViewModel() const { return HealthViewModel; }

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	UBG_InventoryViewModel* GetInventoryViewModel() const { return InventoryViewModel; }

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void OpenInventoryUI();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void CloseInventoryUI();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ToggleInventoryUI();

	UFUNCTION(BlueprintPure, Category = "Inventory UI")
	bool IsInventoryUIOpen() const;

	void NotifyInventoryFailure(EBGInventoryFailReason FailReason, EBG_ItemType ItemType, FGameplayTag ItemTag);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	FBGPlayerInputConfig InputConfig;

	void BindPawnInput();
	void RefreshMappingContext();
	void RefreshViewModelBinding();
	void EnsureGameHUDWidget();
	bool EnsureInventoryWidget();

	void OnMoveInputTriggered(const FInputActionValue& Value);
	void OnLookInputTriggered(const FInputActionValue& Value);
	void OnJumpInputStarted();
	void OnJumpInputCompleted();
	void OnAttackInputStarted();
	void OnAttackInputCompleted();
	void OnEquipPistolInputStarted();
	void OnEquipRifleInputStarted();
	void OnUnequipWeaponInputStarted();
	void OnUseBandageInputStarted();

	// Future extension points. Bind an action asset and implement behavior when ready.
	void OnCrouchInputStarted();
	void OnProneInputStarted();
	void OnLeanLeftInputStarted();
	void OnLeanLeftInputCompleted();
	void OnLeanRightInputStarted();
	void OnLeanRightInputCompleted();
	void OnAimInputStarted();
	void OnAimInputCompleted();
	void OnReloadInputStarted();
	void OnInteractInputStarted(); //상호작용 키
	void OnInventoryInputStarted();

	ABG_Character* GetBGCharacter() const;

public:
	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void HandleControlledCharacterDeath(ABG_Character* DeadCharacter);

	UFUNCTION(BlueprintCallable, Category = "Spectator")
	void HandleControlledCharacterRevived(ABG_Character* RevivedCharacter);

	UFUNCTION(BlueprintNativeEvent, Category = "Spectator")
	void PrepareSpectatorMode(ABG_Character* DeadCharacter);

private:
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> LastBoundInputComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABG_Character> LastBoundCharacter = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_HealthViewModel> HealthViewModel = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBG_InventoryViewModel> InventoryViewModel = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> GameHUDWidgetClass = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GameHUDWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> InventoryWidgetClass = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> InventoryWidget = nullptr;
};
