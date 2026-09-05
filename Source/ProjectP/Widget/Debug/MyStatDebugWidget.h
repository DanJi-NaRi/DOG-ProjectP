////////////////////////////
//! \page MyStatDebugWidget.h
//! \brief 테스트용 스탯 디버그 오버레이 위젯 선언 파일이다. (/stats 치트로 토글)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyStatDebugWidget.generated.h"

class UTextBlock;

////////////////////////////
//! \class UMyStatDebugWidget
//! \author 준혁
//! \brief 화면 좌측에 로컬 플레이어의 어트리뷰트/레벨/메소/활성 효과를 표시하는 테스트용 디버그 위젯이다.
//!        WBP 없이 C++만으로 레이아웃을 구성하며(에디터 자산 불필요), 입력을 가로채지 않는다(HitTestInvisible).
//! \note 로컬 표시 전용 — 값은 복제된 PlayerState/ASC에서 읽기만 하므로 서버 동작에 영향 없음.
UCLASS()
class PROJECTP_API UMyStatDebugWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	//! 스탯 텍스트 갱신 주기(초). 디버그용이라 매 프레임 갱신할 필요 없음.
	static constexpr float RefreshInterval = 0.1f;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatText;

	float TimeSinceRefresh = 0.0f;

	FString BuildStatText() const;
};
