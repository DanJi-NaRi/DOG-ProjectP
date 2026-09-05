////////////////////////////
//! \page MyInventorySlotWidget.h
//! \brief 인벤토리 창의 아이템 한 칸(아이콘 + 개수)을 표시하는 슬롯 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Item/MyItemTypes.h"
#include "TimerManager.h"
#include "MyInventorySlotWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventorySlotItemEventSignature, FName, ItemId);

////////////////////////////
//! \class UMyInventorySlotWidget
//! \brief 아이템 아이콘과 보유 개수를 표시하고, Hover와 더블클릭 시 아이템 ID를 브로드캐스트한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyInventorySlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! 슬롯에 표시할 아이템을 설정한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void SetItem(FName InItemId, const FMyItemData& InItemData, int32 InCount);

	//! 빈 슬롯 상태로 만든다. (아이콘/개수 숨김, 클릭해도 반응 없음)
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void SetEmpty();

	UFUNCTION(BlueprintPure, Category = "UI|Inventory")
	FName GetItemId() const { return ItemId; }

	//! 현재 슬롯 아이템의 쿨타임 태그를 반환한다.
	FGameplayTag GetCooldownTag() const { return CachedItemData.CooldownTag; }

	//! 현재 슬롯 아이템의 전체 쿨타임을 반환한다.
	float GetCooldownDuration() const { return CachedItemData.CooldownSeconds; }

	//! 지정한 남은 시간부터 쿨타임 표시를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void StartCooldownRemaining(float RemainingTime, float Duration);

	//! 진행 중인 쿨타임 표시를 종료하고 초기화한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void ClearCooldown();

	//! 슬롯 Hover 시작 알림 (인벤토리 창이 툴팁 표시에 사용)
	UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
	FInventorySlotItemEventSignature OnSlotHovered;

	//! 슬롯 Hover 종료 알림 (인벤토리 창이 툴팁 숨김에 사용)
	UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
	FInventorySlotItemEventSignature OnSlotUnhovered;

	//! 슬롯 더블클릭 알림 (인벤토리 창이 사용 가능 아이템의 사용에 사용)
	UPROPERTY(BlueprintAssignable, Category = "UI|Inventory")
	FInventorySlotItemEventSignature OnSlotDoubleClicked;

protected:
    virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//~ 드래그앤드랍 (인벤토리 → HUD 퀵슬롯 등록)
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	//~end of 드래그앤드랍

private:
	void ApplyCooldownStyle();
	void ApplyCooldownDisplay(float RemainingTime, float Duration);
	void HandleCooldownTimerTick();

	UFUNCTION()
	void HandleSlotButtonClicked();

	UFUNCTION()
	void HandleSlotHovered();

	UFUNCTION()
	void HandleSlotUnhovered();

	//! 드래그가 어떤 대상에도 드랍되지 못했을 때 호출된다. (드랍 실패 진단용)
	UFUNCTION()
	void HandleDragCancelled(UDragDropOperation* Operation);

	//! 드래그 중 커서를 따라다니는 아이콘 이미지의 크기(px). WBP 디폴트에서 조절한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory|DragDrop", meta = (AllowPrivateAccess))
	FVector2D DragVisualSize = FVector2D(80.0f, 80.0f);

    //! 인벤토리 격자 아이콘의 중앙 기준 표시 배율
    UPROPERTY(EditAnywhere, Category = "UI|Inventory|Style", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "1.0", AllowPrivateAccess))
    float InventoryItemIconScale = 0.75f;

	//! 쿨타임 표시를 갱신하는 주기
	UPROPERTY(EditAnywhere, Category = "UI|Inventory|Cooldown", meta = (ClampMin = "0.01", AllowPrivateAccess))
	float CooldownTickInterval = 0.05f;

	//! 쿨타임 오버레이 색상과 투명도
	UPROPERTY(EditAnywhere, Category = "UI|Inventory|Cooldown|Style", meta = (AllowPrivateAccess))
	FLinearColor CooldownOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	//! 쿨타임 남은 시간 텍스트의 폰트. 지정하지 않으면 WBP 설정을 유지한다.
	UPROPERTY(EditAnywhere, Category = "UI|Inventory|Cooldown|Style", meta = (AllowPrivateAccess))
	FSlateFontInfo CooldownFont;

	//! 쿨타임 남은 시간 텍스트의 색상
	UPROPERTY(EditAnywhere, Category = "UI|Inventory|Cooldown|Style", meta = (AllowPrivateAccess))
	FLinearColor CooldownTextColor = FLinearColor::White;

	//! 쿨타임 머티리얼에 남은 비율을 전달할 스칼라 파라미터 이름
	UPROPERTY(EditAnywhere, Category = "UI|Inventory|Cooldown|Style", meta = (AllowPrivateAccess))
	FName CooldownProgressParamName = TEXT("Progress");

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Slot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_IconBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Count;

	//! 쿨타임 진행 이미지를 표시한다. WBP에 없으면 쿨타임 로직만 동작한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_Cooldown;

	//! 쿨타임 남은 초를 표시한다. WBP에 없으면 쿨타임 로직만 동작한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Cooldown;

	FName ItemId = NAME_None;

	//! 드래그 비주얼 생성에 사용하는 아이템 정적 데이터 사본. SetItem에서 채워진다.
	FMyItemData CachedItemData;

	float CooldownDuration = 0.0f;
	float CooldownEndWorldTime = 0.0f;
	FTimerHandle CooldownTimerHandle;

	bool bIsHovered = false;
};
