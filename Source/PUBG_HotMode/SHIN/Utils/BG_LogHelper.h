#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "PUBG_HotMode/PUBG_HotMode.h"

namespace BGLogHelper
{
	static FORCEINLINE FString SafeName(const UObject* Object)
	{
		return IsValid(Object) ? Object->GetName() : TEXT("None");
	}

	static FORCEINLINE FString SafeClass(const UObject* Object)
	{
		return (IsValid(Object) && Object->GetClass())
			? Object->GetClass()->GetName()
			: TEXT("None");
	}

	static FORCEINLINE FString SafeClass(const UClass* Class)
	{
		return IsValid(Class) ? Class->GetName() : TEXT("None");
	}

	static FORCEINLINE FString GetNetModeString(const UWorld* World)
	{
		if (!World)
		{
			return TEXT("NoWorld");
		}

		switch (World->GetNetMode())
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
		}
	}

	static FORCEINLINE FString GetWorldName(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return TEXT("None");
		}

		const UWorld* World = WorldContextObject->GetWorld();
		return World ? World->GetMapName() : TEXT("None");
	}

	static FORCEINLINE FString GetControllerSummary(const AController* Controller)
	{
		if (!Controller)
		{
			return TEXT("Controller=None Class=None PlayerState=None Pawn=None PawnClass=None HasAuthority=false");
		}

		const APawn* Pawn = Controller->GetPawn();
		const APlayerState* PlayerState = Controller->PlayerState;

		return FString::Printf(
			TEXT("Controller=%s Class=%s PlayerState=%s Pawn=%s PawnClass=%s HasAuthority=%s"),
			*SafeName(Controller),
			*SafeClass(Controller),
			*SafeName(PlayerState),
			*SafeName(Pawn),
			*SafeClass(Pawn),
			Controller->HasAuthority() ? TEXT("true") : TEXT("false"));
	}

	static FORCEINLINE FString GetActorSummary(const AActor* Actor)
	{
		if (!Actor)
		{
			return TEXT("Actor=None Class=None Location=None HasAuthority=false");
		}

		return FString::Printf(
			TEXT("Actor=%s Class=%s Location=%s HasAuthority=%s"),
			*SafeName(Actor),
			*SafeClass(Actor),
			*Actor->GetActorLocation().ToCompactString(),
			Actor->HasAuthority() ? TEXT("true") : TEXT("false"));
	}
}

#define BG_SHIN_LOG_SEPARATOR() \
	do { \
		UE_LOG(Game, Log, TEXT("=======")); \
		UE_LOG(Game, Log, TEXT("Log")); \
		UE_LOG(Game, Log, TEXT("=======")); \
	} while (0)

#define BG_SHIN_LOG_SECTION(TitleLiteral) \
	do { \
		UE_LOG(Game, Log, TEXT("=======")); \
		UE_LOG(Game, Log, TEXT("%s"), TEXT(TitleLiteral)); \
		UE_LOG(Game, Log, TEXT("=======")); \
	} while (0)

#define BG_SHIN_LOG_INFO(Format, ...) \
	do { \
		UE_LOG(Game, Log, TEXT("[INFO] %s(%d) | " Format), TEXT(__FUNCTION__), __LINE__, ##__VA_ARGS__); \
	} while (0)

#define BG_SHIN_LOG_WARN(Format, ...) \
	do { \
		UE_LOG(Game, Warning, TEXT("[WARN] %s(%d) | " Format), TEXT(__FUNCTION__), __LINE__, ##__VA_ARGS__); \
	} while (0)

#define BG_SHIN_LOG_ERROR(Format, ...) \
	do { \
		UE_LOG(Game, Error, TEXT("[ERROR] %s(%d) | " Format), TEXT(__FUNCTION__), __LINE__, ##__VA_ARGS__); \
	} while (0)

#define BG_SHIN_LOG_WORLD(WorldContextObject, Format, ...) \
	do { \
		const UWorld* BG_InternalWorld = (WorldContextObject) ? (WorldContextObject)->GetWorld() : nullptr; \
		UE_LOG(Game, Log, TEXT("[WORLD] NetMode=%s Map=%s | " Format), \
			*BGLogHelper::GetNetModeString(BG_InternalWorld), \
			*BGLogHelper::GetWorldName(WorldContextObject), \
			##__VA_ARGS__); \
	} while (0)

#define BG_SHIN_LOG_CONTROLLER(WorldContextObject, Controller, PrefixLiteral) \
	do { \
		const UWorld* BG_InternalWorld = (WorldContextObject) ? (WorldContextObject)->GetWorld() : nullptr; \
		UE_LOG(Game, Log, TEXT("[CONTROLLER] %s | NetMode=%s Map=%s | %s"), \
			TEXT(PrefixLiteral), \
			*BGLogHelper::GetNetModeString(BG_InternalWorld), \
			*BGLogHelper::GetWorldName(WorldContextObject), \
			*BGLogHelper::GetControllerSummary(Controller)); \
	} while (0)

#define BG_SHIN_LOG_ACTOR(WorldContextObject, Actor, PrefixLiteral) \
	do { \
		const UWorld* BG_InternalWorld = (WorldContextObject) ? (WorldContextObject)->GetWorld() : nullptr; \
		UE_LOG(Game, Log, TEXT("[ACTOR] %s | NetMode=%s Map=%s | %s"), \
			TEXT(PrefixLiteral), \
			*BGLogHelper::GetNetModeString(BG_InternalWorld), \
			*BGLogHelper::GetWorldName(WorldContextObject), \
			*BGLogHelper::GetActorSummary(Actor)); \
	} while (0)

#define BG_SHIN_LOG_EVENT_BLOCK(WorldContextObject, TitleLiteral, Format, ...) \
	do { \
		const UWorld* BG_InternalWorld = (WorldContextObject) ? (WorldContextObject)->GetWorld() : nullptr; \
		UE_LOG(Game, Log, TEXT("=======")); \
		UE_LOG(Game, Log, TEXT("%s"), TEXT(TitleLiteral)); \
		UE_LOG(Game, Log, TEXT("=======")); \
		UE_LOG(Game, Log, TEXT("NetMode=%s Map=%s"), \
			*BGLogHelper::GetNetModeString(BG_InternalWorld), \
			*BGLogHelper::GetWorldName(WorldContextObject)); \
		UE_LOG(Game, Log, Format, ##__VA_ARGS__); \
	} while (0)