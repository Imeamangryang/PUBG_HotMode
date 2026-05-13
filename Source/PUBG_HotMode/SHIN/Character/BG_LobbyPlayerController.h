#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BG_LobbyPlayerController.generated.h"

UCLASS()
class PUBG_HOTMODE_API ABG_LobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABG_LobbyPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;

public:
	UFUNCTION(BlueprintCallable, Category = "BG|Lobby")
	void RequestStartGame();

	UFUNCTION(Server, Reliable)
	void Server_RequestStartGame();

	UFUNCTION(Client, Reliable)
	void Client_ShowLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "BG|Lobby")
	void ShowLobbyUI();
	
	UFUNCTION(Server, Reliable)
	void ServerSetPlayerNickName(const FString& InNickName);

protected:
	
	void TrySendPendingNickNameToServer();
	bool IsLobbyMap() const;
	bool CanSendNickNameToServer() const;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|LobbyUI")
	TSubclassOf<class UUserWidget> LobbyWidgetClass;

	UPROPERTY()
	class UUserWidget* LobbyWidgetInstance;
	
	bool bNickNameSentToServer = false;
};