////////////////////////////
//! \page MyItemSlotPanelWidget.h
//! \brief HUD 하단의 아이템 퀵슬롯(1~4) 패널 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "MyItemSlotPanelWidget.generated.h"

class UAbilitySystemComponent;
class UMyInventoryComponent;
class UMySkillSlotWidget;

////////////////////////////
//! \class UMyItemSlotPanelWidget
//! \brief 로컬 플레이어의 인벤토리 컴포넌트를 구독해 퀵슬롯 등록 아이템의 아이콘/보유 개수를 표시한다.
//!        인벤토리 슬롯에서 드래그한 아이템을 드랍 받아 해당 칸에 등록한다.
//! \note WBP에는 UMySkillSlotWidget 기반 슬롯을 Slot_1 ~ Slot_4 이름으로 배치한다.
//!       퀵슬롯은 로컬 키셋팅이므로 서버와 무관하며, 사용 요청만 Server RPC로 검증된다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyItemSlotPanelWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! 모든 퀵슬롯 표시를 다시 그린다.
	UFUNCTION(BlueprintCallable, Category = "UI|ItemSlotPanel")
	void RefreshAllSlots();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleQuickSlotChanged(int32 SlotIndex, FName ItemId);

	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandleItemDropped(UMySkillSlotWidget* SlotWidget, FName ItemId, int32 SourceQuickSlotIndex);

    UFUNCTION()
    void HandleItemDroppedOutside(UMySkillSlotWidget* SlotWidget, FName ItemId);

	//! 소유 플레이어 PlayerState의 인벤토리 컴포넌트를 찾아 델리게이트를 구독한다.
	bool BindToInventoryComponent();
	void UnbindFromInventoryComponent();
	void ScheduleBindRetry();
	void HandleBindRetry();

	//! 슬롯 위젯들의 드랍 수신/델리게이트를 초기화한다.
	void InitializeSlots();
	void RefreshSlot(int32 SlotIndex);
	UMySkillSlotWidget* GetSlotWidget(int32 SlotIndex) const;
	int32 FindSlotIndex(const UMySkillSlotWidget* SlotWidget) const;

	//~ 쿨타임 표시: 등록 아이템들의 쿨타임 태그를 ASC에서 구독해 슬롯 쿨다운 UI를 구동한다
	void RebindCooldownTags();
	void UnbindCooldownTags();
	void HandleCooldownTagChanged(const FGameplayTag CooldownTag, int32 NewCount);
	//~end of 쿨타임 표시

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UMySkillSlotWidget> Slot_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UMySkillSlotWidget> Slot_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UMySkillSlotWidget> Slot_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UMySkillSlotWidget> Slot_4;

	UPROPERTY(Transient)
	TObjectPtr<UMyInventoryComponent> BoundInventoryComponent;

	//! 쿨타임 태그 이벤트 구독 대상 ASC (PlayerState 소유)
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	//! 슬롯별 등록 아이템의 쿨타임 태그/전체 쿨타임. 태그 이벤트 수신 시 슬롯을 찾는 데 사용한다.
	TArray<FGameplayTag> SlotCooldownTags;
	TArray<float> SlotCooldownDurations;

	//! 구독 중인 쿨타임 태그별 델리게이트 핸들
	TMap<FGameplayTag, FDelegateHandle> CooldownTagDelegateHandles;

	//! PlayerState 복제 지연 대비 구독 재시도 타이머
	FTimerHandle BindRetryTimerHandle;
	int32 BindRetryCount = 0;
};
