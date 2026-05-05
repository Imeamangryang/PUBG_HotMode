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

	UFUNCTION(BlueprintCallable)
	void ConnectToDedicatedServer();

	UFUNCTION(BlueprintCallable, Category = "BG|Loading")
	void StartLoadingScreen();

	UFUNCTION(BlueprintCallable, Category = "BG|Loading")
	void StopLoadingScreen();

protected:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

	void ShowLoadingScreen();
	void HideLoadingScreen();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BG|Loading")
	TSubclassOf<class UUserWidget> LoadingWidgetClass;

	UPROPERTY(Transient)
	class UUserWidget* LoadingWidgetInstance = nullptr;
};