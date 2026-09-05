#pragma once

#include "Blueprint/UserWidget.h"
#include "MyDamageFloatingPreset.h"
#include "MyDamageFloatingLayerWidget.generated.h"

class APlayerController;
class UCanvasPanel;
class UMyDamageNumberStackWidget;
class UUserWidget;
class UWidgetComponent;

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 로컬 플레이어의 데미지 플로팅 대상 한 개를 관리하는 런타임 상태
USTRUCT()
struct FDamageFloatingTargetEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWidgetComponent> SourceWidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HostWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMyDamageNumberStackWidget> DamageNumberStack = nullptr;

	FVector LastWorldLocation = FVector::ZeroVector;
	FVector2D DrawSize = FVector2D::ZeroVector;
	FVector2D Pivot = FVector2D(0.5f, 0.5f);

	// true면 살아 있는 몬스터의 네임플레이트 위치를 계속 추적한다.
	bool bFollowSourceWidget = true;
};

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 몬스터와 독립적으로 데미지 숫자를 보관하고 월드 위치에 투영하는 로컬 전용 UI 레이어
UCLASS()
class PROJECTP_API UMyDamageFloatingLayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//////////////////////////////////////////////////////////////////////
	// - 준혁 -
	// 대상의 기존 네임플레이트 사양을 사용해 로컬 플레이어의 데미지 숫자를 추가하는 함수
	// TargetActor : 데미지를 받은 몬스터
	// SourceWidgetComponent : 기존 네임플레이트를 표시하는 위젯 컴포넌트
	// DamageAmount : 서버가 확정한 최종 데미지
	// DamageType : 일반, 치명타 또는 DOT 표시 타입
	// bKilledTarget : 이번 데미지로 대상을 처치했는지 여부
	void PushDamageNumber(
		AActor* TargetActor,
		UWidgetComponent* SourceWidgetComponent,
		float DamageAmount,
		EDamageNumberDisplayType DamageType,
		bool bKilledTarget);

	//////////////////////////////////////////////////////////////////////
	// - 준혁 -
	// 레이어가 보관 중인 모든 데미지 플로팅 UI를 정리하는 함수
	void ClearDamageNumbers();

	virtual bool Initialize() override;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FDamageFloatingTargetEntry* FindTargetEntry(AActor* TargetActor);
	FDamageFloatingTargetEntry* CreateTargetEntry(
		AActor* TargetActor,
		UWidgetComponent* SourceWidgetComponent);
	void HideWidgetsExceptDamageStack(
		UUserWidget* HostWidget,
		UMyDamageNumberStackWidget* DamageNumberStack) const;
	void UpdateTargetEntryPosition(
		FDamageFloatingTargetEntry& Entry,
		APlayerController* PlayerController) const;
	void RemoveTargetEntry(int32 EntryIndex);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY(Transient)
	TArray<FDamageFloatingTargetEntry> TargetEntries;
};
