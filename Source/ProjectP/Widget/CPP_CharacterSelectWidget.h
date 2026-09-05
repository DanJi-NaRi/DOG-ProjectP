#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CPP_CharacterSelectWidget.generated.h"

class UButton;
class UCPP_LobbyPartyPanel;
class UTextBlock;

UCLASS()
class PROJECTP_API UCPP_CharacterSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Lobby|Character")
    void SetupCharacterSelect(UCPP_LobbyPartyPanel* InLobbyPartyPanel, const TArray<int32>& InConfirmedCharacterIds, int32 InCurrentSelectedCharacterId);

    UFUNCTION(BlueprintPure, Category = "Lobby|Character")
    bool IsCharacterConfirmed(int32 CharacterId) const;

    UFUNCTION(BlueprintPure, Category = "Lobby|Character")
    bool IsCurrentSelectedCharacter(int32 CharacterId) const;

protected:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Character")
    void OnCharacterSelectStateChanged(const TArray<int32>& ConfirmedCharacterIds, int32 CurrentSelectedCharacterId);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character|Text")
    FText ReadyActionText = FText::FromString(TEXT("Ready"));

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Character|Text")
    FText CancelReadyActionText = FText::FromString(TEXT("Cancel Ready"));

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Character100;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Character200;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Character300;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Close;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_Ready;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Ready;

    UPROPERTY(Transient)
    TObjectPtr<UCPP_LobbyPartyPanel> LobbyPartyPanel;

    UPROPERTY(Transient)
    TArray<int32> CachedConfirmedCharacterIds;

    int32 CachedCurrentSelectedCharacterId = -1;
    bool bHasCharacterSelectState = false;

    UFUNCTION()
    void HandleCharacter100Clicked();

    UFUNCTION()
    void HandleCharacter200Clicked();

    UFUNCTION()
    void HandleCharacter300Clicked();

    UFUNCTION()
    void HandleCloseClicked();

    UFUNCTION()
    void HandleReadyClicked();

    void BindCharacterButtons();
    void HandleCharacterButtonClicked(int32 CharacterId);
    void UpdateCharacterButtonStates();
    void UpdateReadyButtonState();
    bool IsValidCharacterId(int32 CharacterId) const;
};
