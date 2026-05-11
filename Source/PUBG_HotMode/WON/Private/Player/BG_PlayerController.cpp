#include "Player/BG_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GameplayTagContainer.h"
#include "UI/BG_HealthViewModel.h"
#include "Inventory/BG_EquipmentComponent.h"
#include "UI/BG_InventoryViewModel.h"
#include "UI/BG_WeaponIconCaptureComponent.h"
#include "Inventory/BG_ItemTypes.h"
#include "Player/BG_Character.h"
#include "Blueprint/UserWidget.h"
#include "PUBG_HotMode/ConstructionHelperExtension.h"
#include "UObject/ConstructorHelpers.h"
#include "Utils/BG_LogHelper.h"
#include "InputMappingContext.h"
#include "Actors/BG_Airplane.h"
#include "Actors/BG_AirplaneCameraRig.h"
#include "Kismet/GameplayStatics.h"

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
	constexpr int32 InventoryHUDZOrder = 50;
	constexpr int32 GameHUDZOrder = 75;
}


ABG_PlayerController::ABG_PlayerController()
{
	EXT_CREATE_DEFAULT_SUBOBJECT(HealthViewModel);
	EXT_CREATE_DEFAULT_SUBOBJECT(InventoryViewModel);
	EXT_CREATE_DEFAULT_SUBOBJECT(WeaponIconCaptureComponent);
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
	
	ShowBattleWaitingTimeUI();
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

	if (WeaponIconCaptureComponent)
	{
		WeaponIconCaptureComponent->ClearPreviewCache();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s: CloseInventoryUI could not clear weapon preview cache because WeaponIconCaptureComponent was null."), *GetNameSafe(this));
	}

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
		GameHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!GameHUDWidget->IsInViewport() && !GameHUDWidget->AddToPlayerScreen(GameHUDZOrder))
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

	GameHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!GameHUDWidget->AddToPlayerScreen(GameHUDZOrder))
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
		if (!InventoryWidget->IsInViewport() && !InventoryWidget->AddToPlayerScreen(InventoryHUDZOrder))
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
	if (!InventoryWidget->AddToPlayerScreen(InventoryHUDZOrder))
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
	InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ABG_PlayerController::OnUnequipWeaponInputStarted);

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
	const FVector2D LookInput = Value.Get<FVector2D>();

	if (AirplaneCameraRig)
	{
		AirplaneCameraRig->AddLookInput(LookInput);
		return;
	}
	
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
		BGCharacter->SetEquippedWeapon(nullptr);
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

	ABG_Character* BGCharacter = GetBGCharacter();
	if (!BGCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: OnUseBandageInputStarted failed because controlled character was null."),
		       *GetNameSafe(this));
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

	BGCharacter->UseInventoryItem(EBG_ItemType::Heal, BandageItemTag);
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
	if (IsInAirplaneView())
	{
		TryExitAirplane();
		return;
	}
	
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
		Client_ShowEndUI();
		StartSpectatingOnly();
	}
	else if (IsLocalController())
	{
		ShowEndUI();
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

void ABG_PlayerController::ShowBattleWaitingTimeUI()
{
	if (!IsLocalController())
	{
		BG_SHIN_LOG_WARN(TEXT("ShowBattleWaitingTimeUI skipped because controller is not local"));
		return;
	}

	if (BattleWaitingTimeWidget)
	{
		BG_SHIN_LOG_WARN(TEXT("ShowBattleWaitingTimeUI skipped because widget already exists"));
		return;
	}

	if (!BattleWaitingTimeWidgetClass)
	{
		BG_SHIN_LOG_ERROR(TEXT("BattleWaitingTimeWidgetClass is not set"));
		return;
	}

	BattleWaitingTimeWidget = CreateWidget<UUserWidget>(this, BattleWaitingTimeWidgetClass);
	if (!BattleWaitingTimeWidget)
	{
		BG_SHIN_LOG_ERROR(TEXT("CreateWidget failed for BattleWaitingTimeWidgetClass=%s"),
			*BGLogHelper::SafeClass(BattleWaitingTimeWidgetClass));
		return;
	}

	if (!BattleWaitingTimeWidget->AddToPlayerScreen(100))
	{
		BG_SHIN_LOG_ERROR(TEXT("Failed to add BattleWaitingTimeWidget to player screen"));
		BattleWaitingTimeWidget = nullptr;
		return;
	}

	BG_SHIN_LOG_INFO(TEXT("BattleWaitingTime widget added to player screen"));
}

void ABG_PlayerController::HideBattleWaitingTimeUI()
{
	if (!BattleWaitingTimeWidget)
	{
		BG_SHIN_LOG_WARN(TEXT("HideBattleWaitingTimeUI skipped because widget does not exist"));
		return;
	}

	BattleWaitingTimeWidget->RemoveFromParent();
	BattleWaitingTimeWidget = nullptr;

	BG_SHIN_LOG_INFO(TEXT("BattleWaitingTime widget removed from player screen"));
}

void ABG_PlayerController::Client_HideBattleWaitingTimeUI_Implementation()
{
	BG_SHIN_LOG_EVENT_BLOCK(this, "Client_HideBattleWaitingTimeUI",
		TEXT("Removing battle waiting time UI"));

	HideBattleWaitingTimeUI();
}

void ABG_PlayerController::Client_BeginAirplaneView_Implementation(
	ABG_Airplane* InAirplane,
	FVector InStartLocation,
	FVector InEndLocation,
	float InFlightSpeed,
	float InFlightStartTimeSeconds
)
{
	BG_SHIN_LOG_INFO(TEXT("Client_BeginAirplaneView_Implementation CALLED"));

	BeginAirplaneView(InAirplane);

	if (AirplaneCameraRig)
	{
		AirplaneCameraRig->InitializeFlightSnapshot(
			InAirplane,
			InStartLocation,
			InEndLocation,
			InFlightSpeed,
			InFlightStartTimeSeconds
		);
	}
}

void ABG_PlayerController::Client_EndAirplaneView_Implementation()
{
	EndAirplaneView();
}

void ABG_PlayerController::BeginAirplaneView(ABG_Airplane* InAirplane)
{
	if (!IsLocalController())
	{
		BG_SHIN_LOG_WARN(TEXT("BeginAirplaneView skipped because controller is not local"));
		return;
	}

	if (!InAirplane)
	{
		BG_SHIN_LOG_ERROR(TEXT("BeginAirplaneView failed because InAirplane was null"));
		return;
	}

	if (!AirplaneCameraRigClass)
	{
		BG_SHIN_LOG_ERROR(TEXT("BeginAirplaneView failed because AirplaneCameraRigClass was not set"));
		return;
	}

	if (AirplaneCameraRig)
	{
		BG_SHIN_LOG_WARN(TEXT("BeginAirplaneView skipped because AirplaneCameraRig already exists"));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AirplaneCameraRig = GetWorld()->SpawnActor<ABG_AirplaneCameraRig>(
		AirplaneCameraRigClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (!AirplaneCameraRig)
	{
		BG_SHIN_LOG_ERROR(TEXT("Failed to spawn AirplaneCameraRig from class=%s"),
			*BGLogHelper::SafeClass(AirplaneCameraRigClass));
		return;
	}

	AirplaneCameraRig->Initialize(InAirplane);
	SetViewTargetWithBlend(AirplaneCameraRig, 1.0f);

	BG_SHIN_LOG_INFO(TEXT("BeginAirplaneView succeeded with airplane=%s rig=%s"),
		*BGLogHelper::SafeName(InAirplane),
		*BGLogHelper::SafeName(AirplaneCameraRig));
}

void ABG_PlayerController::EndAirplaneView()
{
	if (!IsLocalController())
	{
		BG_SHIN_LOG_WARN(TEXT("EndAirplaneView skipped because controller is not local"));
		return;
	}

	if (GetPawn())
	{
		SetViewTargetWithBlend(GetPawn(), 1.0f);
	}

	if (AirplaneCameraRig)
	{
		AirplaneCameraRig->Destroy();
		AirplaneCameraRig = nullptr;
	}

	BG_SHIN_LOG_INFO(TEXT("EndAirplaneView completed"));
}

void ABG_PlayerController::TryExitAirplane()
{
	if (!AirplaneCameraRig)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: TryExitAirplane failed because AirplaneCameraRig was null."), *GetNameSafe(this));
		return;
	}

	ABG_Character* BGCharacter = GetBGCharacter();
	if (!BGCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: TryExitAirplane failed because controlled character was null."), *GetNameSafe(this));
		return;
	}

	const FVector DropLocation = AirplaneCameraRig->GetActorLocation() + FVector(0.0f, 0.0f, -150.0f);
	const FVector DropForwardVector = AirplaneCameraRig->GetActorForwardVector();

	BGCharacter->Server_BeginAirplaneDrop(DropLocation, DropForwardVector);
	EndAirplaneView();
}

void ABG_PlayerController::ShowEndUI()
{
	UE_LOG(LogTemp, Warning,
	TEXT("%s: ShowEndUI called. Local=%d EndUIWidgetClass=%s"),
	*GetNameSafe(this),
	IsLocalController() ? 1 : 0,
	*GetNameSafe(EndUIWidgetClass));
	
	if (!IsLocalController())
	{
		return;
	}

	if (EndUIWidget)
	{
		if (!EndUIWidget->IsInViewport())
		{
			EndUIWidget->AddToPlayerScreen(200);
		}
		return;
	}

	if (!EndUIWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ShowEndUI failed because EndUIWidgetClass was null."), *GetNameSafe(this));
		return;
	}

	EndUIWidget = CreateWidget<UUserWidget>(this, EndUIWidgetClass);
	if (!EndUIWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ShowEndUI failed because CreateWidget returned null."), *GetNameSafe(this));
		return;
	}

	if (!EndUIWidget->AddToPlayerScreen(200))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ShowEndUI failed to add widget to player screen."), *GetNameSafe(this));
		EndUIWidget = nullptr;
	}
}

void ABG_PlayerController::ShowChickenUI()
{
	if (!IsLocalController())
	{
		return;
	}

	if (ChickenUIWidget)
	{
		if (!ChickenUIWidget->IsInViewport())
		{
			ChickenUIWidget->AddToPlayerScreen(210);
		}
		return;
	}

	if (!ChickenUIWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ShowChickenUI failed because ChickenUIWidgetClass was null."), *GetNameSafe(this));
		return;
	}

	ChickenUIWidget = CreateWidget<UUserWidget>(this, ChickenUIWidgetClass);
	if (!ChickenUIWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ShowChickenUI failed because CreateWidget returned null."), *GetNameSafe(this));
		return;
	}

	if (!ChickenUIWidget->AddToPlayerScreen(210))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: ShowChickenUI failed to add widget to player screen."), *GetNameSafe(this));
		ChickenUIWidget = nullptr;
	}
}

void ABG_PlayerController::Client_ShowChickenUI_Implementation()
{
	ShowChickenUI();
}

void ABG_PlayerController::Client_ShowEndUI_Implementation()
{
	ShowEndUI();
}
