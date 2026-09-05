////////////////////////////
//! \page MyDungeonRevivePanelWidget.h
//! \brief 사망한 로컬 플레이어에게 부활 옵션 카드를 표시하는 패널 위젯 선언 파일이다.
#pragma once

#include "Widget/MyActivatableWidget.h"
#include "MyDungeonRevivePanelWidget.generated.h"

class UDungeonReviveDataAsset;
class UHorizontalBox;
class UMyDungeonReviveOptionWidget;
class UMyInventoryComponent;
class UProgressBar;
class UTextBlock;

////////////////////////////
//! \class UMyDungeonRevivePanelWidget
//! \author 장효제
//! \brief DungeonGS의 부활 데이터 에셋을 읽어 옵션 카드를 동적으로 만들고, 선택한 옵션을 ADungeonPC로 서버에 요청한다.
//! \note 로컬 플레이어가 사망하면 ADungeonPC가 Modal 레이어에 푸시하고, 부활하면 제거한다.
//!       임의로 닫을 수 없는 화면이므로 닫기 키와 CommonUI Back 핸들러를 쓰지 않는다.
//!       BP 디폴트에서 InputMode=Menu와 OptionWidgetClass를 지정해야 한다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyDungeonRevivePanelWidget : public UMyActivatableWidget
{
	GENERATED_BODY()

public:
	UMyDungeonRevivePanelWidget();

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//! 카드 클릭이 아닌 곳으로 포커스가 흘러가지 않도록 이 위젯 자신을 포커스 대상으로 지정한다.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	//! 부활 옵션 카드로 생성할 위젯 클래스 (BP 디폴트에서 WBP_DungeonReviveOption 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Revive")
	TSubclassOf<UMyDungeonReviveOptionWidget> OptionWidgetClass;

	//! 자동 선택 남은 시간 표시 형식. {0}에 남은 초가 들어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText SelectionTimeoutFormat = NSLOCTEXT("DungeonRevive", "Panel_SelectionTimeout", "{0}초 후 자동 선택");

	//! 서버에 부활을 요청한 뒤 표시할 문구. {0}에 부활까지 남은 초가 들어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText ReviveRequestedFormat = NSLOCTEXT("DungeonRevive", "Panel_ReviveRequested", "부활 중... {0}초");

	//! 표시할 부활 옵션이 하나도 없을 때의 안내 문구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText NoOptionNoticeText = NSLOCTEXT("DungeonRevive", "Panel_NoOption", "사용할 수 있는 부활 옵션이 없습니다.");

private:
	UFUNCTION()
	void HandleOptionSelected(FName OptionId);

	UFUNCTION()
	void HandleMesoChanged(int32 NewMeso);

	//! 소유 플레이어의 PlayerState에서 인벤토리 컴포넌트를 찾아 메소 변경을 구독한다.
	bool BindToInventoryComponent();
	void UnbindFromInventoryComponent();

	//! DungeonGS의 부활 데이터 에셋을 반환한다. 클라이언트에서도 클래스 디폴트로 유효하다.
	UDungeonReviveDataAsset* GetReviveData() const;

	//! 활성화된 옵션을 SortOrder 순으로 카드로 만들어 HB_ReviveOptions에 채운다.
	void BuildOptionCards();

	//! 현재 메소 보유량으로 각 카드의 구매 가능 상태를 갱신한다.
	void RefreshAffordability();

	//! 남은 선택 시간 표시를 갱신하고, 시간이 끝나면 자동 선택 옵션으로 부활을 요청한다.
	void UpdateSelectionCountdown();

	//! 선택 제한 시간이 끝났을 때 데이터에 지정된 자동 선택 옵션으로 부활을 요청한다.
	void RequestAutoSelectOption();

	void SetStatusText(const FText& StatusText);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UHorizontalBox> HB_ReviveOptions;

	//! 자동 선택 남은 시간과 부활 진행 상태를 함께 표시하는 텍스트
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UProgressBar> PB_SelectionTimeout;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMyDungeonReviveOptionWidget>> OptionWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UMyInventoryComponent> BoundInventoryComponent;

	//! 자동 선택이 실행되는 월드 시간. 제한 시간이 0 이하이면 사용하지 않는다.
	float SelectionDeadlineSeconds = 0.0f;

	bool bSelectionCountdownActive = false;

	//! 서버에 부활을 요청한 뒤의 부활 완료 예정 월드 시간. 표시용이다.
	float ReviveDisplayEndSeconds = 0.0f;

	bool bReviveRequested = false;
};
