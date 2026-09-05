#include "LoginWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "../LoginRequestAsyncAction.h"
#include "../GameInstance/SubSystems/NetSub/LoginFlowSubsystem.h"

void ULoginWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (BTN_Login)
    {
        BTN_Login->OnClicked.RemoveDynamic(this, &ULoginWidget::HandleLoginClicked);
        BTN_Login->OnClicked.AddDynamic(this, &ULoginWidget::HandleLoginClicked);
    }
}

void ULoginWidget::NativeDestruct()
{
    if (BTN_Login)
    {
        BTN_Login->OnClicked.RemoveDynamic(this, &ULoginWidget::HandleLoginClicked);
    }

    FinishLoginRequest();

    Super::NativeDestruct();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 입력 위젯에서 ID와 PW를 읽고 로그인 요청을 시작하는 함수
void ULoginWidget::RequestLoginFromInput()
{
    if (ActiveLoginRequest)
    {
        return;
    }

    if (ETB_ID == nullptr || ETB_Password == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoginWidget input binding missing. Required names: ETB_ID, ETB_Password."));
        return;
    }

    const FString ID = ETB_ID->GetText().ToString();
    const FString Password = ETB_Password->GetText().ToString();
    if (ID.IsEmpty() || Password.IsEmpty())
    {
        ShowLoginMessage(EmptyInputMessage, false);
        return;
    }

    ActiveLoginRequest = ULoginRequestAsyncAction::RequestLogin(this, ID, Password, LoginRequestServerUrl);
    if (ActiveLoginRequest == nullptr)
    {
        ShowLoginMessage(FText::FromString(TEXT("Failed to create login request.")), false);
        return;
    }

    ActiveLoginRequest->OnSuccess.AddDynamic(this, &ULoginWidget::HandleLoginRequestSuccess);
    ActiveLoginRequest->OnFailure.AddDynamic(this, &ULoginWidget::HandleLoginRequestFailure);

    SetLoginButtonEnabled(false);
    ActiveLoginRequest->Activate();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 버튼 클릭 시 로그인 요청 함수를 호출하는 함수
void ULoginWidget::HandleLoginClicked()
{
    RequestLoginFromInput();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 요청 성공 결과를 처리하고 로그인 세션 후처리를 실행하는 함수
// UserIndex : 로그인에 성공한 유저의 DB user_Index
// Username : 로그인에 성공한 유저의 DB username
// Message : 로그인 서버가 전달한 성공 메시지
// LoginToken : 로그인 서버에서 발급받은 세션 토큰
void ULoginWidget::HandleLoginRequestSuccess(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken)
{
    FinishLoginRequest();

    UGameInstance* GameInstance = GetGameInstance();
    ULoginFlowSubsystem* LoginFlowSubsystem = GameInstance
        ? GameInstance->GetSubsystem<ULoginFlowSubsystem>()
        : nullptr;
    if (LoginFlowSubsystem == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoginWidget cannot save login state because LoginFlowSubsystem is missing."));
        ShowLoginMessage(LoginStateSaveFailedMessage, false);
        return;
    }

    LoginFlowSubsystem->HandleLoginSuccess(UserIndex, Username, LoginToken);
    ShowLoginMessage(SuccessLoginMessage, true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 요청 실패 결과를 처리하고 실패 메시지를 표시하는 함수
// UserIndex : 실패 시 사용하지 않는 값
// Username : 실패 시 사용하지 않는 값
// Message : 로그인 실패 이유 메시지
// LoginToken : 실패 시 사용하지 않는 값
void ULoginWidget::HandleLoginRequestFailure(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken)
{
    FinishLoginRequest();

    const FText MessageText = Message.IsEmpty()
        ? FText::FromString(TEXT("로그인에 실패했습니다."))
        : FText::FromString(Message);
    ShowLoginMessage(MessageText, false);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 결과 메시지 위젯을 생성하고 화면에 표시하는 함수
// MessageText : 메시지 위젯에 표시할 문구
// bIsSuccess : 성공 메시지 여부
void ULoginWidget::ShowLoginMessage(const FText& MessageText, bool bIsSuccess)
{
    if (LoginMessageWidgetClass == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoginWidget message class is not set."));
        return;
    }

    UUserWidget* MessageWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), LoginMessageWidgetClass);
    if (MessageWidget == nullptr)
    {
        MessageWidget = CreateWidget<UUserWidget>(GetWorld(), LoginMessageWidgetClass);
    }

    if (MessageWidget == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("LoginWidget failed to create login message widget."));
        return;
    }

    InitializeLoginMessageWidget(MessageWidget, MessageText, bIsSuccess);
    MessageWidget->AddToViewport();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 생성된 로그인 메시지 위젯의 InitMessage 함수를 호출하는 함수
// MessageWidget : 초기화할 로그인 메시지 위젯
// MessageText : 메시지 위젯에 표시할 문구
// bIsSuccess : 성공 메시지 여부
void ULoginWidget::InitializeLoginMessageWidget(UUserWidget* MessageWidget, const FText& MessageText, bool bIsSuccess) const
{
    if (MessageWidget == nullptr || LoginMessageInitFunctionName.IsNone())
    {
        return;
    }

    UFunction* InitMessageFunction = MessageWidget->FindFunction(LoginMessageInitFunctionName);
    if (InitMessageFunction == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Login message widget does not have function: %s"), *LoginMessageInitFunctionName.ToString());
        return;
    }

    struct FLoginMessageInitParams
    {
        FText MessageText;
        bool IsSuccess = false;
    };

    FLoginMessageInitParams Params;
    Params.MessageText = MessageText;
    Params.IsSuccess = bIsSuccess;
    MessageWidget->ProcessEvent(InitMessageFunction, &Params);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 요청 완료 후 델리게이트 연결과 버튼 상태를 정리하는 함수
void ULoginWidget::FinishLoginRequest()
{
    if (ActiveLoginRequest)
    {
        ActiveLoginRequest->OnSuccess.RemoveDynamic(this, &ULoginWidget::HandleLoginRequestSuccess);
        ActiveLoginRequest->OnFailure.RemoveDynamic(this, &ULoginWidget::HandleLoginRequestFailure);
        ActiveLoginRequest = nullptr;
    }

    SetLoginButtonEnabled(true);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로그인 요청 중복 실행을 막기 위해 로그인 버튼 활성 상태를 변경하는 함수
// bShouldEnable : 로그인 버튼 활성화 여부
void ULoginWidget::SetLoginButtonEnabled(bool bShouldEnable) const
{
    if (BTN_Login)
    {
        BTN_Login->SetIsEnabled(bShouldEnable);
    }
}
