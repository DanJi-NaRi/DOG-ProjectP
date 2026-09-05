////////////////////////////
//! \page MyMissionWidget.h
//! \brief 기존 WBP_Mission에 공개 Mission HUD 행을 자동으로 채우는 위젯 부모 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Streaming/MyMissionTypes.h"
#include "Widget/HUD/Mission/MyMissionDisplayTypes.h"
#include "MyMissionWidget.generated.h"

class ADungeonPC;
class UDataTable;
class UMyMissionRowWidget;
class UPanelWidget;
class UVerticalBox;

////////////////////////////
//! \class UMyMissionWidget
//! \author 장효제
//! \brief DungeonPC의 로컬 HUD 선택을 구독해 VB_MissionRows에 디자인 행 WBP를 자동 배치한다.
UCLASS(Blueprintable)
class PROJECTP_API UMyMissionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	////////////////////////////
	//! \author 장효제
	//! \brief 현재 DungeonPC 공개 View로 Mission HUD 행을 즉시 다시 만든다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI")
	void RefreshMissionRows();

	////////////////////////////
	//! \author 장효제
	//! \brief 실제 특수 이벤트 시스템 없이 중요 Mission 행 표현을 확인할 로컬 표시 데이터를 넣는다.
	//! \param InPreviewData 서버와 무관한 로컬 UI 표시 데이터다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI|Preview")
	void SetImportantMissionPreview(const FMyMissionDisplayData& InPreviewData);

	////////////////////////////
	//! \author 장효제
	//! \brief 중요 Mission 표시 데이터를 지우고 전용 영역을 다시 숨긴다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI|Preview")
	void ClearImportantMissionPreview();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	//! WBP_Mission에서 런타임 Mission 행만 보관하는 전용 컨테이너다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VB_MissionRows;

	//! 중요 Mission 행 전용 컨테이너이며 배치하지 않으면 중요 영역을 사용하지 않는다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PNL_ImportantMission;

	//! UMyMissionRowWidget을 부모로 사용하는 WBP_MissionRow 클래스다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI", meta = (AllowAbstract = "false"))
	TSubclassOf<UMyMissionRowWidget> MissionRowWidgetClass;

	//! 중요 Mission 행에 사용할 별도 WBP 클래스이며 비우면 일반 행 클래스를 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI", meta = (AllowAbstract = "false"))
	TSubclassOf<UMyMissionRowWidget> ImportantMissionRowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyMissionDefinitionRow"))
	TObjectPtr<UDataTable> MissionDefinitionTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI", meta = (ClampMin = "0.0"))
	float ResultDisplayDuration = 2.0f;

	//! 에디터 디자인 뷰에서만 중요 Mission 행을 그려 본다. 런타임에는 사용하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Preview")
	bool bUseDesignTimeImportantPreview = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Preview", meta = (EditCondition = "bUseDesignTimeImportantPreview"))
	FMyMissionDisplayData DesignTimeImportantPreview;

private:
	UFUNCTION()
	void HandleMissionHudSelectionChanged();

	void TryBindMissionPlayerController();
	void RefreshImportantMissionRow();
	bool EnsureMissionRowWidgetClass();

	TWeakObjectPtr<ADungeonPC> MissionPlayerController;
	FMyMissionDisplayData ImportantPreviewData;
	bool bHasImportantPreview = false;
	TMap<FGuid, float> TerminalStateFirstSeenTimes;
	TMap<FGuid, TWeakObjectPtr<UMyMissionRowWidget>> TerminalMissionRows;
	bool bLoggedMissingContainer = false;
	bool bLoggedMissingRowClass = false;
};
