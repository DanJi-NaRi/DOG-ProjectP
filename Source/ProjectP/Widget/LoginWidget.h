#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class ULoginRequestAsyncAction;

UCLASS(BlueprintType, Blueprintable)
class PROJECTP_API ULoginWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Login")
    void RequestLoginFromInput();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Request")
    FString LoginRequestServerUrl;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Message")
    TSubclassOf<UUserWidget> LoginMessageWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Message")
    FName LoginMessageInitFunctionName = TEXT("InitMessage");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Message")
    FText EmptyInputMessage = FText::FromString(TEXT("ID 또는 PW를 입력해주세요."));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Message")
    FText SuccessLoginMessage = FText::FromString(TEXT("로그인 성공"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Login|Message")
    FText LoginStateSaveFailedMessage = FText::FromString(TEXT("로그인 상태 저장에 실패했습니다."));

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> ETB_ID;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UEditableTextBox> ETB_Password;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Login;

    UPROPERTY(Transient)
    TObjectPtr<ULoginRequestAsyncAction> ActiveLoginRequest;

    UFUNCTION()
    void HandleLoginClicked();

    UFUNCTION()
    void HandleLoginRequestSuccess(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken);

    UFUNCTION()
    void HandleLoginRequestFailure(int32 UserIndex, const FString& Username, const FString& Message, const FString& LoginToken);

    void ShowLoginMessage(const FText& MessageText, bool bIsSuccess);
    void InitializeLoginMessageWidget(UUserWidget* MessageWidget, const FText& MessageText, bool bIsSuccess) const;
    void FinishLoginRequest();
    void SetLoginButtonEnabled(bool bShouldEnable) const;
};
