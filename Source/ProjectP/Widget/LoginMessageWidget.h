#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoginMessageWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class PROJECTP_API ULoginMessageWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Login|Message")
    void InitMessage(const FText& MessageText, bool IsSuccess);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_OK;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Message;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> Text_OK;

    bool bIsSuccess = false;

    UFUNCTION()
    void HandleOkClicked();

    void RequestLobbyTravelAfterLoginSuccess();
};
