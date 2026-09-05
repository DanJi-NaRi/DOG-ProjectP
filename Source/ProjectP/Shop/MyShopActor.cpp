////////////////////////////
//! \page MyShopActor.cpp
//! \brief 상점 액터 구현 파일이다.
//! \editor 준혁 - 상호작용 상태 관리 설계 적용: 자체 상호작용자 목록을 컴포넌트의 승인 목록으로 대체,
//!         Context 기반 델리게이트로 마이그레이션, 복제 액터로 통일
#include "Shop/MyShopActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MyGameplayTags.h"
#include "Widget/MyUIManagerSubsystem.h"
#include "Widget/Shop/MyShopWidget.h"

////////////////////////////
//! \author 준혁
//! \brief 상점 액터를 생성한다. 메시와 상호작용 컴포넌트를 붙인다.
//!        상호작용 상태 복제를 위해 복제 액터로 통일한다(향후 동적 활성화 대비).
//! \param 없음
//! \return 없음
AMyShopActor::AMyShopActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	InteractableComponent = CreateDefaultSubobject<UInteractableComponent>(TEXT("InteractableComponent"));
}

////////////////////////////
//! \author 준혁
//! \brief 상호작용 로컬 시작/종료 이벤트에 UI 열기/닫기 핸들러를 바인딩한다.
//!        서버 측 상호작용자 추적은 InteractableComponent가 담당하므로 별도 바인딩이 필요 없다.
//! \param 없음
//! \return 없음
void AMyShopActor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractableComponent)
	{
		InteractableComponent->OnLocalInteractionStarted.AddUniqueDynamic(this, &AMyShopActor::HandleLocalInteractionStarted);
		InteractableComponent->OnLocalInteractionEnded.AddUniqueDynamic(this, &AMyShopActor::HandleLocalInteractionEnded);
	}
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 자체 목록 대신 컴포넌트의 승인된 활성 상호작용자 목록으로 판정하도록 변경
//! \brief [서버] 해당 액터가 현재 이 상점과 상호작용 중인지 반환한다. 구매 RPC 검증에 사용된다.
//! \param Interactor 검사할 플레이어 폰
//! \return 상호작용 중이면 true
bool AMyShopActor::IsInteractorActive(const AActor* Interactor) const
{
	return InteractableComponent && InteractableComponent->IsInteractorActive(Interactor);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 구매를 요청한 클라이언트의 상점 NPC에서 성공 반응 몽타주를 재생하는 함수
void AMyShopActor::PlayPurchaseMontage()
{
	if (!PurchaseMontage)
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* AnimInstance = SkeletalMeshComponent ? SkeletalMeshComponent->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(PurchaseMontage);
	}
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 구매를 요청한 클라이언트의 상점 NPC에서 실패 반응 몽타주를 재생하는 함수
void AMyShopActor::PlayPurchaseFailedMontage()
{
	if (!PurchaseFailedMontage)
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* AnimInstance = SkeletalMeshComponent ? SkeletalMeshComponent->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(PurchaseFailedMontage);
	}
}

////////////////////////////
//! \author 준혁
//! \brief [로컬] 상호작용 시작 통지 시 상점 창을 Menu 레이어에 푸시한다.
//! \param Context 서버가 승인한 시작 Context (Interactor = 이 클라이언트의 로컬 플레이어)
//! \return 없음
void AMyShopActor::HandleLocalInteractionStarted(const FInteractionStartContext& Context)
{
	const APawn* InteractorPawn = Cast<APawn>(Context.Interactor);
	APlayerController* PlayerController = InteractorPawn ? InteractorPawn->GetController<APlayerController>() : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!ShopWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop] ShopWidgetClass is not set. Assign it on the shop actor BP. Shop: %s"), *GetNameSafe(this));
		return;
	}

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	UMyUIManagerSubsystem* UIManager = LocalPlayer ? LocalPlayer->GetSubsystem<UMyUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		return;
	}

	// 다이얼로그 시작과 동일하게 기존 Menu 레이어를 정리하고 HUD 레이어를 숨긴다.
	UIManager->ClearLayer(MyGameplayTags::UI_Layer_Menu);
	UIManager->SetLayerActive(MyGameplayTags::UI_Layer_HUD, false);
	UIManager->SetLayerVisible(MyGameplayTags::UI_Layer_HUD, false);

	UMyShopWidget* ShopWidget = Cast<UMyShopWidget>(UIManager->PushMenu(ShopWidgetClass));
	if (ShopWidget)
	{
		ShopWidget->InitShop(this);
		ActiveShopWidget = ShopWidget;
		return;
	}

	// 상점 Push에 실패하면 닫힘 콜백이 없으므로 여기서 HUD를 즉시 복원한다.
	UIManager->SetLayerVisible(MyGameplayTags::UI_Layer_HUD, true);
	UIManager->SetLayerActive(MyGameplayTags::UI_Layer_HUD, true);
	UE_LOG(LogTemp, Warning, TEXT("[Shop] Failed to push shop widget. (UI.Layer.Menu stack registered in WBP_PrimaryGameLayout?)"));
}

////////////////////////////
//! \author 준혁
//! \brief [로컬] 상호작용 종료 통지 시 떠 있는 상점 창을 닫는다.
//! \param Interactor 상호작용을 종료한 플레이어 폰
//! \return 없음
void AMyShopActor::HandleLocalInteractionEnded(AActor* Interactor)
{
	if (UMyShopWidget* ShopWidget = ActiveShopWidget.Get())
	{
		ShopWidget->CloseFromInteraction();
	}

	ActiveShopWidget.Reset();
}
