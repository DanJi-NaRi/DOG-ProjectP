#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DungeonMainUI.generated.h"

class UButton;
class UTextBlock;
class UCPP_LogoutRequestAsyncAction;
class UDungeonSettingsModal;
class UMyInventoryComponent;

UCLASS()
class PROJECTP_API UDungeonMainUI : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Login")
    void RequestLogoutFromDungeon();

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Character")
    void SetSelectedCharacterIdText(int32 SelectedCharacterId);

    void OpenSettingsModal();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SelectedCharacterId;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Meso;

    //! 설정 버튼 클릭 시 Modal 레이어에 띄울 설정 모달 위젯 클래스 (WBP_DungeonSettingsModal 지정)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Settings")
    TSubclassOf<UDungeonSettingsModal> SettingsModalClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Login")
    FName LoginMapName = TEXT("Map_Login");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Login")
    FString LogoutRequestServerUrl;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Login")
    bool bReturnToLoginOnLogoutFailure = true;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Settings;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Inventory;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_God;

    TWeakObjectPtr<UDungeonSettingsModal> ActiveSettingsModal;

    UPROPERTY(Transient)
    TObjectPtr<UCPP_LogoutRequestAsyncAction> ActiveLogoutRequest;

    UPROPERTY(Transient)
    TObjectPtr<UMyInventoryComponent> BoundInventoryComponent;

    int32 CachedSelectedCharacterId = INDEX_NONE;

    UFUNCTION()
    void HandleSettingsClicked();

    UFUNCTION()
    void HandleInventoryClicked();

    UFUNCTION()
    void HandleGodClicked();

    UFUNCTION()
    void HandleLogoutRequestSuccess(const FString& Message);

    UFUNCTION()
    void HandleLogoutRequestFailure(const FString& Message);

    UFUNCTION()
    void HandleMesoChanged(int32 NewMeso);

    void EnsureInventoryBinding();
    void FinishLogoutAndOpenLoginMap();
    void FinishLogoutRequest();
    void SetExitButtonEnabled(bool bShouldEnable) const;
    void RefreshSelectedCharacterIdText();
};
