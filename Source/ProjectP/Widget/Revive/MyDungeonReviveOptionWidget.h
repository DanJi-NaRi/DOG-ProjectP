////////////////////////////
//! \page MyDungeonReviveOptionWidget.h
//! \brief 부활 패널의 부활 옵션 카드 한 장을 표시하는 위젯 선언 파일이다.
#pragma once

#include "CommonUserWidget.h"
#include "Dungeon/Revive/DungeonReviveDataAsset.h"
#include "MyDungeonReviveOptionWidget.generated.h"

class UButton;
class UDataTable;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDungeonReviveOptionSelectedSignature, FName, OptionId);

////////////////////////////
//! \class UMyDungeonReviveOptionWidget
//! \author 장효제
//! \brief 부활 옵션 하나의 초상화, 이름, 설명, 메소 비용, 부활 체력, 대기시간을 표시하고 클릭 시 OptionId를 알린다.
//! \note WBP_DungeonReviveOption의 Parent Class로 사용한다. 카드 개수는 데이터 에셋의 옵션 수만큼 패널이 동적으로 생성한다.
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class PROJECTP_API UMyDungeonReviveOptionWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	//! 카드에 표시할 부활 옵션을 설정한다.
	void SetReviveOption(const FDungeonReviveOption& InOption);

	//! 메소가 충분한지에 따라 버튼 활성 상태와 비용 색상을 갱신한다.
	void SetAffordable(bool bInAffordable);

	UFUNCTION(BlueprintPure, Category = "UI|Revive")
	FName GetOptionId() const { return OptionId; }

	//! 카드 클릭 시 알림. 부활 패널이 서버 요청에 사용한다.
	UPROPERTY(BlueprintAssignable, Category = "UI|Revive")
	FDungeonReviveOptionSelectedSignature OnOptionSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//! 데이터의 StyleId에 맞춰 테두리, 색상 같은 추가 연출을 WBP에서 구현할 때 사용한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Revive")
	void BP_OnStyleApplied(FName StyleId);

	//! 메소 비용 표시 형식. {0}에 비용이 들어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText MesoCostFormat = NSLOCTEXT("DungeonRevive", "Option_MesoCost", "{0} 메소");

	//! 무료 옵션의 비용 표시 문구
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText FreeCostText = NSLOCTEXT("DungeonRevive", "Option_FreeCost", "무료");

	//! 부활 체력 표시 형식. {0}에 최대 체력 대비 퍼센트가 들어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText HealthPercentFormat = NSLOCTEXT("DungeonRevive", "Option_HealthPercent", "체력 {0}%");

	//! 부활 대기시간 표시 형식. {0}에 초가 들어간다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FText ReviveDelayFormat = NSLOCTEXT("DungeonRevive", "Option_ReviveDelay", "{0}초 후 부활");

	//! 메소가 부족한 카드의 비용 텍스트 색상
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Revive|Format")
	FLinearColor NotAffordableCostColor = FLinearColor(0.85f, 0.2f, 0.2f, 1.0f);

	//! 신 이름, 초상화, 대표색을 가져올 DataTable이다. 비워 두면 기본 경로에서 지연 로드한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Revive", meta = (RequiredAssetDataTags = "RowStructure=/Script/ProjectP.MyGodPresentationRow"))
	TObjectPtr<UDataTable> GodPresentationTable;

private:
	UFUNCTION()
	void HandleSelectButtonClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UButton> BTN_Select;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_DisplayName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_MesoCost;

	//! 카드 상단의 스타일 라벨. 데이터의 StyleId를 그대로 표시한다. (기획 목업의 "후견자 보정 / 균형형 / 고회복")
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_StyleLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UImage> IMG_Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_HealthPercent;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess))
	TObjectPtr<UTextBlock> TXT_ReviveDelay;

	FName OptionId = NAME_None;

	//! 비용 부족 색상을 적용하기 전의 원래 TXT_MesoCost 색상
	FSlateColor DefaultCostColor;

	bool bHasCachedDefaultCostColor = false;
};
