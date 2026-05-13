#include "CoreMinimal.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/BG_ItemDataRegistrySubsystem.h"
#include "Inventory/BG_ItemDataRow.h"
#include "Player/BG_Character.h"

#if !UE_BUILD_SHIPPING

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogBGPUBGConsoleCommands, Log, All);

FString GetConsoleItemTypeName(EBG_ItemType ItemType)
{
	const UEnum* ItemTypeEnum = StaticEnum<EBG_ItemType>();
	return ItemTypeEnum ? ItemTypeEnum->GetNameStringByValue(static_cast<int64>(ItemType)) : TEXT("<Unknown>");
}

bool TryParseBoolArgument(const FString& Value, bool& OutValue)
{
	const FString NormalizedValue = Value.TrimStartAndEnd().ToLower();
	if (NormalizedValue == TEXT("true") || NormalizedValue == TEXT("1") || NormalizedValue == TEXT("on") || NormalizedValue == TEXT("yes"))
	{
		OutValue = true;
		return true;
	}

	if (NormalizedValue == TEXT("false") || NormalizedValue == TEXT("0") || NormalizedValue == TEXT("off") || NormalizedValue == TEXT("no"))
	{
		OutValue = false;
		return true;
	}

	return false;
}

bool TryParsePositiveIntArgument(const FString& Value, int32& OutValue)
{
	OutValue = 0;
	return LexTryParseString(OutValue, *Value) && OutValue > 0;
}

void PrintConsoleResult(UWorld* World, const FString& Message, const FColor& Color, bool bIsError)
{
	if (bIsError)
	{
		UE_LOG(LogBGPUBGConsoleCommands, Error, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogBGPUBGConsoleCommands, Log, TEXT("%s"), *Message);
	}

	if (GEngine && World && World->GetNetMode() != NM_DedicatedServer)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, Color, Message);
	}
}

bool MatchesTagRoot(const FGameplayTag& ItemTag, const TCHAR* RootTagName)
{
	const FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(FName(RootTagName), false);
	return RootTag.IsValid() && ItemTag.MatchesTag(RootTag);
}

EBG_ItemType ResolveItemTypeFromTag(const FGameplayTag& ItemTag)
{
	if (MatchesTagRoot(ItemTag, TEXT("Item.Ammo")))
	{
		return EBG_ItemType::Ammo;
	}
	if (MatchesTagRoot(ItemTag, TEXT("Item.Heal")))
	{
		return EBG_ItemType::Heal;
	}
	if (MatchesTagRoot(ItemTag, TEXT("Item.Boost")))
	{
		return EBG_ItemType::Boost;
	}
	if (MatchesTagRoot(ItemTag, TEXT("Item.Throwable")))
	{
		return EBG_ItemType::Throwable;
	}
	if (MatchesTagRoot(ItemTag, TEXT("Item.Weapon")))
	{
		return EBG_ItemType::Weapon;
	}
	if (MatchesTagRoot(ItemTag, TEXT("Item.Equipment.Backpack")))
	{
		return EBG_ItemType::Backpack;
	}
	if (MatchesTagRoot(ItemTag, TEXT("Item.Equipment.Helmet")) || MatchesTagRoot(ItemTag, TEXT("Item.Equipment.Vest")))
	{
		return EBG_ItemType::Armor;
	}

	return EBG_ItemType::None;
}

bool IsGiveSupportedItemType(EBG_ItemType ItemType)
{
	return ItemType == EBG_ItemType::Ammo
		|| ItemType == EBG_ItemType::Heal
		|| ItemType == EBG_ItemType::Boost
		|| ItemType == EBG_ItemType::Throwable;
}

UBG_ItemDataRegistrySubsystem* GetItemDataRegistrySubsystem(UWorld* World, const TCHAR* CommandName)
{
	if (!World)
	{
		UE_LOG(LogBGPUBGConsoleCommands, Error, TEXT("%s failed because World was null."), CommandName);
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogBGPUBGConsoleCommands, Error, TEXT("%s failed because GameInstance was null."), CommandName);
		return nullptr;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GameInstance->GetSubsystem<UBG_ItemDataRegistrySubsystem>();
	if (!RegistrySubsystem)
	{
		UE_LOG(LogBGPUBGConsoleCommands, Error, TEXT("%s failed because UBG_ItemDataRegistrySubsystem was null."), CommandName);
		return nullptr;
	}

	return RegistrySubsystem;
}

bool TryResolveGiveItem(UWorld* World, const FGameplayTag& ItemTag, EBG_ItemType& OutItemType)
{
	OutItemType = ResolveItemTypeFromTag(ItemTag);
	if (OutItemType == EBG_ItemType::None)
	{
		PrintConsoleResult(World, FString::Printf(TEXT("PUBG.Give failed because item tag %s has no supported item type prefix."), *ItemTag.ToString()), FColor::Red, true);
		return false;
	}

	if (!IsGiveSupportedItemType(OutItemType))
	{
		PrintConsoleResult(World, FString::Printf(TEXT("PUBG.Give failed because item type %s is not supported."), *GetConsoleItemTypeName(OutItemType)), FColor::Red, true);
		return false;
	}

	UBG_ItemDataRegistrySubsystem* RegistrySubsystem = GetItemDataRegistrySubsystem(World, TEXT("PUBG.Give"));
	if (!RegistrySubsystem)
	{
		return false;
	}

	FString FailureReason;
	if (!RegistrySubsystem->FindItemRow(OutItemType, ItemTag, &FailureReason))
	{
		PrintConsoleResult(World, FString::Printf(TEXT("PUBG.Give failed because item row validation failed for %s %s. %s"),
			*GetConsoleItemTypeName(OutItemType),
			*ItemTag.ToString(),
			*FailureReason), FColor::Red, true);
		return false;
	}

	return true;
}

ABG_Character* FindConsoleTargetCharacter(UWorld* World, const TCHAR* CommandName)
{
	if (!World)
	{
		UE_LOG(LogBGPUBGConsoleCommands, Error, TEXT("%s failed because World was null."), CommandName);
		return nullptr;
	}

	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (ABG_Character* Character = Cast<ABG_Character>(PlayerController->GetPawn()))
		{
			return Character;
		}
	}

	for (TActorIterator<ABG_Character> It(World); It; ++It)
	{
		ABG_Character* Character = *It;
		if (IsValid(Character))
		{
			return Character;
		}
	}

	UE_LOG(LogBGPUBGConsoleCommands, Error, TEXT("%s failed because no ABG_Character was found."), CommandName);
	return nullptr;
}

void HandleGiveCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() != 2)
	{
		PrintConsoleResult(World, TEXT("Usage: PUBG.Give <itemtag> <count>"), FColor::Red, true);
		return;
	}

	const FGameplayTag ItemTag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), false);
	if (!ItemTag.IsValid())
	{
		PrintConsoleResult(World, FString::Printf(TEXT("PUBG.Give failed because item tag %s was invalid."), *Args[0]), FColor::Red, true);
		return;
	}

	int32 Quantity = 0;
	if (!TryParsePositiveIntArgument(Args[1], Quantity))
	{
		PrintConsoleResult(World, TEXT("PUBG.Give failed because count was invalid. Use a positive integer."), FColor::Red, true);
		return;
	}

	EBG_ItemType ItemType = EBG_ItemType::None;
	if (!TryResolveGiveItem(World, ItemTag, ItemType))
	{
		return;
	}

	ABG_Character* Character = FindConsoleTargetCharacter(World, TEXT("PUBG.Give"));
	if (!Character)
	{
		return;
	}

	const bool bHasAuthority = Character->HasAuthority();
	int32 AddedQuantity = 0;
	const bool bAccepted = Character->GiveInventoryItem(ItemType, ItemTag, Quantity, AddedQuantity);
	const FString Message = bHasAuthority
		? FString::Printf(TEXT("PUBG.Give %s %s x%d/%d for %s."),
			bAccepted ? TEXT("added") : TEXT("failed to add"),
			*ItemTag.ToString(),
			AddedQuantity,
			Quantity,
			*GetNameSafe(Character))
		: FString::Printf(TEXT("PUBG.Give %s %s x%d for %s."),
			bAccepted ? TEXT("requested") : TEXT("failed to request"),
			*ItemTag.ToString(),
			Quantity,
			*GetNameSafe(Character));
	PrintConsoleResult(World, Message, bAccepted ? FColor::Green : FColor::Red, !bAccepted);
}

void HandleInfAmmoCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() != 1)
	{
		PrintConsoleResult(World, TEXT("Usage: PUBG.InfAmmo <true|false>"), FColor::Red, true);
		return;
	}

	bool bEnableInfiniteAmmo = false;
	if (!TryParseBoolArgument(Args[0], bEnableInfiniteAmmo))
	{
		PrintConsoleResult(World, TEXT("PUBG.InfAmmo failed because bool argument was invalid. Use true/false, 1/0, on/off, or yes/no."), FColor::Red, true);
		return;
	}

	ABG_Character* Character = FindConsoleTargetCharacter(World, TEXT("PUBG.InfAmmo"));
	if (!Character)
	{
		return;
	}

	const bool bHasAuthority = Character->HasAuthority();
	const bool bAccepted = Character->SetInfiniteAmmo(bEnableInfiniteAmmo);
	const FString Message = FString::Printf(
		TEXT("PUBG.InfAmmo %s %s for %s."),
		bAccepted ? (bHasAuthority ? TEXT("set") : TEXT("requested")) : TEXT("failed to set"),
		bEnableInfiniteAmmo ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Character));
	PrintConsoleResult(World, Message, bAccepted ? FColor::Green : FColor::Red, !bAccepted);
}

FAutoConsoleCommandWithWorldAndArgs GPUBGGiveCommand(
	TEXT("PUBG.Give"),
	TEXT("Adds a regular inventory item through the server-authority inventory path. Usage: PUBG.Give <itemtag> <count>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleGiveCommand),
	ECVF_Cheat);

FAutoConsoleCommandWithWorldAndArgs GPUBGInfAmmoCommand(
	TEXT("PUBG.InfAmmo"),
	TEXT("Sets bUseInfiniteDebugAmmo on the active equipped weapon. Usage: PUBG.InfAmmo <true|false>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleInfAmmoCommand),
	ECVF_Cheat);
}

#endif
