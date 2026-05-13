#include "NickNameWidget.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/TextBlock.h"
#include "Internationalization/Regex.h"
#include "GameFramework/BG_GameInstance.h"

void UNickNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UNickNameWidget::OnClickedConfirmButton);
	}

	if (ErrorText)
	{
		ErrorText->SetText(FText::GetEmpty());
	}
}

void UNickNameWidget::OnClickedConfirmButton()
{
	if (!NickNameEditableText)
	{
		return;
	}

	const FString NickName = NickNameEditableText->GetText().ToString();

	if (IsValidNickName(NickName))
	{
		if (ErrorText)
		{
			ErrorText->SetText(FText::FromString(TEXT("사용 가능한 닉네임입니다.")));
		}

		// TODO:
		// 여기서 닉네임 저장 또는 서버 전송 처리
		UBG_GameInstance* BGGameInstance = Cast<UBG_GameInstance>(GetGameInstance());
		BGGameInstance->SetPendingPlayerNickName(NickName);
		BGGameInstance->ConnectToDedicatedServer();
	}
	else
	{
		if (ErrorText)
		{
			ErrorText->SetText(
				FText::FromString(TEXT("사용 불가능한 닉네임입니다."))
			);
		}
	}
}

bool UNickNameWidget::IsValidNickName(const FString& InNickName) const
{
	static const FRegexPattern NickNamePattern(TEXT("^[A-Za-z][A-Za-z0-9_-]{3,15}$"));
	FRegexMatcher Matcher(NickNamePattern, InNickName);

	return Matcher.FindNext();
}