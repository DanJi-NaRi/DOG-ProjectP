////////////////////////////
//! \page MyInventoryWidget.h
//! \brief 보유 아이템 목록, 메소, 정렬, Hover 툴팁과 더블클릭 사용을 제공하는 인벤토리 창 위젯 선언 파일이다.
#pragma once

#include "GameplayTagContainer.h"
#include "Item/MyItemTypes.h"
#include "Widget/MyActivatableWidget.h"
#include "MyInventoryWidget.generated.h"

class UAbilitySystemComponent;
class UButton;
class UMyInventoryComponent;
class UMyInventorySlotWidget;
class UMyItemTooltipWidget;
class UTextBlock;
class UUniformGridPanel;
class UWidget;
class UWidgetAnimation;

//! \enum EMyInventorySortMode 인벤토리에 한 번 적용할 정렬 기준
UENUM(BlueprintType)
enum class EMyInventorySortMode : uint8
{
	ByCount,       //! 개수 많은 순
	ByAcquisition, //! 획득 순서
	ByName,        //! 이름 가나다순
	ByType         //! 아이템 타입별
};

////////////////////////////
//! \class UMyInventoryWidget
//! \brief Menu 레이어에 푸시되는 인벤토리 창. PlayerState의 인벤토리 컴포넌트를 구독해 표시를 갱신한다.
//!        슬롯 Hover 동안 툴팁을 표시하고, 사용 가능 아이템을 더블클릭하면 사용을 요청한다.
//! \note BP 디폴트에서 InputMode=GameAndMenu(또는 Menu)와 SlotWidgetClass를 지정해야 한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyInventoryWidget : public UMyActivatableWidget
{
	GENERATED_BODY()

public:
	UMyInventoryWidget();

	//! 표시 내용을 전부 다시 그린다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void RefreshInventory();

	//! 닫기 슬라이드 애니메이션을 재생한 뒤 인벤토리를 비활성화한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Inventory")
	void RequestCloseInventory();

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//! 아이템 슬롯으로 생성할 위젯 클래스 (BP 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory")
	TSubclassOf<UMyInventorySlotWidget> SlotWidgetClass;

	//! 아이템 그리드 열 개수 (고정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory", meta = (ClampMin = "1"))
	int32 NumGridColumns = 5;

	//! 항상 표시할 최소 행 개수. 아이템이 이보다 적어도 빈 슬롯으로 채운다. 넘치는 행은 UMG의 ScrollBox가 스크롤 처리.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Inventory", meta = (ClampMin = "1"))
	int32 MinGridRows = 6;

private:
	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandleMesoChanged(int32 NewMeso);

	UFUNCTION()
	void HandleSlotHovered(FName ItemId);

	UFUNCTION()
	void HandleSlotUnhovered(FName ItemId);

	UFUNCTION()
	void HandleSlotDoubleClicked(FName ItemId);

	UFUNCTION()
	void HandleSortClicked();

	UFUNCTION()
	void HandleSortByCountClicked();

	UFUNCTION()
	void HandleSortByAcquisitionClicked();

	UFUNCTION()
	void HandleSortByNameClicked();

	UFUNCTION()
	void HandleSortByTypeClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleSlideAnimationFinished();

	//! 소유 플레이어의 PlayerState에서 인벤토리 컴포넌트를 찾아 델리게이트를 구독한다.
	bool BindToInventoryComponent();
	void UnbindFromInventoryComponent();

	//! 현재 표시 중인 아이템 쿨타임 태그를 ASC에 구독한다.
	void RebindCooldownTags();
	void UnbindCooldownTags();
	void HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount);

	//! 저장된 표시 순서를 현재 인벤토리 항목과 동기화한다.
	void SynchronizeDisplayOrder();

	//! 저장된 표시 순서에 맞는 인벤토리 항목 목록을 만든다.
	void BuildDisplayEntries(TArray<FMyInventoryEntry>& OutEntries) const;

	//! 지정한 조건으로 현재 표시 순서를 한 번만 정렬한다.
	void ApplySortOnce(EMyInventorySortMode SortMode);

	//! 정렬 방식 선택 패널의 표시 여부를 변경한다.
	void SetSortOptionsVisible(bool bVisible);

	void RefreshMesoText();

	//! Hover 아이템을 갱신하고 고정 위치 툴팁 상태를 반영한다.
	void ShowHoveredItem(FName ItemId);
	void HideTooltip();

	//! 더블클릭과 드래그앤드롭으로 대체된 기존 사용/등록 버튼을 숨긴다.
	void HideLegacyActionButtons();

	//! 지정 아이템이 현재 더블클릭으로 사용 가능한지 확인한다.
	bool IsItemUsable(FName ItemId) const;

	float CalculateSlideAnimationPlaybackSpeed() const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UUniformGridPanel> GRD_ItemBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Meso;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Use;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_RegisterKey;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Sort;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UWidget> PNL_SortOptions;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_SortByCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_SortByAcquisition;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_SortByName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_SortByType;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UMyItemTooltipWidget> WBP_Tooltip;

	UPROPERTY(Transient)
	TObjectPtr<UMyInventoryComponent> BoundInventoryComponent;

	//! 아이템 쿨타임 태그를 복제받는 PlayerState의 ASC
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	//! 현재 인벤토리 그리드에 표시 중인 아이템 슬롯
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMyInventorySlotWidget>> ActiveItemSlotWidgets;

	//! 구독 중인 쿨타임 태그별 델리게이트 핸들
	TMap<FGameplayTag, FDelegateHandle> CooldownTagDelegateHandles;

	//! 슬롯 Hover로 가리킨 아이템. 툴팁과 더블클릭 사용의 대상이다.
	FName HoveredItemId = NAME_None;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetAnimation> InventorySlideAnimation;

	//! 키셋팅 등록 버튼 대신 사용하는 인벤토리 슬라이드 애니메이션의 목표 재생 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Inventory|Animation",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01"))
	float SlideAnimationDuration = 0.25f;

	bool bIsClosing = false;
};
