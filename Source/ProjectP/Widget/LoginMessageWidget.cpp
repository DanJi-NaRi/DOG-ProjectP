#include "LoginMessageWidget.h"

#include "../GameInstance/SubSystems/NetSub/SessionTravelSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

void ULoginMessageWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_OK)
    {
        BTN_OK->OnClicked.RemoveDynamic(this, &ULoginMessageWidget::HandleOkClicked);
        BTN_OK->OnClicked.AddDynamic(this, &ULoginMessageWidget::HandleOkClicked);
    }
}

void ULoginMessageWidget::NativeDestruct()
{
    if (BTN_OK)
    {
        BTN_OK->OnClicked.RemoveDynamic(this, &ULoginMessageWidget::HandleOkClicked);
    }

    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 결과 메시지와 성공 여부를 위젯에 반영하는 함수
// MessageText : 메시지 위젯에 표시할 문구
// IsSuccess : 로그인 성공 여부
void ULoginMessageWidget::InitMessage(const FText& MessageText, bool IsSuccess)
{
    bIsSuccess = IsSuccess;

    if (TXT_Message)
    {
        TXT_Message->SetText(MessageText);
    }
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 확인 버튼 클릭 시 성공 메시지는 로비 이동을 요청하고 실패 메시지는 위젯을 닫는 함수
void ULoginMessageWidget::HandleOkClicked()
{
    if (bIsSuccess)
    {
        RequestLobbyTravelAfterLoginSuccess();
        return;
    }

    RemoveFromParent();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 성공 후 세션 검증을 거쳐 로비 서버 이동을 요청하는 함수
void ULoginMessageWidget::RequestLobbyTravelAfterLoginSuccess()
{
    UGameInstance* GameInstance = GetGameInstance();
    USessionTravelSubsystem* SessionTravelSubsystem = GameInstance
        ? GameInstance->GetSubsystem<USessionTravelSubsystem>()
        : nullptr;
    if (SessionTravelSubsystem == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel after login because SessionTravelSubsystem is missing."));
        return;
    }

    APlayerController* PlayerController = GetOwningPlayer();
    if (PlayerController == nullptr && GetWorld())
    {
        PlayerController = GetWorld()->GetFirstPlayerController();
    }

    if (PlayerController == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot travel after login because PlayerController is missing."));
        return;
    }

    RemoveFromParent();
    SessionTravelSubsystem->VerifySessionAndTravelToLobby(PlayerController);
}
