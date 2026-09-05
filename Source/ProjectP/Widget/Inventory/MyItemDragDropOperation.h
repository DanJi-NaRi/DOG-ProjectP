////////////////////////////
//! \page MyItemDragDropOperation.h
//! \brief 인벤토리 슬롯에서 시작하는 아이템 드래그앤드랍 페이로드(아이템 ID) 선언 파일이다.
#pragma once

#include "Blueprint/DragDropOperation.h"
#include "MyItemDragDropOperation.generated.h"

class UTexture2D;
class UWidget;

////////////////////////////
//! \class UMyItemDragDropOperation
//! \brief 드래그 중인 아이템 ID를 드랍 대상(HUD 퀵슬롯 등)에 전달하는 오퍼레이션이다.
UCLASS()
class PROJECTP_API UMyItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
    virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

    bool WasDroppedOutsideByPointer() const;

    static UWidget* CreateItemDragVisual(
        UObject* Outer,
        UTexture2D* IconTexture,
        const FVector2D& IconSize,
        const FVector2D& SourceWidgetSize);

	//! 드래그 중인 아이템 ID (ItemDataTable Row Name)
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

    //! 드래그 중인 아이템 아이콘. 빈 퀵슬롯의 드롭 미리보기에 사용한다.
    UPROPERTY(BlueprintReadOnly, Category = "Item")
    TObjectPtr<UTexture2D> ItemIcon;

    //! 드래그를 시작한 퀵슬롯 인덱스. 인벤토리에서 시작했으면 INDEX_NONE이다.
    UPROPERTY(BlueprintReadOnly, Category = "Item")
    int32 SourceQuickSlotIndex = INDEX_NONE;

private:
    //! 처리되지 않은 마우스 드롭으로 DragCancelled가 발생했는지 여부
    bool bWasDroppedOutsideByPointer = false;
};
