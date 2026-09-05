#pragma once

#include "CoreMinimal.h"
#include "MyActivatableWidget.h"
#include "CPP_EntryCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로비 입장 직후 강제로 표시되는 캐릭터 선택 전용 위젯
// 캐릭터 3개 버튼과 선택 완료 버튼만 제공하고 닫기 버튼은 없다.
// 로비 입장 시에는 관전자 상태이며, 캐릭터를 선택해야 서버가 로비 캐릭터를 스폰한다.
// 위젯이 떠 있는 동안에는 관전자 이동 입력도 차단하고, 선택 완료 시에만 닫히며 입력을 복구한다.
UCLASS()
class PROJECTP_API UCPP_EntryCharacterSelectWidget : public UMyActivatableWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Lobby|Character")
    int32 GetSelectedCharacterId() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Character")
    void OnEntryCharacterSelectionChanged(int32 NewSelectedCharacterId);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character|Text")
    FText ConfirmActionText = FText::FromString(TEXT("선택 완료"));

private:
    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Character", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UButton> BTN_Character100;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Character", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UButton> BTN_Character200;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Character", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UButton> BTN_Character300;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Character", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UButton> BTN_Confirm;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby|Character", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UTextBlock> TXT_Confirm;

    int32 SelectedCharacterId = -1;
    bool bLobbyMoveInputBlocked = false;

    UFUNCTION()
    void HandleCharacter100Clicked();

    UFUNCTION()
    void HandleCharacter200Clicked();

    UFUNCTION()
    void HandleCharacter300Clicked();

    UFUNCTION()
    void HandleConfirmClicked();

    void BindEntryButtons();
    void HandleCharacterButtonClicked(int32 CharacterId);
    void UpdateConfirmButtonState();
    void SetLobbyMoveInputBlocked(bool bShouldBlock);
    bool IsValidCharacterId(int32 CharacterId) const;
};
