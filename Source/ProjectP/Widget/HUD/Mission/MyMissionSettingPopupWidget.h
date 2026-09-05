////////////////////////////
//! \page MyMissionSettingPopupWidget.h
//! \brief HUD에 표시할 일반 Mission을 최대 3개 고르는 설정 팝업 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Widget/MyActivatableWidget.h"
#include "Widget/HUD/Mission/MyMissionDisplayTypes.h"
#include "MyMissionSettingPopupWidget.generated.h"

class ADungeonPC;
class UButton;
class UDataTable;
class UMyMissionSettingRowWidget;
class UScrollBox;
class UTextBlock;
class UWidget;
struct FMyMissionPublicView;

////////////////////////////
//! \class UMyMissionSettingPopupWidget
//! \author 장효제
//! \brief DraftSelection과 최대 3개 규칙을 소유하는 Mission 설정 팝업이다.
//!
//! 선택 상태는 클라이언트 로컬이며 서버 RPC를 보내지 않는다. Mission 진행 원본은
//! 서버 권위를 유지하고 팝업은 표시할 대상만 고른다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyMissionSettingPopupWidget : public UMyActivatableWidget
{
	GENERATED_BODY()

public:
	UMyMissionSettingPopupWidget();

	////////////////////////////
	//! \author 장효제
	//! \brief 실제 특수 이벤트 시스템 없이 중요 Mission 행 표현을 확인할 로컬 표시 데이터를 넣는다.
	//! \param InPreviewData 서버와 무관한 로컬 UI 표시 데이터다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI|Preview")
	void SetImportantMissionPreview(const FMyMissionDisplayData& InPreviewData);

	////////////////////////////
	//! \author 장효제
	//! \brief 중요 Mission 표시 데이터를 지우고 최상단 행을 제거한다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI|Preview")
	void ClearImportantMissionPreview();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//! 중요 Mission 행과 일반 Mission 행을 함께 담는 가변 목록이다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> SB_MissionRows;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_SelectionGuide;

	//! 활성 일반 Mission이 없을 때만 표시할 빈 상태 문구다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_EmptyMission;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Apply;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Cancel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Reset;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Close;

	//! 팝업 바깥 배경 클릭으로 취소할 때 사용할 전체 덮개 버튼이다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BTN_Background;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI", meta = (AllowAbstract = "false"))
	TSubclassOf<UMyMissionSettingRowWidget> MissionSettingRowWidgetClass;

	//! 중요 Mission 행에 사용할 별도 WBP 클래스이며 비우면 일반 행 클래스를 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI", meta = (AllowAbstract = "false"))
	TSubclassOf<UMyMissionSettingRowWidget> ImportantMissionSettingRowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyMissionDefinitionRow"))
	TObjectPtr<UDataTable> MissionDefinitionTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyGodPresentationRow"))
	TObjectPtr<UDataTable> GodPresentationTable;

	//! 에디터 디자인 뷰에서만 중요 Mission 행을 그려 본다. 런타임에는 사용하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Preview")
	bool bUseDesignTimeImportantPreview = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Preview", meta = (EditCondition = "bUseDesignTimeImportantPreview"))
	FMyMissionDisplayData DesignTimeImportantPreview;

private:
	UFUNCTION()
	void HandleMissionViewsChanged();

	UFUNCTION()
	void HandleMissionRowClicked(FGuid MissionInstanceId);

	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleResetClicked();

	void RebuildMissionRows();
	void UpdateSelectionGuide();
	void ToggleDraftSelection(FGuid MissionInstanceId);
	void PruneDraftSelection(const TArray<FMyMissionPublicView>& SelectableViews);
	TArray<FMyMissionPublicView> GetSelectableMissionViews() const;
	UMyMissionSettingRowWidget* CreateMissionRow(const FMyMissionDisplayData& RowDisplayData);

	TWeakObjectPtr<ADungeonPC> MissionPlayerController;
	TArray<FGuid> DraftSelection;
	FMyMissionDisplayData ImportantPreviewData;
	bool bHasImportantPreview = false;

	//! HUD 선택 상한이며 중요 Mission은 이 수에 포함하지 않는다.
	static constexpr int32 MaxHudMissionCount = 3;
};
