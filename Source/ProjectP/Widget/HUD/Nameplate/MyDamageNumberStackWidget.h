#pragma once

#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "MyDamageFloatingPreset.h"
#include "MyDamageNumberStackWidget.generated.h"

class UMyDamageFloatingPresetSet;
class UOverlay;
class UTextBlock;

////////////////////////////
//! \brief 데미지 숫자 한 줄의 텍스트 스타일. 폰트 종류/사이즈/타입페이스/아웃라인은 Font 안에서 지정한다.
USTRUCT(BlueprintType)
struct FDamageNumberTextStyle
{
	GENERATED_BODY()

	// 폰트. 종류(Font Family)/사이즈(Size)/타입페이스/아웃라인을 모두 포함한다. 미지정 시 엔진 기본 폰트를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
	FSlateFontInfo Font;

	// 숫자 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber")
	FLinearColor Color = FLinearColor::White;

	// 등장 순간의 시작 스케일. 1.0이면 팝 연출 없음.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageNumber", meta = (ClampMin = "1.0"))
	float PopInScale = 1.3f;
};

////////////////////////////
//! \brief 스택에 표시 중인 데미지 숫자 한 줄의 런타임 상태.
USTRUCT()
struct FDamageNumberEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text = nullptr;

	// 0 = 체력바 바로 위(최신), 새 데미지가 들어올 때마다 1씩 증가하며 위로 밀린다.
	int32 SlotIndex = 0;

	// 스폰 후 경과 시간. EntryLifetime을 넘기면 페이드아웃을 시작한다.
	float Age = 0.0f;

	// 위로 슬라이드하는 보간 상태
	float SlideStartY = 0.0f;
	float SlideElapsed = 0.0f;
	float CurrentY = 0.0f;

	// 등장 팝 보간 상태
	float PopElapsed = 0.0f;
	float PopInScale = 1.0f;

	// 페이드아웃 상태
	bool bFadingOut = false;
	float FadeElapsed = 0.0f;

	// 이 숫자 한 건이 생성될 때 복사한 이동/크기/시간 프리셋
	FDamageNumberPreset Preset;

	// 강제 퇴장 시 현재 상태에서 자연스럽게 이어가기 위한 화면 상태
	float CurrentScale = 1.0f;
	float CurrentOpacity = 1.0f;
	float FadeStartScale = 1.0f;
	float FadeStartOpacity = 1.0f;
};

////////////////////////////
//! \author 준혁
//! \brief 몬스터 체력바 바로 위에 로컬 플레이어가 넣은 데미지를 줄 단위로 쌓아 보여주는 위젯.
//!        가장 아래 줄이 최신 데미지이고, 새 데미지가 들어오면 기존 줄이 위로 슬라이드된다.
//!        MaxLines를 넘겨 밀려난 줄과 수명이 다한 줄은 페이드아웃된다.
//!        WBP 파생 없이 동작하도록 루트(SizeBox > Overlay)와 텍스트 블록을 코드에서 생성하며,
//!        줄 간격 x 줄 수만큼 높이를 항상 예약해 숫자가 뜨고 사라져도 주변 레이아웃이 밀리지 않는다.
//!        WBP_Nameplate에서 배치하고 이름을 DamageNumberStack으로 맞추면 네임플레이트와 연결된다.
UCLASS()
class PROJECTP_API UMyDamageNumberStackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 데미지 한 건을 스택 맨 아래(체력바 바로 위)에 추가한다.
	UFUNCTION(BlueprintCallable, Category = "UI|DamageNumber")
	void PushDamage(float DamageAmount, bool bCriticalHit);

	// 데미지 타입을 명시해 숫자를 추가한다. DOT는 이 함수를 통해 별도 스타일과 프리셋을 사용한다.
	UFUNCTION(BlueprintCallable, Category = "UI|DamageNumber")
	void PushTypedDamage(float DamageAmount, EDamageNumberDisplayType DamageType);

	// 현재 재생 중인 데미지 숫자가 하나 이상 있는지 반환한다.
	UFUNCTION(BlueprintPure, Category = "UI|DamageNumber")
	bool HasActiveEntries() const;

	virtual bool Initialize() override;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- 스타일 (전부 에디터에서 튜닝, 코드 수정 불필요) ---

	// JSON에서 가져온 Normal/Critical/DOT 이동 프리셋 세트. 미지정 시 기존 설정으로 동작한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Preset")
	TObjectPtr<UMyDamageFloatingPresetSet> DamageFloatingPresetSet;

	// 일반 데미지 스타일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Style")
	FDamageNumberTextStyle NormalStyle;

	// 치명타 데미지 스타일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Style")
	FDamageNumberTextStyle CriticalStyle;

	// 도트 데미지 스타일
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Style")
	FDamageNumberTextStyle DotStyle;

	// --- 배치 ---

	// 동시에 보여줄 최대 줄 수. 초과로 밀려난 가장 오래된 줄은 즉시 페이드아웃된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Layout", meta = (ClampMin = "1"))
	int32 MaxLines = 2;

	// 줄 간격(px). 한 줄이 위로 밀려 올라가는 한 칸의 높이.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Layout", meta = (ClampMin = "1.0"))
	float LineHeight = 24.0f;

	// --- 타이밍 ---

	// 한 줄이 표시를 유지하는 시간(초). 지나면 페이드아웃을 시작한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
	float EntryLifetime = 1.0f;

	// 윗줄로 밀려 올라가는 슬라이드 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
	float SlideDuration = 0.12f;

	// 등장 팝(스케일이 PopInScale에서 1.0으로 줄어드는) 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
	float PopInDuration = 0.08f;

	// 사라질 때 페이드아웃 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageNumber|Timing", meta = (ClampMin = "0.0"))
	float FadeOutDuration = 0.2f;

	// --- 디자이너 미리보기 ---

	// WBP 디자이너에서 예시 2줄(아래=치명타, 위=일반)을 표시할지 여부. 게임에는 영향 없음.
	UPROPERTY(EditAnywhere, Category = "DamageNumber|Preview")
	bool bShowDesignPreview = true;

private:
	UOverlay* GetRootOverlay() const;
	UTextBlock* AcquireTextBlock();
	void ReleaseEntry(FDamageNumberEntry& Entry);
	void ApplyStyleToText(UTextBlock* InText, const FDamageNumberTextStyle& Style) const;
	void ApplyPresetTextStyleToText(
		UTextBlock* InText,
		const FDamageNumberPreset& Preset,
		const FDamageNumberTextStyle& FallbackStyle) const;
	void BuildDesignPreview();
	FDamageNumberPreset ResolvePreset(EDamageNumberDisplayType DamageType) const;
	const FDamageNumberTextStyle& GetTextStyle(EDamageNumberDisplayType DamageType) const;

	// 표시 중인 줄 목록 (배열 순서와 화면 슬롯 순서는 무관)
	UPROPERTY(Transient)
	TArray<FDamageNumberEntry> Entries;

	// 재사용 대기 중인 텍스트 블록 풀 (Overlay 자식으로 유지, Collapsed 상태)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TextBlockPool;

	// 코드로 구성한 루트(SizeBox) 안에서 숫자 텍스트들을 담는 Overlay
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> EntryOverlay;
};
