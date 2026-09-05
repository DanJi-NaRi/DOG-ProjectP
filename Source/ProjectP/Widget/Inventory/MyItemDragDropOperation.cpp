#include "Widget/Inventory/MyItemDragDropOperation.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 드래그 취소가 처리되지 않은 마우스 드롭인지 확인한 뒤 기본 취소 델리게이트를 호출하는 함수
// PointerEvent : 드래그가 종료될 때 전달된 포인터 입력 이벤트
void UMyItemDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
    bWasDroppedOutsideByPointer = PointerEvent.GetEffectingButton().IsMouseButton();
    Super::DragCancelled_Implementation(PointerEvent);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 퀵슬롯 밖에 마우스로 드롭해서 취소 콜백이 발생했는지 반환하는 함수
// 반환값 : 처리되지 않은 마우스 드롭이면 true, Esc 등 명시적인 취소이면 false
bool UMyItemDragDropOperation::WasDroppedOutsideByPointer() const
{
    return bWasDroppedOutsideByPointer;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 원본 슬롯 크기의 투명 영역 중앙에 아이템 아이콘을 배치한 공통 드래그 비주얼을 생성하는 함수
// Outer : 생성할 위젯들의 Outer 객체
// IconTexture : 드래그 중 표시할 아이템 아이콘
// IconSize : 화면에 보이는 아이콘 크기
// SourceWidgetSize : 드래그를 시작한 원본 슬롯 위젯 크기
// 반환값 : 생성된 드래그 비주얼, 입력이 유효하지 않으면 nullptr
UWidget* UMyItemDragDropOperation::CreateItemDragVisual(
    UObject* Outer,
    UTexture2D* IconTexture,
    const FVector2D& IconSize,
    const FVector2D& SourceWidgetSize)
{
    if (!Outer || !IconTexture)
    {
        return nullptr;
    }

    const FVector2D DragRootSize(
        FMath::Max(SourceWidgetSize.X, IconSize.X),
        FMath::Max(SourceWidgetSize.Y, IconSize.Y));

    USizeBox* DragRoot = NewObject<USizeBox>(Outer);
    DragRoot->SetWidthOverride(DragRootSize.X);
    DragRoot->SetHeightOverride(DragRootSize.Y);

    UImage* DragIcon = NewObject<UImage>(DragRoot);
    FSlateBrush IconBrush;
    IconBrush.SetResourceObject(IconTexture);
    IconBrush.ImageSize = IconSize;
    DragIcon->SetBrush(IconBrush);

    if (USizeBoxSlot* IconSlot = Cast<USizeBoxSlot>(DragRoot->AddChild(DragIcon)))
    {
        IconSlot->SetHorizontalAlignment(HAlign_Center);
        IconSlot->SetVerticalAlignment(VAlign_Center);
    }

    return DragRoot;
}
