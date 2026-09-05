////////////////////////////
//! \page MyDamageNumberStackWidget.cpp
//! \brief 몬스터 체력바 위 데미지 숫자 스택(아래=최신, 이전 줄은 위로 슬라이드) 위젯을 구현한다.
#include "MyDamageNumberStackWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "MyDamageFloatingPresetSet.h"

////////////////////////////
//! \author 준혁
//! \brief WBP 파생 없이도 동작하도록, 루트 위젯이 없으면 SizeBox > Overlay 구조를 만들어 루트로 지정한다.
//!        SizeBox가 줄 간격 x 줄 수만큼 최소 높이를 예약해, 숫자가 뜨고 사라져도
//!        데미지 표시 영역의 크기가 변하지 않아 주변(이름/HP바) 레이아웃이 밀리지 않는다.
//! \param 없음
//! \return 초기화 성공 여부
bool UMyDamageNumberStackWidget::Initialize()
{
	const bool bResult = Super::Initialize();

	if (bResult && WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DamageNumberRootSizeBox"));
		UOverlay* NewOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DamageNumberOverlay"));

		if (RootSizeBox && NewOverlay)
		{
			if (USizeBoxSlot* OverlayHostSlot = Cast<USizeBoxSlot>(RootSizeBox->AddChild(NewOverlay)))
			{
				OverlayHostSlot->SetHorizontalAlignment(HAlign_Fill);
				OverlayHostSlot->SetVerticalAlignment(VAlign_Fill);
			}

			RootSizeBox->SetMinDesiredHeight(LineHeight * static_cast<float>(MaxLines));
			EntryOverlay = NewOverlay;
			WidgetTree->RootWidget = RootSizeBox;
		}
	}

	return bResult;
}

////////////////////////////
//! \author 준혁
//! \brief 예약 높이를 현재 프로퍼티(LineHeight x MaxLines)로 갱신하고,
//!        WBP 디자이너에서는 배치/스타일 확인용 미리보기 줄을 구성한다.
//! \param 없음
//! \return 없음
void UMyDamageNumberStackWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (WidgetTree)
	{
		if (USizeBox* RootSizeBox = Cast<USizeBox>(WidgetTree->RootWidget))
		{
			RootSizeBox->SetMinDesiredHeight(LineHeight * static_cast<float>(MaxLines));
		}
	}

	if (IsDesignTime() && bShowDesignPreview)
	{
		BuildDesignPreview();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 데미지 한 건을 스택 맨 아래에 추가한다. 기존 줄은 한 칸씩 위로 슬라이드를 시작하고,
//!        MaxLines를 넘겨 밀려난 줄은 즉시 페이드아웃으로 전환한다.
//! \param DamageAmount 표시할 데미지 값 (정수 반올림 표시)
//! \param bCriticalHit 치명타 여부 (Normal/Critical 스타일 분기)
//! \return 없음
void UMyDamageNumberStackWidget::PushDamage(float DamageAmount, bool bCriticalHit)
{
	PushTypedDamage(
		DamageAmount,
		bCriticalHit ? EDamageNumberDisplayType::Critical : EDamageNumberDisplayType::Normal);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 데미지 타입에 대응하는 텍스트 스타일과 이동 프리셋으로 숫자 한 건을 추가하는 함수
// DamageAmount : 표시할 데미지 값 (정수 반올림 표시)
// DamageType : Normal, Critical 또는 DOT 표시 타입
void UMyDamageNumberStackWidget::PushTypedDamage(float DamageAmount, EDamageNumberDisplayType DamageType)
{
	if (!GetRootOverlay())
	{
		return;
	}

	// 기존 줄을 한 칸씩 위로 밀고, 최대 줄 수를 넘긴 줄은 페이드아웃시킨다.
	for (FDamageNumberEntry& Entry : Entries)
	{
		++Entry.SlotIndex;
		Entry.SlideStartY = Entry.CurrentY;
		Entry.SlideElapsed = 0.0f;

		if (Entry.SlotIndex >= MaxLines && !Entry.bFadingOut)
		{
			Entry.bFadingOut = true;
			Entry.FadeElapsed = 0.0f;
			Entry.FadeStartScale = Entry.CurrentScale;
			Entry.FadeStartOpacity = Entry.CurrentOpacity;
		}
	}

	UTextBlock* NewText = AcquireTextBlock();
	if (!NewText)
	{
		return;
	}

	const FDamageNumberTextStyle& Style = GetTextStyle(DamageType);
	const FDamageNumberPreset Preset = ResolvePreset(DamageType);
	const FDamageNumberAnimationSample InitialSample =
		UMyDamageFloatingPresetJsonLibrary::EvaluatePresetAtTime(Preset, 0.0f);

	ApplyPresetTextStyleToText(NewText, Preset, Style);
	NewText->SetText(FText::AsNumber(FMath::RoundToInt(DamageAmount)));
	NewText->SetRenderOpacity(InitialSample.Opacity);
	NewText->SetRenderTranslation(InitialSample.Translation);
	NewText->SetRenderScale(FVector2D(InitialSample.Scale, InitialSample.Scale));
	NewText->SetVisibility(ESlateVisibility::HitTestInvisible);

	FDamageNumberEntry NewEntry;
	NewEntry.Text = NewText;
	NewEntry.PopInScale = Preset.Scale.Initial;
	NewEntry.Preset = Preset;
	NewEntry.CurrentScale = InitialSample.Scale;
	NewEntry.CurrentOpacity = InitialSample.Opacity;
	Entries.Add(NewEntry);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 현재 재생 중인 데미지 숫자가 하나 이상 있는지 반환하는 함수
// Return Value : 재생 중인 숫자가 있으면 true
bool UMyDamageNumberStackWidget::HasActiveEntries() const
{
	return !Entries.IsEmpty();
}

////////////////////////////
//! \author 준혁
//! \brief 표시 중인 줄들의 슬라이드/팝/수명/페이드를 진행시키고, 끝난 줄은 풀로 회수한다.
//! \param MyGeometry 위젯 지오메트리
//! \param InDeltaTime 프레임 간격(초)
//! \return 없음
void UMyDamageNumberStackWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Entries.Num() == 0)
	{
		return;
	}

	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FDamageNumberEntry& Entry = Entries[Index];
		if (!Entry.Text)
		{
			Entries.RemoveAt(Index);
			continue;
		}

		Entry.Age += InDeltaTime;

		// 위로 밀려 올라가는 슬라이드 (EaseOut)
		const float TargetY = -static_cast<float>(Entry.SlotIndex) * LineHeight;
		if (!FMath::IsNearlyEqual(Entry.CurrentY, TargetY))
		{
			Entry.SlideElapsed += InDeltaTime;
			const float SlideAlpha = SlideDuration > 0.0f
				? FMath::Clamp(Entry.SlideElapsed / SlideDuration, 0.0f, 1.0f)
				: 1.0f;
			Entry.CurrentY = FMath::InterpEaseOut(Entry.SlideStartY, TargetY, SlideAlpha, 2.0f);
		}

		// 등장 팝 (Initial -> Normal), 경로 이동, 등장 페이드를 프리셋 시간으로 함께 계산한다.
		FDamageNumberAnimationSample AnimationSample =
			UMyDamageFloatingPresetJsonLibrary::EvaluatePresetAtTime(Entry.Preset, Entry.Age);

		// 수명이 다한 줄은 프리셋의 퇴장 크기/투명도 구간으로 전환한다.
		const bool bWasFadingOut = Entry.bFadingOut;
		const float NaturalExitStartTime = Entry.Preset.Timing.Appear + Entry.Preset.Timing.Hold;
		if (!Entry.bFadingOut && Entry.Age >= NaturalExitStartTime)
		{
			Entry.bFadingOut = true;
			Entry.FadeElapsed = FMath::Max(Entry.Age - NaturalExitStartTime, 0.0f);
			Entry.FadeStartScale = Entry.Preset.Scale.Normal;
			Entry.FadeStartOpacity = 1.0f;
		}
		else if (bWasFadingOut)
		{
			Entry.FadeElapsed += InDeltaTime;
		}

		if (Entry.bFadingOut)
		{
			const float FadeAlpha = Entry.Preset.Timing.Exit > 0.0f
				? FMath::Clamp(Entry.FadeElapsed / Entry.Preset.Timing.Exit, 0.0f, 1.0f)
				: 1.0f;
			const float FadeProgress = FMath::InterpEaseOut(0.0f, 1.0f, FadeAlpha, 3.0f);
			AnimationSample.Scale = FMath::Lerp(
				Entry.FadeStartScale,
				Entry.Preset.Scale.Exit,
				FadeProgress);
			AnimationSample.Opacity = Entry.Preset.Opacity.bFadeOut
				? FMath::Lerp(Entry.FadeStartOpacity, 0.0f, FadeAlpha)
				: Entry.FadeStartOpacity;
			AnimationSample.bFinished = FadeAlpha >= 1.0f;

			if (AnimationSample.bFinished)
			{
				ReleaseEntry(Entry);
				Entries.RemoveAt(Index);
				continue;
			}
		}

		AnimationSample.Translation.Y += Entry.CurrentY;
		Entry.CurrentScale = AnimationSample.Scale;
		Entry.CurrentOpacity = AnimationSample.Opacity;
		Entry.Text->SetRenderTranslation(AnimationSample.Translation);
		Entry.Text->SetRenderScale(FVector2D(AnimationSample.Scale, AnimationSample.Scale));
		Entry.Text->SetRenderOpacity(AnimationSample.Opacity);
	}
}

////////////////////////////
//! \author 준혁
//! \brief 숫자 텍스트들을 담는 Overlay를 반환한다. 코드 구성 루트(SizeBox > Overlay)가 기본이고,
//!        파생 WBP가 자체 루트 Overlay를 가진 경우는 그것을 사용한다.
//! \param 없음
//! \return 엔트리 Overlay (없으면 nullptr)
UOverlay* UMyDamageNumberStackWidget::GetRootOverlay() const
{
	if (EntryOverlay)
	{
		return EntryOverlay;
	}

	return WidgetTree ? Cast<UOverlay>(WidgetTree->RootWidget) : nullptr;
}

////////////////////////////
//! \author 준혁
//! \brief 풀에서 텍스트 블록을 꺼내거나, 없으면 새로 만들어 Overlay 하단 중앙 정렬로 붙인다.
//! \param 없음
//! \return 사용 준비된 텍스트 블록 (실패 시 nullptr)
UTextBlock* UMyDamageNumberStackWidget::AcquireTextBlock()
{
	UOverlay* RootOverlay = GetRootOverlay();
	if (!RootOverlay || !WidgetTree)
	{
		return nullptr;
	}

	while (TextBlockPool.Num() > 0)
	{
		if (UTextBlock* Pooled = TextBlockPool.Pop())
		{
			return Pooled;
		}
	}

	UTextBlock* NewText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!NewText)
	{
		return nullptr;
	}

	// 하단 중앙 기준으로 겹쳐 두고 RenderTranslation으로 줄 위치를 잡는다.
	if (UOverlaySlot* NewSlot = RootOverlay->AddChildToOverlay(NewText))
	{
		NewSlot->SetHorizontalAlignment(HAlign_Center);
		NewSlot->SetVerticalAlignment(VAlign_Bottom);
	}

	return NewText;
}

////////////////////////////
//! \author 준혁
//! \brief 표시가 끝난 줄의 텍스트 블록을 Collapsed로 감추고 재사용 풀에 반납한다.
//! \param Entry 회수할 줄 상태
//! \return 없음
void UMyDamageNumberStackWidget::ReleaseEntry(FDamageNumberEntry& Entry)
{
	if (Entry.Text)
	{
		Entry.Text->SetVisibility(ESlateVisibility::Collapsed);
		TextBlockPool.Add(Entry.Text);
		Entry.Text = nullptr;
	}
}

////////////////////////////
//! \author 준혁
//! \brief 스타일의 폰트/색상을 텍스트 블록에 적용한다. 폰트 미지정이면 기존(엔진 기본) 폰트를 유지한다.
//! \param InText 적용 대상 텍스트 블록
//! \param Style 적용할 스타일
//! \return 없음
void UMyDamageNumberStackWidget::ApplyStyleToText(UTextBlock* InText, const FDamageNumberTextStyle& Style) const
{
	if (!InText)
	{
		return;
	}

	if (Style.Font.HasValidFont())
	{
		InText->SetFont(Style.Font);
	}

	InText->SetColorAndOpacity(FSlateColor(Style.Color));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// schemaVersion 2 프리셋의 폰트, 크기, 타입페이스, 색상, 외곽선을 텍스트에 적용하는 함수
// InText : 적용 대상 텍스트 블록
// Preset : JSON에서 가져온 타입별 프리셋
// FallbackStyle : 프리셋 폰트를 불러오지 못하거나 구형 프리셋일 때 사용할 위젯 스타일
void UMyDamageNumberStackWidget::ApplyPresetTextStyleToText(
	UTextBlock* InText,
	const FDamageNumberPreset& Preset,
	const FDamageNumberTextStyle& FallbackStyle) const
{
	if (!InText)
	{
		return;
	}

	if (Preset.SchemaVersion < 2)
	{
		ApplyStyleToText(InText, FallbackStyle);
		return;
	}

	FSlateFontInfo FontInfo = FallbackStyle.Font.HasValidFont()
		? FallbackStyle.Font
		: InText->GetFont();

	if (UObject* FontObject = Preset.TextStyle.FontAssetPath.TryLoad())
	{
		FontInfo.FontObject = FontObject;
	}

	FontInfo.Size = static_cast<float>(Preset.TextStyle.FontSize);
	FontInfo.TypefaceFontName = Preset.TextStyle.Typeface;
	FontInfo.OutlineSettings.OutlineSize = Preset.TextStyle.OutlineSize;
	FontInfo.OutlineSettings.OutlineColor = Preset.TextStyle.OutlineColor;
	InText->SetFont(FontInfo);
	InText->SetColorAndOpacity(FSlateColor(Preset.TextStyle.Color));
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// DataAsset에서 타입별 프리셋을 가져오고, 미지정 또는 유효하지 않으면 기존 설정으로 대체하는 함수
// DamageType : Normal, Critical 또는 DOT 표시 타입
// Return Value : 숫자 한 건에 복사해 사용할 유효한 데미지 플로팅 프리셋
FDamageNumberPreset UMyDamageNumberStackWidget::ResolvePreset(EDamageNumberDisplayType DamageType) const
{
	if (DamageFloatingPresetSet)
	{
		const FDamageNumberPreset& ImportedPreset = DamageFloatingPresetSet->GetPreset(DamageType);
		FString ValidationError;
		if (UMyDamageFloatingPresetJsonLibrary::ValidatePreset(ImportedPreset, ValidationError))
		{
			return ImportedPreset;
		}
	}

	const FDamageNumberTextStyle& TextStyle = GetTextStyle(DamageType);
	FDamageNumberPreset FallbackPreset;
	FallbackPreset.SchemaVersion = 1;
	FallbackPreset.DesignName = TEXT("LegacyFallback");
	FallbackPreset.DamageType = DamageType;
	FallbackPreset.Motion.Path = EDamageNumberMotionPath::Linear;
	FallbackPreset.Scale.Initial = TextStyle.PopInScale;
	FallbackPreset.Scale.Normal = 1.0f;
	FallbackPreset.Scale.Exit = 1.0f;
	FallbackPreset.Timing.Appear = PopInDuration;
	FallbackPreset.Timing.Hold = FMath::Max(EntryLifetime - PopInDuration, 0.0f);
	FallbackPreset.Timing.Exit = FadeOutDuration;
	FallbackPreset.Timing.Total =
		FallbackPreset.Timing.Appear + FallbackPreset.Timing.Hold + FallbackPreset.Timing.Exit;
	FallbackPreset.Opacity.bFadeIn = false;
	FallbackPreset.Opacity.bFadeOut = true;
	return FallbackPreset;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 데미지 표시 타입에 대응하는 폰트와 색상 스타일을 반환하는 함수
// DamageType : Normal, Critical 또는 DOT 표시 타입
// Return Value : 위젯 Details 패널에서 설정한 해당 타입의 텍스트 스타일
const FDamageNumberTextStyle& UMyDamageNumberStackWidget::GetTextStyle(EDamageNumberDisplayType DamageType) const
{
	switch (DamageType)
	{
	case EDamageNumberDisplayType::Critical:
		return CriticalStyle;

	case EDamageNumberDisplayType::Dot:
		return DotStyle;

	case EDamageNumberDisplayType::Normal:
	default:
		return NormalStyle;
	}
}

////////////////////////////
//! \author 준혁
//! \brief WBP 디자이너 전용 미리보기: 치명타/일반/DOT 스타일 예시를 정적으로 배치한다.
//! \param 없음
//! \return 없음
void UMyDamageNumberStackWidget::BuildDesignPreview()
{
	UOverlay* RootOverlay = GetRootOverlay();
	if (!RootOverlay || !WidgetTree)
	{
		return;
	}

	// NativePreConstruct는 프로퍼티가 바뀔 때마다 다시 불리므로 매번 새로 구성한다.
	RootOverlay->ClearChildren();
	TextBlockPool.Reset();
	Entries.Reset();

	struct FPreviewLine
	{
		const TCHAR* Text;
		EDamageNumberDisplayType DamageType;
		int32 Line;
	};
	const FPreviewLine PreviewLines[] = {
		{ TEXT("999"), EDamageNumberDisplayType::Critical, 0 },
		{ TEXT("128"), EDamageNumberDisplayType::Normal, 1 },
		{ TEXT("24"), EDamageNumberDisplayType::Dot, 2 },
	};

	for (const FPreviewLine& Preview : PreviewLines)
	{
		UTextBlock* PreviewText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (!PreviewText)
		{
			continue;
		}

		const FDamageNumberPreset Preset = ResolvePreset(Preview.DamageType);
		ApplyPresetTextStyleToText(
			PreviewText,
			Preset,
			GetTextStyle(Preview.DamageType));
		PreviewText->SetText(FText::FromString(Preview.Text));
		PreviewText->SetRenderTranslation(FVector2D(0.0f, -static_cast<float>(Preview.Line) * LineHeight));

		if (UOverlaySlot* PreviewSlot = RootOverlay->AddChildToOverlay(PreviewText))
		{
			PreviewSlot->SetHorizontalAlignment(HAlign_Center);
			PreviewSlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}
}
