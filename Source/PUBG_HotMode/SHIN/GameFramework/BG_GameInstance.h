#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BG_GameInstance.generated.h"

UCLASS()
class PUBG_HOTMODE_API UBG_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "BG|Network")
	void ConnectToDedicatedServer();

	UFUNCTION(BlueprintCallable, Category = "BG|Loading")
	void StartLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "BG|Loading")
	void StopLoadingScreen();
	
	UFUNCTION(BlueprintCallable, Category = "BG|Player")
	void SetPendingPlayerNickName(const FString& InNickName);

	UFUNCTION(BlueprintPure, Category = "BG|Player")
	FString GetPendingPlayerNickName() const { return PendingPlayerNickName; }

protected:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	void ShowLoadingScreen();
	void HideLoadingScreen();

	FString ResolveServerAddress() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Loading")
	TSubclassOf<class UUserWidget> LoadingWidgetClass;

	UPROPERTY(Transient)
	class UUserWidget* LoadingWidgetInstance = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BG|Network")
	FString DefaultServerAddress = TEXT("127.0.0.1:7777");
	
	UPROPERTY(Transient)
	FString PendingPlayerNickName;
};