#pragma once

#include "CommonUserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "GAS/SkillData/MySkillSetDataAsset.h"
#include "GameplayTagContainer.h"
#include "MySkillSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;
class UMySkillSlotWidget;

//! 아이템 드래그앤드랍이 이 슬롯 위에서 드랍됐을 때 알림 (퀵슬롯 등록용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSkillSlotItemDroppedSignature, UMySkillSlotWidget*, SlotWidget, FName, ItemId, int32, SourceQuickSlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSkillSlotItemDroppedOutsideSignature, UMySkillSlotWidget*, SlotWidget, FName, ItemId);

UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMySkillSlotWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetSkillData(const FMySkillDataEntry& SkillData);

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void ClearSkillData();

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetInputKeyText(FText InInputKeyText);

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetSkillIcon(UTexture2D* InIcon);

    //! 아이템 퀵슬롯의 아이템 유무에 맞춰 IMG_Frame을 Item_ON/Item_OFF로 전환한다.
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetItemFrameActive(bool bHasItem);

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void StartCooldown(float Duration);

    //! 남은 시간부터 진행되는 쿨다운을 시작한다. (진행 중 쿨타임의 중간 표시용)
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void StartCooldownRemaining(float RemainingTime, float Duration);

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetCooldown(float RemainingTime, float Duration);

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void ClearCooldown();

    //! 강제 쿨타임 초기화 시 에디터에서 구성한 블루프린트 효과를 재생한다.
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void PlayCooldownResetEffect();

    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetLocked(bool bInLocked);

    //! 보유 개수 텍스트(TXT_Count)를 설정한다. 빈 텍스트면 숨긴다. (아이템 퀵슬롯 표시용)
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetCountText(FText InCountText);

    //! 아이템 드래그앤드랍 수신 허용 여부를 설정한다. (아이템 퀵슬롯 패널이 켠다)
    UFUNCTION(BlueprintCallable, Category = "UI|SkillSlot")
    void SetAcceptsItemDrop(bool bInAcceptsItemDrop);

    //! 퀵슬롯 드래그에 필요한 슬롯 인덱스와 현재 아이템 ID를 설정한다.
    void SetQuickSlotContext(int32 InQuickSlotIndex, FName InItemId);

    FGameplayTag GetInputTag() const { return InputTag; }
    FGameplayTag GetCooldownTag() const { return CooldownTag; }
    float GetCooldownDuration() const { return CooldownDuration; }

    //! 아이템 드랍 알림. bAcceptsItemDrop이 켜진 슬롯에서만 발생한다.
    UPROPERTY(BlueprintAssignable, Category = "UI|SkillSlot")
    FSkillSlotItemDroppedSignature OnItemDropped;

    //! 퀵슬롯 아이템이 다른 슬롯이 아닌 영역에 드롭됐을 때 발생한다.
    UPROPERTY(BlueprintAssignable, Category = "UI|SkillSlot")
    FSkillSlotItemDroppedOutsideSignature OnItemDroppedOutside;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;

    //! WBP_SkillSlot에서 원하는 쿨타임 초기화 연출을 구현하는 이벤트.
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|SkillSlot")
    void BP_OnCooldownResetEffect();

    //~ 아이템 드래그앤드랍 수신 (HUD 퀵슬롯 등록)
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    //~end of 아이템 드래그앤드랍 수신

private:
    void ApplyDisplay();
    void ApplyCooldownStyle();
    void ApplyCooldownDisplay(float RemainingTime, float Duration);
    void ApplyItemIconScale();
    void HandleCooldownTimerTick();
    void ShowItemDropPreview(UTexture2D* InIcon);
    void ClearItemDropPreview();

    UFUNCTION()
    void HandleItemDragCancelled(UDragDropOperation* Operation);

private:
    //! 디자이너 미리보기용 키 텍스트. 런타임에 SetInputKeyText가 따로 호출되지 않으면(아이템 퀵슬롯 등) 이 값이 그대로 기본 키 텍스트로 쓰인다.
    UPROPERTY(EditAnywhere, Category = "Preview")
    FText PreviewInputKeyText;

    UPROPERTY(EditAnywhere, Category = "Preview")
    FText PreviewSkillName;

    UPROPERTY(EditAnywhere, Category = "Preview")
    TObjectPtr<UTexture2D> PreviewIcon;

    UPROPERTY(EditAnywhere, Category = "Preview")
    bool bPreviewLocked = false;

    UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (ClampMin = "0.01"))
    float CooldownTickInterval = 0.05f;

    //! 쿨다운 오버레이(IMG_Cooldown) 색상. 배경 어둡기를 WBP 디폴트나 배치 인스턴스별로 조절한다.
    UPROPERTY(EditAnywhere, Category = "Cooldown|Style")
    FLinearColor CooldownOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

    //! 쿨다운 남은 초(TXT_Cooldown) 폰트. Font Family를 지정한 경우에만 적용한다. (미지정 시 WBP 설정 유지)
    UPROPERTY(EditAnywhere, Category = "Cooldown|Style")
    FSlateFontInfo CooldownFont;

    //! 쿨다운 남은 초 텍스트 색상
    UPROPERTY(EditAnywhere, Category = "Cooldown|Style")
    FLinearColor CooldownTextColor = FLinearColor::White;

    //! IMG_Cooldown 브러시에 머티리얼을 지정한 경우, 진행도(남은시간/전체 0~1)를 전달할 스칼라 파라미터 이름.
    //! 머티리얼이면 라디얼(시계방향) 표현, 텍스처면 기존 세로 스케일 표현으로 동작한다.
    UPROPERTY(EditAnywhere, Category = "Cooldown|Style")
    FName CooldownProgressParamName = TEXT("Progress");

    //! 아이템이 등록된 퀵슬롯에 표시할 프레임
    UPROPERTY(EditDefaultsOnly, Category = "Item Slot|Style", meta = (AllowPrivateAccess))
    TSoftObjectPtr<UTexture2D> ItemOnFrameTexture = TSoftObjectPtr<UTexture2D>(
        FSoftObjectPath(TEXT("/Game/Assets/UI/Dungeon/HUD/Item_Bar/Item_ON.Item_ON")));

    //! 비어 있는 퀵슬롯에 표시할 프레임
    UPROPERTY(EditDefaultsOnly, Category = "Item Slot|Style", meta = (AllowPrivateAccess))
    TSoftObjectPtr<UTexture2D> ItemOffFrameTexture = TSoftObjectPtr<UTexture2D>(
        FSoftObjectPath(TEXT("/Game/Assets/UI/Dungeon/HUD/Item_Bar/Item_OFF.Item_OFF")));

    //! 아이템 퀵슬롯 아이콘의 중앙 기준 표시 배율
    UPROPERTY(EditAnywhere, Category = "Item Slot|Style", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "1.0", AllowPrivateAccess))
    float QuickSlotItemIconScale = 0.75f;

    //! 빈 퀵슬롯에 표시하는 드롭 미리보기 아이콘의 불투명도
    UPROPERTY(EditAnywhere, Category = "Item Slot|DragDrop", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", AllowPrivateAccess))
    float ItemDropPreviewOpacity = 0.35f;

    //! 퀵슬롯에서 드래그할 때 커서를 따라다니는 아이콘 크기
    UPROPERTY(EditAnywhere, Category = "Item Slot|DragDrop", meta = (AllowPrivateAccess))
    FVector2D ItemDragVisualSize = FVector2D(50.0f, 50.0f);

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_Frame;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_Icon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_Cooldown;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UImage> IMG_Locked;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UTextBlock> TXT_Cooldown;

    //! 보유 개수 표시 (아이템 퀵슬롯 전용, 스킬 슬롯 WBP에는 없어도 된다)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UTextBlock> TXT_Count;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UTextBlock> TXT_Key;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
    TObjectPtr<UTextBlock> TXT_Name;

    UPROPERTY(Transient)
    FGameplayTag InputTag;

    UPROPERTY(Transient)
    FGameplayTag CooldownTag;

    UPROPERTY(Transient)
    FText InputKeyText;

    UPROPERTY(Transient)
    FText SkillName;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> SkillIcon;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ItemDropPreviewIcon;

    UPROPERTY(Transient)
    FName QuickSlotItemId = NAME_None;

    float CooldownDuration = 0.0f;
    float CooldownEndWorldTime = 0.0f;
    bool bLocked = false;
    bool bShowingItemDropPreview = false;
    int32 QuickSlotIndex = INDEX_NONE;

    //! 아이템 드랍 수신 허용 여부. 아이템 퀵슬롯 패널이 초기화 시 켠다.
    bool bAcceptsItemDrop = false;

    FTimerHandle CooldownTimerHandle;
};
