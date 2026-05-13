#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NickNameWidget.generated.h"

class UEditableText;
class UButton;
class UTextBlock;

UCLASS()
class PUBG_HOTMODE_API UNickNameWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> NickNameEditableText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ErrorText;

	UFUNCTION()
	void OnClickedConfirmButton();

	bool IsValidNickName(const FString& InNickName) const;
};