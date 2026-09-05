////////////////////////////
//! \page MyShopActor.h
//! \brief 월드에 배치하는 상점 액터 선언 파일이다. 상호작용 시 상점 UI를 열고, 서버 구매 검증용 상호작용 상태를 추적한다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dungeon/Interactable/Components/InteractableComponent.h"
#include "MyShopActor.generated.h"

class UMyShopWidget;
class UAnimMontage;
class UStaticMeshComponent;

////////////////////////////
//! \class AMyShopActor
//! \brief F키 상호작용으로 상점 UI(Menu 레이어)를 여는 상점 액터이다.
//!        판매 목록은 별도 지정 없이 아이템 데이터테이블의 BuyPrice > 0인 모든 아이템이다.
//! \note 3인 멀티 기준: UI는 상호작용한 플레이어의 로컬에만 뜨고, 여러 명이 동시에 이용할 수 있다.
//!       서버는 상호작용 중인 플레이어 목록을 추적하고, 구매 RPC는 IsInteractorActive로
//!       "실제로 이 상점과 상호작용 중인가"를 검증한다(거리 검증은 상호작용 시스템이 이미 수행).
//!       BP에서 ShopWidgetClass를 지정하고 레벨에 배치해 사용한다.
UCLASS()
class PROJECTP_API AMyShopActor : public AActor
{
	GENERATED_BODY()

public:
	AMyShopActor();

	//! [서버] 해당 액터(플레이어 폰)가 현재 이 상점과 상호작용 중인지 반환한다. 구매 RPC 검증용.
	bool IsInteractorActive(const AActor* Interactor) const;

	//! [로컬] 구매 성공 시 상점 NPC의 성공 반응 몽타주를 재생한다.
	void PlayPurchaseMontage();

	//! [로컬] 구매 실패 시 상점 NPC의 실패 반응 몽타주를 재생한다.
	void PlayPurchaseFailedMontage();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UInteractableComponent> InteractableComponent;

	//! 상호작용 시 Menu 레이어에 푸시할 상점 창 위젯 클래스 (BP 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UMyShopWidget> ShopWidgetClass;

	//! 구매 성공 시 재생할 상점 NPC 반응 몽타주 (BP 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Animation")
	TObjectPtr<UAnimMontage> PurchaseMontage;

	//! 구매 실패 시 재생할 상점 NPC 반응 몽타주 (BP 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop|Animation")
	TObjectPtr<UAnimMontage> PurchaseFailedMontage;

private:
	UFUNCTION()
	void HandleLocalInteractionStarted(const FInteractionStartContext& Context);

	UFUNCTION()
	void HandleLocalInteractionEnded(AActor* Interactor);

	//! 이 클라이언트에 떠 있는 상점 창. 상호작용 종료 통지 시 닫는 데 사용한다.
	TWeakObjectPtr<UMyShopWidget> ActiveShopWidget;
};
