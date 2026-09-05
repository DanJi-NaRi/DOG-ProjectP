#include "MyDamageFloatingLayerWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetComponent.h"
#include "MyDamageNumberStackWidget.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 전용 데미지 플로팅 레이어의 루트 캔버스를 생성하는 함수
// Return Value : 위젯 초기화 성공 여부
bool UMyDamageFloatingLayerWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!WidgetTree)
	{
		return false;
	}

	RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("DamageFloatingRootCanvas"));
		WidgetTree->RootWidget = RootCanvas;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return RootCanvas != nullptr;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 대상의 기존 네임플레이트 사양을 사용해 로컬 플레이어의 데미지 숫자를 추가하는 함수
// TargetActor : 데미지를 받은 몬스터
// SourceWidgetComponent : 기존 네임플레이트를 표시하는 위젯 컴포넌트
// DamageAmount : 서버가 확정한 최종 데미지
// DamageType : 일반, 치명타 또는 DOT 표시 타입
// bKilledTarget : 이번 데미지로 대상을 처치했는지 여부
void UMyDamageFloatingLayerWidget::PushDamageNumber(
	AActor* TargetActor,
	UWidgetComponent* SourceWidgetComponent,
	float DamageAmount,
	EDamageNumberDisplayType DamageType,
	bool bKilledTarget)
{
	if (!IsValid(TargetActor) || !IsValid(SourceWidgetComponent))
	{
		return;
	}

	FDamageFloatingTargetEntry* Entry = FindTargetEntry(TargetActor);
	if (!Entry)
	{
		Entry = CreateTargetEntry(TargetActor, SourceWidgetComponent);
	}

	if (Entry && Entry->DamageNumberStack)
	{
		Entry->DamageNumberStack->PushTypedDamage(DamageAmount, DamageType);

		if (bKilledTarget)
		{
			// 사망 래그돌과 넉백이 시작되기 전 마지막 정상 위치에서 숫자 재생을 이어간다.
			Entry->bFollowSourceWidget = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 레이어가 보관 중인 모든 데미지 플로팅 UI를 정리하는 함수
void UMyDamageFloatingLayerWidget::ClearDamageNumbers()
{
	for (FDamageFloatingTargetEntry& Entry : TargetEntries)
	{
		if (Entry.HostWidget)
		{
			Entry.HostWidget->RemoveFromParent();
		}
	}

	TargetEntries.Reset();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 표시 중인 숫자의 위치를 갱신하고 재생이 끝난 대상 UI를 제거하는 함수
// MyGeometry : 위젯 지오메트리
// InDeltaTime : 프레임 간격
void UMyDamageFloatingLayerWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	for (int32 EntryIndex = TargetEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FDamageFloatingTargetEntry& Entry = TargetEntries[EntryIndex];
		if (!Entry.DamageNumberStack || !Entry.DamageNumberStack->HasActiveEntries())
		{
			RemoveTargetEntry(EntryIndex);
			continue;
		}

		if (Entry.bFollowSourceWidget)
		{
			if (UWidgetComponent* SourceWidgetComponent = Entry.SourceWidgetComponent.Get())
			{
				Entry.LastWorldLocation = SourceWidgetComponent->GetComponentLocation();
			}
		}

		UpdateTargetEntryPosition(Entry, PlayerController);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 지정한 몬스터에 대응하는 데미지 플로팅 상태를 찾는 함수
// TargetActor : 찾을 몬스터
// Return Value : 대상 상태의 포인터, 찾지 못하면 nullptr
FDamageFloatingTargetEntry* UMyDamageFloatingLayerWidget::FindTargetEntry(
	AActor* TargetActor)
{
	return TargetEntries.FindByPredicate(
		[TargetActor](const FDamageFloatingTargetEntry& Entry)
		{
			return Entry.TargetActor.Get() == TargetActor;
		});
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 기존 네임플레이트와 같은 위젯 클래스로 독립 데미지 플로팅 상태를 생성하는 함수
// TargetActor : 데미지를 받은 몬스터
// SourceWidgetComponent : 기존 네임플레이트를 표시하는 위젯 컴포넌트
// Return Value : 생성된 대상 상태의 포인터, 생성하지 못하면 nullptr
FDamageFloatingTargetEntry* UMyDamageFloatingLayerWidget::CreateTargetEntry(
	AActor* TargetActor,
	UWidgetComponent* SourceWidgetComponent)
{
	if (!RootCanvas)
	{
		return nullptr;
	}

	UUserWidget* SourceHostWidget = SourceWidgetComponent->GetWidget();
	if (!SourceHostWidget)
	{
		return nullptr;
	}

	UUserWidget* HostWidget = CreateWidget<UUserWidget>(
		GetOwningPlayer(),
		SourceHostWidget->GetClass());
	if (!HostWidget)
	{
		return nullptr;
	}

	UMyDamageNumberStackWidget* DamageNumberStack =
		Cast<UMyDamageNumberStackWidget>(
			HostWidget->GetWidgetFromName(TEXT("DamageNumberStack")));
	if (!DamageNumberStack)
	{
		return nullptr;
	}

	HideWidgetsExceptDamageStack(HostWidget, DamageNumberStack);

	UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(HostWidget);
	if (!CanvasSlot)
	{
		return nullptr;
	}

	const FVector2D DrawSize = SourceWidgetComponent->GetDrawSize();
	const FVector2D Pivot = SourceWidgetComponent->GetPivot();

	CanvasSlot->SetAutoSize(false);
	CanvasSlot->SetSize(DrawSize);
	CanvasSlot->SetAlignment(Pivot);

	HostWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	FDamageFloatingTargetEntry& NewEntry = TargetEntries.AddDefaulted_GetRef();
	NewEntry.TargetActor = TargetActor;
	NewEntry.SourceWidgetComponent = SourceWidgetComponent;
	NewEntry.HostWidget = HostWidget;
	NewEntry.DamageNumberStack = DamageNumberStack;
	NewEntry.LastWorldLocation = SourceWidgetComponent->GetComponentLocation();
	NewEntry.DrawSize = DrawSize;
	NewEntry.Pivot = Pivot;

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		UpdateTargetEntryPosition(NewEntry, PlayerController);
	}

	return &NewEntry;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 네임플레이트 레이아웃은 유지하면서 데미지 스택 이외의 형제 위젯만 숨기는 함수
// HostWidget : 복제해서 생성한 네임플레이트 위젯
// DamageNumberStack : 계속 표시할 데미지 숫자 스택
void UMyDamageFloatingLayerWidget::HideWidgetsExceptDamageStack(
	UUserWidget* HostWidget,
	UMyDamageNumberStackWidget* DamageNumberStack) const
{
	if (!HostWidget || !DamageNumberStack)
	{
		return;
	}

	UWidget* ChildOnPath = DamageNumberStack;
	while (UPanelWidget* ParentPanel = Cast<UPanelWidget>(ChildOnPath->GetParent()))
	{
		for (int32 ChildIndex = 0; ChildIndex < ParentPanel->GetChildrenCount(); ++ChildIndex)
		{
			UWidget* SiblingWidget = ParentPanel->GetChildAt(ChildIndex);
			if (SiblingWidget && SiblingWidget != ChildOnPath)
			{
				// Hidden은 레이아웃 공간을 유지하므로 기존 데미지 숫자의 화면 위치가 변하지 않는다.
				SiblingWidget->SetVisibility(ESlateVisibility::Hidden);
			}
		}

		ChildOnPath = ParentPanel;
	}

	DamageNumberStack->SetVisibility(ESlateVisibility::HitTestInvisible);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 대상의 마지막 월드 위치를 화면 좌표로 투영해 독립 UI의 위치를 갱신하는 함수
// Entry : 위치를 갱신할 대상 상태
// PlayerController : 월드 위치를 화면으로 투영할 로컬 플레이어 컨트롤러
void UMyDamageFloatingLayerWidget::UpdateTargetEntryPosition(
	FDamageFloatingTargetEntry& Entry,
	APlayerController* PlayerController) const
{
	if (!Entry.HostWidget || !PlayerController)
	{
		return;
	}

	FVector2D WidgetPosition;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		Entry.LastWorldLocation,
		WidgetPosition,
		false);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Entry.HostWidget->Slot))
	{
		if (bProjected)
		{
			CanvasSlot->SetPosition(WidgetPosition);
		}
	}

	// 투영 실패 중에도 위젯을 Visible 상태로 유지해야 숫자 애니메이션 시간이 계속 진행된다.
	Entry.HostWidget->SetRenderOpacity(bProjected ? 1.0f : 0.0f);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재생이 끝난 대상의 독립 UI와 런타임 상태를 제거하는 함수
// EntryIndex : 제거할 대상 상태의 배열 인덱스
void UMyDamageFloatingLayerWidget::RemoveTargetEntry(int32 EntryIndex)
{
	if (!TargetEntries.IsValidIndex(EntryIndex))
	{
		return;
	}

	if (TargetEntries[EntryIndex].HostWidget)
	{
		TargetEntries[EntryIndex].HostWidget->RemoveFromParent();
	}

	TargetEntries.RemoveAtSwap(EntryIndex);
}
