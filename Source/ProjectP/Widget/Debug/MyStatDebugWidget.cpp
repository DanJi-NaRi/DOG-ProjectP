#include "Widget/Debug/MyStatDebugWidget.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GAS/MyAttributeSet.h"
#include "GAS/MyPlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Item/MyInventoryComponent.h"
#include "Player/PlayerCharacterBase.h"
#include "Styling/CoreStyle.h"

////////////////////////////
//! \author 준혁
//! \brief WBP 없이 코드로 위젯 트리를 구성한다. (캔버스 → 반투명 배경 보더 → 텍스트, 화면 좌측 중앙 고정)
//! \return 구성된 슬레이트 위젯
TSharedRef<SWidget> UMyStatDebugWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
		Background->SetPadding(FMargin(10.0f, 8.0f));
		Canvas->AddChildToCanvas(Background);

		StatText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatText"));
		StatText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
		StatText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 1.0f, 0.85f)));
		StatText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Background->SetContent(StatText);

		if (UCanvasPanelSlot* BackgroundSlot = Cast<UCanvasPanelSlot>(Background->Slot))
		{
			BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.5f));
			BackgroundSlot->SetAlignment(FVector2D(0.0f, 0.5f));
			BackgroundSlot->SetPosition(FVector2D(16.0f, 0.0f));
			BackgroundSlot->SetAutoSize(true);
		}
	}

	// 디버그 오버레이가 게임 입력/클릭을 가로채지 않도록 한다
	SetVisibility(ESlateVisibility::HitTestInvisible);

	return Super::RebuildWidget();
}

////////////////////////////
//! \author 준혁
//! \brief 갱신 주기마다 스탯 텍스트를 다시 만든다.
//! \param MyGeometry 위젯 지오메트리
//! \param InDeltaTime 프레임 간격(초)
void UMyStatDebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TimeSinceRefresh += InDeltaTime;
	if (TimeSinceRefresh < RefreshInterval || !StatText)
	{
		return;
	}
	TimeSinceRefresh = 0.0f;

	StatText->SetText(FText::FromString(BuildStatText()));
}

////////////////////////////
//! \author 준혁
//! \brief 로컬 플레이어의 레벨/메소/어트리뷰트/활성 효과를 읽어 표시 문자열을 만든다.
//!        이동속도는 어트리뷰트 값과 실제 MaxWalkSpeed를 나란히 표시해 연결 상태를 검증할 수 있게 한다.
//! \return 화면에 표시할 여러 줄 문자열
FString UMyStatDebugWidget::BuildStatText() const
{
	const APlayerController* OwningPC = GetOwningPlayer();
	const AMyPlayerState* MyPlayerState = OwningPC ? OwningPC->GetPlayerState<AMyPlayerState>() : nullptr;
	if (!MyPlayerState)
	{
		return TEXT("[스탯 디버그]\nPlayerState 없음");
	}

	TArray<FString> Lines;
	Lines.Add(TEXT("[스탯 디버그] (/stats 로 닫기)"));

	const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(OwningPC->GetPawn());
	const int32 RequiredExp = PlayerCharacter ? PlayerCharacter->GetExpRequiredForNextLevel() : 0;
	Lines.Add(FString::Printf(TEXT("레벨 %d   경험치 %d / %s"),
		MyPlayerState->GetCharacterLevel(),
		MyPlayerState->GetCharacterExp(),
		RequiredExp > 0 ? *FString::FromInt(RequiredExp) : TEXT("MAX")));

	if (const UMyInventoryComponent* Inventory = MyPlayerState->GetInventoryComponent())
	{
		Lines.Add(FString::Printf(TEXT("메소 %d"), Inventory->GetMeso()));
	}

	if (const UMyAttributeSet* Attributes = MyPlayerState->GetMyAttributeSet())
	{
		const ACharacter* AsCharacter = Cast<ACharacter>(OwningPC->GetPawn());
		const float ActualWalkSpeed = (AsCharacter && AsCharacter->GetCharacterMovement())
			? AsCharacter->GetCharacterMovement()->MaxWalkSpeed
			: 0.0f;

		Lines.Add(TEXT("--------------------"));
		Lines.Add(FString::Printf(TEXT("체력       %.0f / %.0f"), Attributes->GetHealth(), Attributes->GetMaxHealth()));
		Lines.Add(FString::Printf(TEXT("보호막     %.0f"), Attributes->GetShield()));
		Lines.Add(FString::Printf(TEXT("공격력     %.1f"), Attributes->GetAttackPower()));
		Lines.Add(FString::Printf(TEXT("방어력     %.1f"), Attributes->GetDefense()));
		Lines.Add(FString::Printf(TEXT("치명타     %.0f%% (상한 %.0f%%)"), Attributes->GetCritChance() * 100.0f, UMyAttributeSet::CritChanceCap * 100.0f));
		Lines.Add(FString::Printf(TEXT("치명배율   x%.2f"), Attributes->GetCritDamage()));
		Lines.Add(FString::Printf(TEXT("공격속도   %.2f"), Attributes->GetAttackSpeed()));
		Lines.Add(FString::Printf(TEXT("쿨타임감소 %.0f%%"), Attributes->GetCooldownReduction() * 100.0f));
		Lines.Add(FString::Printf(TEXT("대쉬충전   %.1f / %.1f"), Attributes->GetMoveCharge(), Attributes->GetMaxMoveCharge()));
		Lines.Add(FString::Printf(TEXT("이동속도   %.0f (실제 %.0f)"), Attributes->GetMoveSpeed(), ActualWalkSpeed));
	}

	// 지속시간이 있는 활성 GE만 표시한다 (버프 계열 덮어쓰기/쿨타임 확인용, 영구·즉발 GE 제외)
	if (UAbilitySystemComponent* ASC = MyPlayerState->GetAbilitySystemComponent())
	{
		TArray<FString> EffectLines;
		const float WorldTime = ASC->GetWorld() ? ASC->GetWorld()->GetTimeSeconds() : 0.0f;
		for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle);
			if (!ActiveEffect || ActiveEffect->GetDuration() <= 0.0f)
			{
				continue;
			}

			FGameplayTagContainer GrantedTags;
			ActiveEffect->Spec.GetAllGrantedTags(GrantedTags);
			const FString EffectLabel = GrantedTags.IsEmpty() ? GetNameSafe(ActiveEffect->Spec.Def) : GrantedTags.ToStringSimple();
			EffectLines.Add(FString::Printf(TEXT("%s  %.1f초"), *EffectLabel, ActiveEffect->GetTimeRemaining(WorldTime)));
		}

		if (!EffectLines.IsEmpty())
		{
			Lines.Add(TEXT("---- 활성 효과 ----"));
			Lines.Append(EffectLines);
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}
