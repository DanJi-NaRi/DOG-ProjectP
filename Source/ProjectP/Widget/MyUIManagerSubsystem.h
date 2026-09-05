#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "Components/SlateWrapperTypes.h"
#include "MyNoticeTypes.h"
#include "MyUIManagerSubsystem.generated.h"

class APlayerController;
class UMyActivatableWidget;
class UMyNoticeWidget;
class UMyPrimaryGameLayout;
class UUserWidget;

UCLASS(config = Game)
class PROJECTP_API UMyUIManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    UMyPrimaryGameLayout* EnsurePrimaryLayout(APlayerController* OwningPlayer);

    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    UMyPrimaryGameLayout* EnsurePrimaryLayoutUsingClass(
        APlayerController* OwningPlayer,
        TSubclassOf<UMyPrimaryGameLayout> LayoutClass);

    UFUNCTION(BlueprintPure, Category = "UI|Layout")
    UMyPrimaryGameLayout* GetPrimaryLayout() const;

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    UMyNoticeWidget* EnsureNoticeWidget(
        APlayerController* OwningPlayer,
        TSubclassOf<UMyNoticeWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ShowNotice(const FText& Message, float DurationSeconds);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ShowNoticeData(const FMyNoticeData& NoticeData);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ShowCountdownNotice(const FText& MessageFormat, float EndServerTime);

    UFUNCTION(BlueprintCallable, Category = "UI|Notice")
    void ClearNotice();

    UFUNCTION(BlueprintCallable, Category = "UI|Layer", meta = (Categories = "UI.Layer"))
    UMyActivatableWidget* PushWidgetToLayerStack(
        FGameplayTag LayerTag,
        TSubclassOf<UMyActivatableWidget> WidgetClass);

    //~ 편의 함수
    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushLobby(TSubclassOf<UMyActivatableWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushHUD(TSubclassOf<UMyActivatableWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushMenu(TSubclassOf<UMyActivatableWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushModal(TSubclassOf<UMyActivatableWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushDialogue(TSubclassOf<UMyActivatableWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushToast(TSubclassOf<UMyActivatableWidget> WidgetClass);

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    UMyActivatableWidget* PushOverlay(TSubclassOf<UMyActivatableWidget> WidgetClass);
    //~end of 편의 함수

    UFUNCTION(BlueprintCallable, Category = "UI|Layer")
    void RemoveWidgetFromLayer(UMyActivatableWidget* ActivatableWidget);

    //! 레이어 스택의 모든 위젯을 닫는다. (대화 시작 시 Menu 레이어 정리용)
    UFUNCTION(BlueprintCallable, Category = "UI|Layer", meta = (Categories = "UI.Layer"))
    void ClearLayer(FGameplayTag LayerTag);

    //! 레이어 스택 자체를 숨기거나 되돌린다. 숨길 때 원래 Visibility를 저장했다가 복원한다.
    UFUNCTION(BlueprintCallable, Category = "UI|Layer", meta = (Categories = "UI.Layer"))
    void SetLayerVisible(FGameplayTag LayerTag, bool bVisible);

    //! 레이어 스택에서 현재 표시 중인 위젯의 CommonUI 활성 상태를 변경한다.
    void SetLayerActive(FGameplayTag LayerTag, bool bActive);

    //! 상주 위젯(투표 패널 등)을 생성해 레이어 스택들 위 최상단에 추가한다.
    //! 활성 상주 위젯을 레이어 스택에 넣으면 액티브 루트를 점유해 하위 레이어 입력 설정이 무시되므로 이 경로를 쓴다.
    UFUNCTION(BlueprintCallable, Category = "UI|Persistent")
    UUserWidget* AddPersistentWidget(TSubclassOf<UUserWidget> WidgetClass);

private:
    //! SetLayerVisible(false)가 저장한 레이어별 원래 Visibility
    TMap<FGameplayTag, ESlateVisibility> SavedLayerVisibilities;

    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSoftClassPtr<UMyPrimaryGameLayout> PrimaryLayoutClass;

    UPROPERTY(Transient)
    TObjectPtr<UMyPrimaryGameLayout> PrimaryLayout;

    UPROPERTY(Transient)
    TObjectPtr<UMyNoticeWidget> NoticeWidget;
};
