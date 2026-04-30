#include "Player/BG_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GameplayTagContainer.h"
#include "BG_HealthViewModel.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "UI/BG_InventoryViewModel.h"
#include "Inventory/BG_ItemTypes.h"
#include "Player/BG_Character.h"
#include "Blueprint/UserWidget.h"
#include "PUBG_HotMode/ConstructionHelperExtension.h"
#include "UObject/ConstructorHelpers.h"
#include "Utils/BG_LogHelper.h"
#include "InputMappingContext.h"

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

	const FName BandageItemTagName(TEXT("Item.Heal.Bandage"));
}


ABG_PlayerController::ABG_PlayerController()
{
	EXT_CREATE_DEFAULT_SUBOBJECT(HealthViewModel);
	EXT_CREATE_DEFAULT_SUBOBJECT(InventoryViewModel);
	Ext::SetClass(GameHUDWidgetClass, TEXT("/Script/UMGEditor.WidgetBlueprint'/Game/GEONU/Blueprints/Widgets/WBP_GameHUD.WBP_GameHUD_C'"));
	Ext::SetClass(InventoryWidgetClass, TEXT("/Script/UMGEditor.WidgetBlueprint'/Game/GEONU/Blueprints/Widgets/WBP_InventoryHUD.WBP_InventoryHUD_C'"));
}

void ABG_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	BG_SHIN_LOG_EVENT_BLOCK(this, "BG_PlayerController BeginPlay",
		TEXT("IsLocalController=%s"),
		IsLocalController() ? TEXT("true") : TEXT("false"));

	BG_SHIN_LOG_CONTROLLER(this, this, "PlayerController BeginPlay");
	
	RefreshMappingContext();
	EnsureGameHUDWidget();
	RefreshViewModelBinding();
	
	if (IsLocalController())
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;

		BG_SHIN_LOG_INFO(TEXT("Applied GameOnly input mode on local controller"));
	}
}

void ABG_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	BG_SHIN_LOG_EVENT_BLOCK(this, "BG_PlayerController OnPossess",
		TEXT("PossessedPawn=%s PawnClass=%s"),
		*BGLogHelper::SafeName(InPawn),
		*BGLogHelper::SafeClass(InPawn));

	BG_SHIN_LOG_CONTROLLER(this, this, "After OnPossess");
	
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
	
	BG_SHIN_LOG_EVENT_BLOCK(this, "BG_PlayerController SetPawn",
		TEXT("NewPawn=%s PawnClass=%s"),
		*BGLogHelper::SafeName(InPawn),
		*BGLogHelper::SafeClass(InPawn));

	BG_SHIN_LOG_CONTROLLER(this, this, "After SetPawn");
	
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

	ABG_Character* BGCharacter = GetBGCharacter();

	if (!HealthViewModel)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: RefreshHUDViewModelBinding failed because HUDViewModel was null."), *GetNameSafe(this));
	}
	else if (BGCharacter)
	{
		HealthViewModel->NotifyPossessedCharacterReady(BGCharacter);
	}
	else
	{
		HealthViewModel->NotifyPossessedCharacterCleared();
	}

	if (!InventoryViewModel)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: RefreshViewModelBinding failed because InventoryViewModel was null."), *GetNameSafe(this));
	}
	else if (BGCharacter)
	{
		InventoryViewModel->NotifyPossessedCharacterReady(BGCharacter);
	}
	else
	{
		InventoryViewModel->NotifyPossessedCharacterCleared();
	}
}

void ABG_PlayerController::NotifyInventoryFailure(
	EBGInventoryFailReason FailReason,
	EBG_ItemType ItemType,
	FGameplayTag ItemTag)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!InventoryViewModel)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: NotifyInventoryFailure failed because InventoryViewModel was null."), *GetNameSafe(this));
		return;
	}

	InventoryViewModel->NotifyInventoryFailure(FailReason, ItemType, ItemTag);
}

void ABG_PlayerController::OpenInventoryUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!EnsureInventoryWidget())
	{
		return;
	}

	if (InventoryViewModel)
	{
		InventoryViewModel->RefreshAllRenderData();
		InventoryViewModel->ForceBroadcastAll();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s: OpenInventoryUI could not refresh because InventoryViewModel was null."), *GetNameSafe(this));
	}

	InventoryWidget->SetVisibility(ESlateVisibility::Visible);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ABG_PlayerController::CloseInventoryUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!InventoryWidget)
	{
		return;
	}

	InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	bShowMouseCursor = false;
}

void ABG_PlayerController::ToggleInventoryUI()
{
	if (IsInventoryUIOpen())
	{
		CloseInventoryUI();
		return;
	}

	OpenInventoryUI();
}

bool ABG_PlayerController::IsInventoryUIOpen() const
{
	return InventoryWidget && InventoryWidget->IsInViewport() && InventoryWidget->IsVisible();
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

bool ABG_PlayerController::EnsureInventoryWidget()
{
	if (!IsLocalController())
	{
		return false;
	}

	if (InventoryWidget)
	{
		if (!InventoryWidget->IsInViewport() && !InventoryWidget->AddToPlayerScreen(50))
		{
			UE_LOG(LogTemp, Error, TEXT("%s: EnsureInventoryWidget failed to add existing InventoryWidget to player screen."), *GetNameSafe(this));
			return false;
		}

		return true;
	}

	if (!InventoryWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: EnsureInventoryWidget failed because InventoryWidgetClass was null."), *GetNameSafe(this));
		return false;
	}

	InventoryWidget = CreateWidget<UUserWidget>(this, InventoryWidgetClass);
	if (!InventoryWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: EnsureInventoryWidget failed because CreateWidget returned null."), *GetNameSafe(this));
		return false;
	}

	InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (!InventoryWidget->AddToPlayerScreen(50))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: EnsureInventoryWidget failed to add InventoryWidget to player screen."), *GetNameSafe(this));
		return false;
	}

	return true;
}

void ABG_PlayerController::RefreshMappingContext()
{
	BG_SHIN_LOG_INFO(TEXT("RefreshMappingContext start IsLocal=%s"),
		IsLocalController() ? TEXT("true") : TEXT("false"));

	if (!IsLocalController())
	{
		BG_SHIN_LOG_WARN(TEXT("RefreshMappingContext skipped because controller is not local"));
		return;
	}

	UInputMappingContext* DefaultMappingContext = InputConfig.DefaultMappingContext.Get();
	if (!DefaultMappingContext)
	{
		BG_SHIN_LOG_ERROR(TEXT("RefreshMappingContext failed because DefaultMappingContext was null"));
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
			Subsystem->AddMappingContext(DefaultMappingContext, 0);

			BG_SHIN_LOG_INFO(TEXT("RefreshMappingContext applied MappingContext=%s"),
				*BGLogHelper::SafeName(DefaultMappingContext));
			return;
		}

		BG_SHIN_LOG_ERROR(TEXT("RefreshMappingContext failed because EnhancedInputLocalPlayerSubsystem was null"));
		return;
	}

	BG_SHIN_LOG_ERROR(TEXT("RefreshMappingContext failed because LocalPlayer was null"));
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
	BindStartedIfValid(EnhancedInputComponent, InputConfig.ReloadAction.Get(), this, &ABG_PlayerController::OnReloadInputStarted);

	// 상호작용 키
	BindStartedIfValid(EnhancedInputComponent, InputConfig.InteractAction.Get(), this, &ABG_PlayerController::OnInteractInputStarted);
	BindStartedIfValid(EnhancedInputComponent, InputConfig.InventoryAction.Get(), this, &ABG_PlayerController::OnInventoryInputStarted);
	
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABG_PlayerController::OnEquipPistolInputStarted);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABG_PlayerController::OnEquipRifleInputStarted);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ABG_PlayerController::OnUseBandageInputStarted);
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ABG_PlayerController::OnUnequipWeaponInputStarted);

	LastBoundCharacter = BGCharacter;
	LastBoundInputComponent = InputComponent;
}

void ABG_PlayerController::OnMoveInputTriggered(const FInputActionValue& Value)
{
	BG_SHIN_LOG_INFO(TEXT("OnMoveInputTriggered"));
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

void ABG_PlayerController::OnUseBandageInputStarted()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!InventoryViewModel)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: OnUseBandageInputStarted failed because InventoryViewModel was null."), *GetNameSafe(this));
		return;
	}

	const FGameplayTag BandageItemTag = FGameplayTag::RequestGameplayTag(BandageItemTagName, false);
	if (!BandageItemTag.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: OnUseBandageInputStarted failed because gameplay tag %s was not registered."),
		       *GetNameSafe(this),
		       *BandageItemTagName.ToString());
		NotifyInventoryFailure(EBGInventoryFailReason::InvalidItem, EBG_ItemType::Heal, BandageItemTag);
		return;
	}

	InventoryViewModel->RequestUseInventoryItem(EBG_ItemType::Heal, BandageItemTag);
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

void ABG_PlayerController::OnReloadInputStarted()
{
	if (ABG_Character* BGCharacter = GetBGCharacter())
	{
		BGCharacter->ReloadFromInput();
		return;
	}

	UE_LOG(LogTemp, Error, TEXT("%s: OnReloadInputStarted failed because controlled character was null."), *GetNameSafe(this));
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

void ABG_PlayerController::OnInventoryInputStarted()
{
	ToggleInventoryUI();
}


ABG_Character* ABG_PlayerController::GetBGCharacter() const
{
	return Cast<ABG_Character>(GetPawn());
}

void ABG_PlayerController::HandleControlledCharacterDeath(ABG_Character* DeadCharacter)
{
	if (!DeadCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: HandleControlledCharacterDeath failed because DeadCharacter was null."), *GetNameSafe(this));
		return;
	}

	if (GetPawn() != DeadCharacter)
	{
		return;
	}

	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	CloseInventoryUI();

	if (HasAuthority())
	{
		StartSpectatingOnly();
	}

	PrepareSpectatorMode(DeadCharacter);
}

void ABG_PlayerController::HandleControlledCharacterRevived(ABG_Character* RevivedCharacter)
{
	if (!RevivedCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: HandleControlledCharacterRevived failed because RevivedCharacter was null."), *GetNameSafe(this));
		return;
	}

	if (GetPawn() != RevivedCharacter)
	{
		return;
	}

	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
}

void ABG_PlayerController::PrepareSpectatorMode_Implementation(ABG_Character* DeadCharacter)
{
	if (!DeadCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: PrepareSpectatorMode failed because DeadCharacter was null."), *GetNameSafe(this));
	}
}
