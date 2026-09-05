////////////////////////////
//! \page MyMissionRowWidget.h
//! \brief 개별 Mission HUD 행에 텍스트와 상태를 적용하는 위젯 클래스 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Streaming/MyMissionTypes.h"
#include "Widget/HUD/Mission/MyMissionDisplayTypes.h"
#include "MyMissionRowWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;

////////////////////////////
//! \class UMyMissionRowWidget
//! \author 장효제
//! \brief Mission 행의 데이터 적용은 C++에 두고 이미지·색·애니메이션 표현은 자식 WBP에 맡긴다.
UCLASS(Abstract, Blueprintable)
class PROJECTP_API UMyMissionRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	////////////////////////////
	//! \author 장효제
	//! \brief Mission 식별자, 목표 문구, 진행 문구와 상태를 현재 HUD 행에 적용한다.
	//! \param InMissionInstanceId 표시할 Mission 인스턴스 식별자다.
	//! \param InObjectiveText 표시할 Mission 목표 문구다.
	//! \param InProgressText 표시할 Mission 진행 또는 결과 문구다.
	//! \param InMissionState 표시할 Mission 상태다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI")
	void SetMissionRowData(
		const FGuid& InMissionInstanceId,
		const FText& InObjectiveText,
		const FText& InProgressText,
		EMyMissionState InMissionState);

	////////////////////////////
	//! \author 장효제
	//! \brief 일반 Mission과 중요 Mission을 같은 UI 전용 표시 데이터로 HUD 행에 적용한다.
	//! \param InDisplayData 표시할 UI 전용 Mission 데이터다.
	UFUNCTION(BlueprintCallable, Category = "Mission|UI")
	void SetMissionDisplayData(const FMyMissionDisplayData& InDisplayData);

	UFUNCTION(BlueprintPure, Category = "Mission|UI")
	FGuid GetMissionInstanceId() const;

protected:
	virtual void NativePreConstruct() override;

	//! 중요 행과 일반 행을 색으로 구분할 배경이며 배치하지 않으면 색 적용을 건너뛴다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BRD_RowBackground;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Objective;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TXT_Progress;

	//! 연출용 제목을 별도로 보여줄 때만 WBP에 배치한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_DisplayName;

	//! 달성 시 Meso 효과를 보여줄 때만 WBP에 배치한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_MesoDelta;

	//! 제안 신 이름을 보여줄 때만 WBP에 배치한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_GodName;

	//! 제안 신 초상화를 보여줄 때만 WBP에 배치한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IMG_GodIcon;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Mission|UI")
	FGuid MissionInstanceId;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Mission|UI")
	EMyMissionState MissionState = EMyMissionState::None;

	//! 중요 Mission 행 WBP가 일반 행과 다른 표현을 적용할 수 있게 마지막 적용값을 남긴다.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Mission|UI")
	bool bIsImportantRow = false;

	//! 일반 HUD Mission 행 배경색이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style")
	FLinearColor NormalRowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	//! 최상단 중요 Mission 행 배경색이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mission|UI|Style")
	FLinearColor ImportantRowColor = FLinearColor(1.0f, 0.25f, 0.08f, 0.55f);

	UFUNCTION(BlueprintImplementableEvent, Category = "Mission|UI", meta = (DisplayName = "On Mission State Applied"))
	void BP_OnMissionStateApplied(EMyMissionState InMissionState);

private:
	void ApplyRowVisualState();
};
