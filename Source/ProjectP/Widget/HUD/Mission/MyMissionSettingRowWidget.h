////////////////////////////
//! \page MyMissionSettingRowWidget.h
//! \brief Mission 설정 팝업의 개별 행 표시와 입력만 담당하는 위젯 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Widget/HUD/Mission/MyMissionDisplayTypes.h"
#include "MyMissionSettingRowWidget.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UImage;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionSettingRowClickedSignature, FGuid, MissionInstanceId);

////////////////////////////
//! \class UMyMissionSettingRowWidget
//! \author 장효제
//! \brief 상태를 소유하지 않고 표시와 클릭 전달만 하는 Mission 설정 팝업 행이다.
//!
//! 이 행은 DraftSelection을 소유하지 않고 DungeonPC 선택 API를 직접 호출하지 않으며
//! 최대 3개 선택 정책도 스스로 판단하지 않는다. 판단은 전부 부모 팝업이 한다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyMissionSettingRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	////////////////////////////
	//! \author 장효제
	//! \brief 일반 Mission과 중요 Mission을 같은 UI 전용 표시 데이터로 행에 적용한다.
	//! \param InDisplayData 표시할 UI 전용 Mission 데이터다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI")
	void SetMissionDisplayData(const FMyMissionDisplayData& InDisplayData);

	////////////////////////////
	//! \author 장효제
	//! \brief 부모 팝업이 확정한 선택 여부와 선택 가능 여부를 행 표현에 반영한다.
	//! \param bInSelected 현재 DraftSelection에 포함되어 있는지 여부다.
	//! \param bInSelectable 지금 이 행을 토글할 수 있는지 여부다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI")
	void SetSelectionState(bool bInSelected, bool bInSelectable);

	UFUNCTION(BlueprintPure, Category = "Mission|UI")
	FGuid GetMissionInstanceId() const;

	//! 행 전체 클릭과 CheckBox 클릭을 모두 같은 토글 요청으로 부모 팝업에 전달한다.
	FOnMissionSettingRowClickedSignature OnMissionSettingRowClicked;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//! 행 상태 색을 적용할 배경이며 배치하지 않으면 색 적용을 건너뛴다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BRD_RowBackground;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_GodIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_GodName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_DisplayName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Description;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_Progress;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_RemainingTime;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_MesoDelta;

	//! 파티 공통 Mission에만 표시할 배지 컨테이너다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PNL_PartyBadge;

	//! 중요 Mission 행에는 배치되어도 항상 숨긴다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> CB_Select;

	//! 행 전체를 덮는 클릭 영역이며 중요 Mission 행에서는 비활성화한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Row;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Mission|UI")
	FMyMissionDisplayData DisplayData;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Mission|UI")
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Mission|UI")
	bool bIsSelectable = false;

	//! 선택 가능하지만 아직 고르지 않은 일반 행 배경색이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style")
	FLinearColor NormalRowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	//! DraftSelection에 포함된 행 배경색이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style")
	FLinearColor SelectedRowColor = FLinearColor(0.10f, 0.80f, 0.75f, 0.55f);

	//! 3개를 이미 채워 더 고를 수 없는 행 배경색이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style")
	FLinearColor DisabledRowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);

	//! 최상단 고정 비선택 행 배경색이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style")
	FLinearColor ImportantRowColor = FLinearColor(1.0f, 0.25f, 0.08f, 0.55f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DisabledRowOpacity = 0.45f;

	////////////////////////////
	//! \author 장효제
	//! \brief 중요·선택·선택 가능 상태에 따른 색과 애니메이션 표현을 자식 WBP에 맡긴다.
	//! \param bInIsImportant 최상단 고정 비선택 행인지 여부다.
	//! \param bInIsSelected 현재 선택되어 있는지 여부다.
	//! \param bInIsSelectable 지금 토글할 수 있는지 여부다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Mission|UI", meta = (DisplayName = "On Mission Row State Applied"))
	void BP_OnMissionRowStateApplied(bool bInIsImportant, bool bInIsSelected, bool bInIsSelectable);

private:
	UFUNCTION()
	void HandleRowButtonClicked();

	UFUNCTION()
	void HandleSelectCheckBoxChanged(bool bChecked);

	void RequestToggle();
	void UpdateRemainingTimeText();
	void ApplyRowVisualState();

	//! SetSelectionState가 CheckBox를 갱신할 때 발생하는 되먹임 토글을 막는다.
	bool bApplyingSelectionState = false;
};
