#include "Player/BG_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "BG_HealthViewModel.h"
#include "Player/BG_Character.h"
#include "Blueprint/UserWidget.h"
#include "PUBG_HotMode/ConstructionHelperExtension.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	template <typename UserClass, typename FuncType>
	void BindStartedIfValid(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* InputAction, UserClass* Owner, FuncType Func)
	{
		if (EnhancedInputComponent && InputAction && Owner)
		{
			EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Started, Owner, Func);
		}
	}

	template <typename UserClass, typename FuncType>
	void BindCompletedIfValid(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* InputAction, UserClass* Owner, FuncType Func)
	{
		if (EnhancedInputComponent && InputAction && Owner)
		{
			EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Completed, Owner, Func);
		}
	}

	template <typename UserClass, typename FuncType>
	void BindTriggeredIfValid(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* InputAction, UserClass* Owner, FuncType Func)
	{
		if (EnhancedInputComponent && InputAction && Owner)
		{
			EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Triggered, Owner, Func);
		}
	}
}

ABG_PlayerController::ABG_PlayerController()
{
	EXT_CREATE_DEFAULT_SUBOBJECT(HUDViewModel);
	Ext::SetClass(GameHUDWidgetClass, TEXT("/Script/UMGEditor.WidgetBlueprint'/Game/GEONU/Blueprints/Widgets/WBP_GameHUD.WBP_GameHUD_C'"));
}

void ABG_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	RefreshMappingContext();
	EnsureGameHUDWidget();
	RefreshViewModelBinding();
}

void ABG_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshMappingContext();
	BindPawnInput();
	RefreshViewModelBinding();
}

void ABG_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindPawnInput();
}

void ABG_PlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	RefreshMappingContext();
	BindPawnInput();
	RefreshViewModelBinding();
}

void ABG_PlayerController::RefreshViewModelBinding()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!HUDViewModel)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: RefreshHUDViewModelBinding failed because HUDViewModel was null."), *GetNameSafe(this));
		return;
	}

	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		HUDViewModel->NotifyPossessedCharacterReady(BGCharacter);
		return;
	}

	HUDViewModel->NotifyPossessedCharacterCleared();
}

void ABG_PlayerController::EnsureGameHUDWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GameHUDWidget)
	{
		if (!GameHUDWidget->IsInViewport() && !GameHUDWidget->AddToPlayerScreen())
		{
			UE_LOG(LogTemp, Error, TEXT("%s: EnsureGameHUDWidget failed to add existing GameHUDWidget to player screen."), *GetNameSafe(this));
		}
		return;
	}

	if (!GameHUDWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: EnsureGameHUDWidget failed because GameHUDWidgetClass was null."), *GetNameSafe(this));
		return;
	}

	GameHUDWidget = CreateWidget<UUserWidget>(this, GameHUDWidgetClass);
	if (!GameHUDWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: EnsureGameHUDWidget failed because CreateWidget returned null."), *GetNameSafe(this));
		return;
	}

	if (!GameHUDWidget->AddToPlayerScreen())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: EnsureGameHUDWidget failed to add GameHUDWidget to player screen."), *GetNameSafe(this));
	}
}

void ABG_PlayerController::RefreshMappingContext()
{
	if (!IsLocalController())
	{
		return;
	}

	UInputMappingContext* DefaultMappingContext = InputConfig.DefaultMappingContext.Get();
	if (!DefaultMappingContext)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ABG_PlayerController::BindPawnInput()
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	ABG_Character* BGCharacter = GetBGCharacter();
	

	if (!EnhancedInputComponent)
	{
		return;
	}

	if (!BGCharacter)
	{
		LastBoundCharacter = nullptr;
		LastBoundInputComponent = InputComponent;
		return;
	}

	if (LastBoundInputComponent == InputComponent && LastBoundCharacter == BGCharacter)
	{
		return;
	}

	EnhancedInputComponent->ClearActionBindings();

	BindTriggeredIfValid(EnhancedInputComponent, InputConfig.MoveAction.Get(), this, &ABG_PlayerController::OnMoveInputTriggered);
	BindTriggeredIfValid(EnhancedInputComponent, InputConfig.LookAction.Get(), this, &ABG_PlayerController::OnLookInputTriggered);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.JumpAction.Get(), this, &ABG_PlayerController::OnJumpInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.JumpAction.Get(), this, &ABG_PlayerController::OnJumpInputCompleted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.AttackAction.Get(), this, &ABG_PlayerController::OnAttackInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.AttackAction.Get(), this, &ABG_PlayerController::OnAttackInputCompleted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.CrouchAction.Get(), this, &ABG_PlayerController::OnCrouchInputStarted);
	BindStartedIfValid(EnhancedInputComponent, InputConfig.ProneAction.Get(), this, &ABG_PlayerController::OnProneInputStarted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.LeanLeftAction.Get(), this, &ABG_PlayerController::OnLeanLeftInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.LeanLeftAction.Get(), this, &ABG_PlayerController::OnLeanLeftInputCompleted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.LeanRightAction.Get(), this, &ABG_PlayerController::OnLeanRightInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.LeanRightAction.Get(), this, &ABG_PlayerController::OnLeanRightInputCompleted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.AimAction.Get(), this, &ABG_PlayerController::OnAimInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.AimAction.Get(), this, &ABG_PlayerController::OnAimInputCompleted);

	// 상호작용 키
	BindStartedIfValid(EnhancedInputComponent, InputConfig.InteractAction.Get(), this, &ABG_PlayerController::OnInteractInputStarted);
	
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABG_PlayerController::OnEquipPistolInputStarted);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABG_PlayerController::OnEquipRifleInputStarted);
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ABG_PlayerController::OnUnequipWeaponInputStarted);

	LastBoundCharacter = BGCharacter;
	LastBoundInputComponent = InputComponent;
}

void ABG_PlayerController::OnMoveInputTriggered(const FInputActionValue& Value)
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->MoveFromInput(Value);
	}
}

void ABG_PlayerController::OnLookInputTriggered(const FInputActionValue& Value)
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->LookFromInput(Value);
	}
}

void ABG_PlayerController::OnJumpInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StartJumpFromInput();
	}
}

void ABG_PlayerController::OnJumpInputCompleted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StopJumpFromInput();
	}
}

void ABG_PlayerController::OnAttackInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StartPrimaryActionFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnAttackInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnAttackInputCompleted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StopPrimaryActionFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnAttackInputCompleted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnEquipPistolInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		// 임시 장착 입력이다. 나중에 인벤토리/아이템 연동이 들어오면 여기만 교체하면 된다.
		BGCharacter->SetWeaponState(EBGWeaponPoseType::Pistol, true);
		UE_LOG(LogTemp, Warning, TEXT("%s: Temporary weapon equip switched to Pistol."), *GetNameSafe(this));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnEquipPistolInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnEquipRifleInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		// 임시 장착 입력이다. 나중에 인벤토리/아이템 연동이 들어오면 여기만 교체하면 된다.
		BGCharacter->SetWeaponState(EBGWeaponPoseType::Rifle, true);
		UE_LOG(LogTemp, Warning, TEXT("%s: Temporary weapon equip switched to Rifle."), *GetNameSafe(this));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnEquipRifleInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnUnequipWeaponInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		// 임시 해제 입력이다. 최종 인벤토리 시스템이 오면 장착 상태는 그쪽이 관리한다.
		BGCharacter->SetWeaponState(EBGWeaponPoseType::None, false);
		UE_LOG(LogTemp, Warning, TEXT("%s: Temporary weapon equip cleared."), *GetNameSafe(this));
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnUnequipWeaponInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnCrouchInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->ToggleCrouchFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnCrouchInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnProneInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->ToggleProneFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnProneInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnLeanLeftInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StartLeanLeftFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnLeanLeftInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnLeanLeftInputCompleted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StopLeanLeftFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnLeanLeftInputCompleted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnLeanRightInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StartLeanRightFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnLeanRightInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnLeanRightInputCompleted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StopLeanRightFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnLeanRightInputCompleted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnAimInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter()) 
	{
		BGCharacter->StartAimFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnAimInputStarted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnAimInputCompleted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->StopAimFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnAimInputCompleted failed because controlled character was null."), *GetNameSafe(this));
}

void ABG_PlayerController::OnInteractInputStarted()
{
	if (ABG_Character* BGChar = GetBGCharacter())
	{
		BGChar->InteractFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnInteractInputStarted failed because controlled character was null."), *GetNameSafe(this));
}


ABG_Character* ABG_PlayerController::GetBGCharacter() const
{
	return Cast<ABG_Character>(GetPawn());
}
