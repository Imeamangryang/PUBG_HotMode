#include "Player/BG_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Player/BG_Character.h"

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

void ABG_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	RefreshMappingContext();
}

void ABG_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshMappingContext();
	BindPawnInput();
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

	BindStartedIfValid(EnhancedInputComponent, InputConfig.CrouchAction.Get(), this, &ABG_PlayerController::OnCrouchInputStarted);
	BindStartedIfValid(EnhancedInputComponent, InputConfig.ProneAction.Get(), this, &ABG_PlayerController::OnProneInputStarted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.LeanLeftAction.Get(), this, &ABG_PlayerController::OnLeanLeftInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.LeanLeftAction.Get(), this, &ABG_PlayerController::OnLeanLeftInputCompleted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.LeanRightAction.Get(), this, &ABG_PlayerController::OnLeanRightInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.LeanRightAction.Get(), this, &ABG_PlayerController::OnLeanRightInputCompleted);

	BindStartedIfValid(EnhancedInputComponent, InputConfig.AimAction.Get(), this, &ABG_PlayerController::OnAimInputStarted);
	BindCompletedIfValid(EnhancedInputComponent, InputConfig.AimAction.Get(), this, &ABG_PlayerController::OnAimInputCompleted);

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
		BGCharacter->Req_PrimaryAction();
	}
}

void ABG_PlayerController::OnCrouchInputStarted()
{
}

void ABG_PlayerController::OnProneInputStarted()
{
}

void ABG_PlayerController::OnLeanLeftInputStarted()
{
}

void ABG_PlayerController::OnLeanLeftInputCompleted()
{
}

void ABG_PlayerController::OnLeanRightInputStarted()
{
}

void ABG_PlayerController::OnLeanRightInputCompleted()
{
}

void ABG_PlayerController::OnAimInputStarted()
{
}

void ABG_PlayerController::OnAimInputCompleted()
{
}

ABG_Character* ABG_PlayerController::GetBGCharacter() const
{
	return Cast<ABG_Character>(GetPawn());
}