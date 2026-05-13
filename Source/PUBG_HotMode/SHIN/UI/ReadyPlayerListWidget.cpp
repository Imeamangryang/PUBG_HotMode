#include "UI/ReadyPlayerListWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/BG_GameState.h"
#include "Utils/BG_LogHelper.h"

void UReadyPlayerListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindGameStateDelegate();
	RefreshReadyPlayerCount();
}

void UReadyPlayerListWidget::NativeDestruct()
{
	if (CachedGameState)
	{
		CachedGameState->OnLobbyPlayerListChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UReadyPlayerListWidget::BindGameStateDelegate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("BindGameStateDelegate failed because World was null"));
		return;
	}

	CachedGameState = World->GetGameState<ABG_GameState>();
	if (!CachedGameState)
	{
		BG_SHIN_LOG_ERROR(TEXT("BindGameStateDelegate failed because GameState was null"));
		return;
	}

	CachedGameState->OnLobbyPlayerListChanged.RemoveAll(this);
	CachedGameState->OnLobbyPlayerListChanged.AddDynamic(this, &UReadyPlayerListWidget::RefreshReadyPlayerCount);

	BG_SHIN_LOG_INFO(TEXT("ReadyPlayerListWidget bound to OnLobbyPlayerListChanged"));
}

void UReadyPlayerListWidget::RefreshReadyPlayerCount()
{
	if (!ReadyCountText)
	{
		BG_SHIN_LOG_ERROR(TEXT("RefreshReadyPlayerCount failed because ReadyCountText was null"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		BG_SHIN_LOG_ERROR(TEXT("RefreshReadyPlayerCount failed because World was null"));
		return;
	}

	ABG_GameState* BGGameState = World->GetGameState<ABG_GameState>();
	if (!BGGameState)
	{
		BG_SHIN_LOG_ERROR(TEXT("RefreshReadyPlayerCount failed because GameState was null"));
		return;
	}

	const int32 ReadyPlayerCount = BGGameState->GetLobbyReadyPlayerCount();
	const int32 TotalPlayerCount = BGGameState->GetLobbyTotalPlayerCount();

	ReadyCountText->SetText(FText::FromString(
		FString::Printf(TEXT("%d / %d"), ReadyPlayerCount, TotalPlayerCount)
	));

	BG_SHIN_LOG_INFO(TEXT("Ready player count updated: %d / %d"), ReadyPlayerCount, TotalPlayerCount);
}