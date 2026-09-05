////////////////////////////
//! \page MyDungeonRevivePanelWidget.cpp
//! \brief 부활 옵션 패널 위젯 구현 파일이다.
#include "Widget/Revive/MyDungeonRevivePanelWidget.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Dungeon/DungeonGS.h"
#include "Dungeon/DungeonPC.h"
#include "Dungeon/Revive/DungeonReviveDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Item/MyInventoryComponent.h"
#include "Widget/Revive/MyDungeonReviveOptionWidget.h"

////////////////////////////
//! \author 장효제
//! \brief 부활 패널은 임의로 닫을 수 없으므로 CommonUI Back 핸들러를 끈 상태로 시작한다.
//! \param 없음
//! \return 없음
UMyDungeonRevivePanelWidget::UMyDungeonRevivePanelWidget()
{
	bUseCommonUIBackHandler = false;
}

////////////////////////////
//! \author 장효제
//! \brief 활성화 시 메소 변경을 구독하고 부활 옵션 카드를 만들며 선택 제한 시간을 시작한다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (!BindToInventoryComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Revive UI] InventoryComponent not found on owning PlayerState."));
	}

	bReviveRequested = false;
	ReviveDisplayEndSeconds = 0.0f;

	BuildOptionCards();
	RefreshAffordability();

	const UDungeonReviveDataAsset* ReviveData = GetReviveData();
	const float SelectionTimeoutSeconds = ReviveData ? ReviveData->SelectionTimeoutSeconds : 0.0f;
	const UWorld* World = GetWorld();
	bSelectionCountdownActive = SelectionTimeoutSeconds > 0.0f && World != nullptr && OptionWidgets.Num() > 0;
	SelectionDeadlineSeconds = bSelectionCountdownActive
		? World->GetTimeSeconds() + SelectionTimeoutSeconds
		: 0.0f;

	if (PB_SelectionTimeout)
	{
		PB_SelectionTimeout->SetVisibility(bSelectionCountdownActive
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
		PB_SelectionTimeout->SetPercent(1.0f);
	}

	if (OptionWidgets.Num() == 0)
	{
		SetStatusText(NoOptionNoticeText);
	}

	// InputMode=Menu에서 카드 버튼 입력을 받도록 패널이 포커스를 가져간다.
	SetFocus();
}

////////////////////////////
//! \author 장효제
//! \brief 비활성화 시 메소 구독을 해제하고 카드와 카운트다운 상태를 정리한다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::NativeOnDeactivated()
{
	UnbindFromInventoryComponent();

	for (const TObjectPtr<UMyDungeonReviveOptionWidget>& OptionWidget : OptionWidgets)
	{
		if (OptionWidget)
		{
			OptionWidget->OnOptionSelected.RemoveDynamic(this, &UMyDungeonRevivePanelWidget::HandleOptionSelected);
		}
	}
	OptionWidgets.Reset();

	if (HB_ReviveOptions)
	{
		HB_ReviveOptions->ClearChildren();
	}

	bSelectionCountdownActive = false;
	bReviveRequested = false;

	Super::NativeOnDeactivated();
}

////////////////////////////
//! \author 장효제
//! \brief 선택 제한 시간과 부활 진행 표시를 매 프레임 갱신한다.
//! \param MyGeometry 위젯 지오메트리
//! \param InDeltaTime 프레임 델타 시간
//! \return 없음
void UMyDungeonRevivePanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bReviveRequested)
	{
		// ponytail: 서버의 실제 남은 대기시간을 복제하지 않고 요청 시점 기준으로만 표시한다.
		// 진행바 정확도가 필요해지면 DungeonGS에 부활 완료 예정 서버 시간을 복제해 대체한다.
		const float RemainingSeconds = FMath::Max(0.0f, ReviveDisplayEndSeconds - World->GetTimeSeconds());
		SetStatusText(FText::Format(ReviveRequestedFormat, FText::AsNumber(FMath::CeilToInt(RemainingSeconds))));
		return;
	}

	UpdateSelectionCountdown();
}

////////////////////////////
//! \author 장효제
//! \brief CommonUI가 활성화 시 포커스를 줄 대상으로 이 위젯 자신을 반환한다.
//! \param 없음
//! \return 포커스 대상 위젯
UWidget* UMyDungeonRevivePanelWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UMyDungeonRevivePanelWidget*>(this);
}

////////////////////////////
//! \author 장효제
//! \brief 선택한 부활 옵션을 ADungeonPC를 통해 서버에 요청하고 진행 표시로 전환한다.
//! \param OptionId 선택한 부활 옵션 ID
//! \return 없음
void UMyDungeonRevivePanelWidget::HandleOptionSelected(FName OptionId)
{
	ADungeonPC* DungeonPC = Cast<ADungeonPC>(GetOwningPlayer());
	if (!DungeonPC)
	{
		return;
	}

	// 중복 요청은 서버가 이미 진행 중인 부활로 거절하므로 카드 입력을 잠그지 않는다.
	DungeonPC->RequestRevive(OptionId);

	const UDungeonReviveDataAsset* ReviveData = GetReviveData();
	const FDungeonReviveOption* SelectedOption = ReviveData ? ReviveData->FindOption(OptionId, true) : nullptr;
	const UWorld* World = GetWorld();
	if (SelectedOption && World)
	{
		bReviveRequested = true;
		bSelectionCountdownActive = false;
		ReviveDisplayEndSeconds = World->GetTimeSeconds() + SelectedOption->ReviveDelaySeconds;

		if (PB_SelectionTimeout)
		{
			PB_SelectionTimeout->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

////////////////////////////
//! \author 장효제
//! \brief 메소 보유량이 바뀌면 보유량 표시와 카드 구매 가능 상태를 갱신한다.
//! \param NewMeso 변경된 메소 보유량
//! \return 없음
void UMyDungeonRevivePanelWidget::HandleMesoChanged(int32 NewMeso)
{
	(void)NewMeso;
	RefreshAffordability();
}

////////////////////////////
//! \author 장효제
//! \brief 소유 플레이어의 PlayerState에서 인벤토리 컴포넌트를 찾아 메소 변경을 구독한다.
//! \param 없음
//! \return 구독에 성공하면 true
bool UMyDungeonRevivePanelWidget::BindToInventoryComponent()
{
	if (BoundInventoryComponent)
	{
		return true;
	}

	const APlayerController* OwningPC = GetOwningPlayer();
	const APlayerState* OwningPS = OwningPC ? OwningPC->PlayerState : nullptr;
	UMyInventoryComponent* InventoryComponent = OwningPS ? OwningPS->FindComponentByClass<UMyInventoryComponent>() : nullptr;
	if (!InventoryComponent)
	{
		return false;
	}

	BoundInventoryComponent = InventoryComponent;
	BoundInventoryComponent->OnMesoChanged.AddUniqueDynamic(this, &UMyDungeonRevivePanelWidget::HandleMesoChanged);
	return true;
}

////////////////////////////
//! \author 장효제
//! \brief 인벤토리 컴포넌트 구독을 해제한다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::UnbindFromInventoryComponent()
{
	if (!BoundInventoryComponent)
	{
		return;
	}

	BoundInventoryComponent->OnMesoChanged.RemoveDynamic(this, &UMyDungeonRevivePanelWidget::HandleMesoChanged);
	BoundInventoryComponent = nullptr;
}

////////////////////////////
//! \author 장효제
//! \brief DungeonGS에 지정된 부활 데이터 에셋을 반환한다.
//! \param 없음
//! \return 부활 데이터 에셋, 없으면 nullptr
UDungeonReviveDataAsset* UMyDungeonRevivePanelWidget::GetReviveData() const
{
	const UWorld* World = GetWorld();
	ADungeonGS* DungeonGS = World ? World->GetGameState<ADungeonGS>() : nullptr;
	return DungeonGS ? DungeonGS->GetReviveData() : nullptr;
}

////////////////////////////
//! \author 장효제
//! \brief 활성화된 부활 옵션을 SortOrder 순으로 정렬해 카드 위젯으로 채운다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::BuildOptionCards()
{
	if (!HB_ReviveOptions)
	{
		return;
	}

	HB_ReviveOptions->ClearChildren();
	OptionWidgets.Reset();

	if (!OptionWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Revive UI] OptionWidgetClass is not set. Assign WBP_DungeonReviveOption in %s class defaults."), *GetNameSafe(GetClass()));
		return;
	}

	const UDungeonReviveDataAsset* ReviveData = GetReviveData();
	if (!ReviveData)
	{
		// 서버에서 /revive 목록이 나오는데 여기서 null이면 클라이언트의 GameState가 ADungeonGS가 아니거나
		// BP_DungeonGS 디폴트가 아닌 곳에서 ReviveData를 지정한 것이다.
		UE_LOG(LogTemp, Warning, TEXT("[Revive UI] ReviveData is null on this client. Check BP_DungeonGS defaults. GameState: %s"),
			*GetNameSafe(GetWorld() ? GetWorld()->GetGameState() : nullptr));
		return;
	}

	TArray<const FDungeonReviveOption*> EnabledOptions;
	for (const FDungeonReviveOption& Option : ReviveData->Options)
	{
		if (Option.bEnabled && !Option.OptionId.IsNone())
		{
			EnabledOptions.Add(&Option);
		}
	}

	EnabledOptions.Sort([](const FDungeonReviveOption& Left, const FDungeonReviveOption& Right)
		{
			return Left.SortOrder < Right.SortOrder;
		});

	for (const FDungeonReviveOption* Option : EnabledOptions)
	{
		UMyDungeonReviveOptionWidget* OptionWidget = CreateWidget<UMyDungeonReviveOptionWidget>(this, OptionWidgetClass);
		if (!OptionWidget)
		{
			continue;
		}

		OptionWidget->SetReviveOption(*Option);
		OptionWidget->OnOptionSelected.AddUniqueDynamic(this, &UMyDungeonRevivePanelWidget::HandleOptionSelected);
		OptionWidgets.Add(OptionWidget);

		// 카드 간격과 정렬은 WBP_DungeonRevivePanel의 HB_ReviveOptions 슬롯 설정을 그대로 쓴다.
		HB_ReviveOptions->AddChildToHorizontalBox(OptionWidget);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 현재 메소 보유량으로 각 카드의 선택 가능 상태와 보유 메소 표시를 갱신한다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::RefreshAffordability()
{
	const int32 CurrentMeso = BoundInventoryComponent ? BoundInventoryComponent->GetMeso() : 0;

	const UDungeonReviveDataAsset* ReviveData = GetReviveData();
	for (const TObjectPtr<UMyDungeonReviveOptionWidget>& OptionWidget : OptionWidgets)
	{
		if (!OptionWidget)
		{
			continue;
		}

		const FDungeonReviveOption* Option = ReviveData
			? ReviveData->FindOption(OptionWidget->GetOptionId(), true)
			: nullptr;
		OptionWidget->SetAffordable(Option == nullptr || Option->MesoCost <= CurrentMeso);
	}
}

////////////////////////////
//! \author 장효제
//! \brief 남은 선택 시간 표시를 갱신하고 제한 시간이 끝나면 자동 선택을 실행한다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::UpdateSelectionCountdown()
{
	if (!bSelectionCountdownActive)
	{
		return;
	}

	const UDungeonReviveDataAsset* ReviveData = GetReviveData();
	const UWorld* World = GetWorld();
	const float SelectionTimeoutSeconds = ReviveData ? ReviveData->SelectionTimeoutSeconds : 0.0f;
	const float RemainingSeconds = FMath::Max(0.0f, SelectionDeadlineSeconds - World->GetTimeSeconds());

	SetStatusText(FText::Format(SelectionTimeoutFormat, FText::AsNumber(FMath::CeilToInt(RemainingSeconds))));

	if (PB_SelectionTimeout && SelectionTimeoutSeconds > 0.0f)
	{
		PB_SelectionTimeout->SetPercent(RemainingSeconds / SelectionTimeoutSeconds);
	}

	if (RemainingSeconds <= 0.0f)
	{
		bSelectionCountdownActive = false;
		RequestAutoSelectOption();
	}
}

////////////////////////////
//! \author 장효제
//! \brief 데이터에 지정된 자동 선택 옵션으로 부활을 요청한다. 지정이 없으면 아무것도 하지 않는다.
//! \param 없음
//! \return 없음
void UMyDungeonRevivePanelWidget::RequestAutoSelectOption()
{
	const UDungeonReviveDataAsset* ReviveData = GetReviveData();
	const FDungeonReviveOption* AutoSelectOption = ReviveData
		? ReviveData->FindOption(ReviveData->DefaultAutoSelectOptionId, true)
		: nullptr;
	if (!AutoSelectOption || !AutoSelectOption->bCanAutoSelect)
	{
		UE_LOG(LogTemp, Log, TEXT("[Revive UI] Selection timeout elapsed without a usable auto select option."));
		return;
	}

	HandleOptionSelected(AutoSelectOption->OptionId);
}

//////////////////////////////////////////////////////////////////////
// - 장효제 -
// 상태 텍스트가 WBP에 있을 때만 문구를 갱신하는 함수
// StatusText : 표시할 문구
void UMyDungeonRevivePanelWidget::SetStatusText(const FText& StatusText)
{
	if (TXT_Status)
	{
		TXT_Status->SetText(StatusText);
	}
}
