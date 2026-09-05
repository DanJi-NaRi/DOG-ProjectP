////////////////////////////
//! \page MyMissionDisplayTypes.h
//! \brief Mission HUD와 설정 팝업이 공용으로 쓰는 UI 전용 표시 데이터 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Streaming/MyMissionTypes.h"
#include "MyMissionDisplayTypes.generated.h"

class UDataTable;
class UTexture2D;
class UWidget;

////////////////////////////
//! \struct FMyMissionDisplayData
//! \author 장효제
//! \brief 서버 Mission View와 1회성 특수 이벤트를 같은 형태로 표시하는 로컬 UI 데이터다.
//!
//! 이 구조체는 복제되지 않으며 어떤 서버 원본 구조체에도 포함하지 않는다.
//! bIsImportant는 Mission 도메인의 영구 속성이 아니라 UI 표시 속성이다.
//! 1회성 특수 이벤트는 일반 Kill Mission과 데이터 형식이 같다고 보장할 수 없으므로
//! 각 표시 요소를 bShow 계열 플래그로 개별 Collapse할 수 있게 한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMyMissionDisplayData
{
	GENERATED_BODY()

	//! 서버 Mission에서 변환한 행만 유효하다. 중요 Mission Preview는 무효 Guid를 허용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	FGuid MissionInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	FText GodName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	TObjectPtr<UTexture2D> GodIcon = nullptr;

	//! 신이 Mission을 방송 콘텐츠로 포장한 연출용 제목이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	FText DisplayName;

	//! 실제로 달성해야 하는 객관적 조건이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	FText ProgressText;

	//! 남은 시간은 실시간 갱신 대상이므로 문구가 아니라 권위 서버 종료 시각으로 전달한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	float EndsAtServerTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	FText MesoDeltaText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	EMyMissionState MissionState = EMyMissionState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bShowGod = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bShowProgress = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bShowRemainingTime = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bShowMesoDelta = false;

	//! 파티 공통 Mission에만 작은 파티 배지를 표시한다. 개인 Mission은 대상 배지가 없다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bShowPartyBadge = false;

	//! 최상단 고정·선택 불가 행으로 표시할지 여부다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bIsImportant = false;

	//! 중요 여부와 이미 3개를 선택했는지를 합친 결과이며 팝업이 행 생성 직전에 확정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|UI")
	bool bIsSelectable = false;
};

namespace MyMissionDisplay
{
	PROJECTP_API FMyMissionDisplayData MakeDisplayDataFromView(
		const FMyMissionPublicView& MissionView,
		const UDataTable* MissionDefinitionTable,
		const UDataTable* GodPresentationTable);

	PROJECTP_API void ApplyOptionalVisibility(UWidget* Widget, bool bShouldShow);
}
