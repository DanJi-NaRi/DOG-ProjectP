#pragma once

#include "CommonUserWidget.h"
#include "TimerManager.h"
#include "MyTeamStatusPanelWidget.generated.h"

class AMyPlayerState;
class UMyTeamMemberStatusWidget;

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyTeamStatusPanelWidget : public UCommonUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    bool TryInitializeCharacterSlots();
    void RefreshTeamMembers();
    AMyPlayerState* FindActivePlayerStateByCharacterId(int32 CharacterId) const;
    void RefreshCharacterSlot(
        UMyTeamMemberStatusWidget* TeamMemberSlot,
        int32 CharacterId,
        bool& bWasConnected);

private:
    UPROPERTY(EditDefaultsOnly, Category = "UI|TeamStatus", meta = (ClampMin = "0.1"))
    float PlayerStateRefreshInterval = 0.25f;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyTeamMemberStatusWidget> TeamMemberSlot1;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
    TObjectPtr<UMyTeamMemberStatusWidget> TeamMemberSlot2;

    int32 FirstSlotCharacterId = -1;
    int32 SecondSlotCharacterId = -1;
    bool bCharacterSlotsInitialized = false;
    bool bFirstSlotWasConnected = false;
    bool bSecondSlotWasConnected = false;
    FTimerHandle PlayerStateRefreshTimerHandle;
};
