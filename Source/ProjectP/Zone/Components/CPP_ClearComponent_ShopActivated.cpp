//////////////////////////////////////////////////////////////////////
// - Codex -
// 오벨리스크 상호작용을 상점 활성화와 Shop Zone 클리어로 연결하는 컴포넌트 구현
#include "CPP_ClearComponent_ShopActivated.h"

#include "Dungeon/Dialogue/CPP_ObeliskActor.h"
#include "Dungeon/Interactable/Components/InteractableComponent.h"
#include "GameFramework/Actor.h"
#include "Shop/MyShopActor.h"

//////////////////////////////////////////////////////////////////////
// - Codex -
// Shop Zone 활성화 전에는 오벨리스크와 상점의 상호작용을 잠그는 함수
void UCPP_ClearComponent_ShopActivated::BeginPlay()
{
    Super::BeginPlay();

    if (!HasZoneAuthority())
    {
        return;
    }

    SetObeliskInteractionEnabled(false);
    SetShopInteractionEnabled(false);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Shop Zone이 Active 상태가 되면 오벨리스크 이벤트를 구독하고 상호작용을 허용하는 함수
void UCPP_ClearComponent_ShopActivated::ActivateClearCondition_Implementation()
{
    Super::ActivateClearCondition_Implementation();

    if (!HasZoneAuthority())
    {
        return;
    }

    if (!IsValid(TriggerObelisk))
    {
        UE_LOG(LogTemp, Error, TEXT("[ShopZone] TriggerObelisk가 지정되지 않아 Shop Zone을 진행할 수 없습니다. Zone: %s"),
            *GetNameSafe(GetOwner()));
        return;
    }

    if (!GetObeliskInteractable())
    {
        UE_LOG(LogTemp, Error, TEXT("[ShopZone] TriggerObelisk에 InteractableComponent가 없습니다. Zone: %s, Obelisk: %s"),
            *GetNameSafe(GetOwner()),
            *GetNameSafe(TriggerObelisk));
        return;
    }

    TriggerObelisk->OnGimmickTriggered.AddUniqueDynamic(
        this,
        &UCPP_ClearComponent_ShopActivated::HandleObeliskTriggered);

    SetObeliskInteractionEnabled(true);

    if (TriggerObelisk->IsGimmickTriggered())
    {
        HandleObeliskTriggered(nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Shop Zone의 클리어 감시가 끝나면 오벨리스크 이벤트와 상호작용을 정리하는 함수
void UCPP_ClearComponent_ShopActivated::DeactivateClearCondition_Implementation()
{
    Super::DeactivateClearCondition_Implementation();

    if (!HasZoneAuthority())
    {
        return;
    }

    UnbindObelisk();
    SetObeliskInteractionEnabled(false);
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// Shop Zone 재시작을 위해 오벨리스크와 상점의 상호작용 상태를 초기화하는 함수
void UCPP_ClearComponent_ShopActivated::ResetClearCondition_Implementation()
{
    Super::ResetClearCondition_Implementation();

    if (!HasZoneAuthority())
    {
        return;
    }

    UnbindObelisk();

    if (UInteractableComponent* ObeliskInteractable = GetObeliskInteractable())
    {
        ObeliskInteractable->SetInteractionEnabled(false);
        ObeliskInteractable->ResetInteractionState();
    }

    if (IsValid(TriggerObelisk))
    {
        TriggerObelisk->ResetGimmickTrigger();
    }

    for (AMyShopActor* ShopActor : ShopActors)
    {
        if (!IsValid(ShopActor))
        {
            continue;
        }

        if (UInteractableComponent* ShopInteractable = ShopActor->FindComponentByClass<UInteractableComponent>())
        {
            ShopInteractable->SetInteractionEnabled(false);
            ShopInteractable->ResetInteractionState();
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 오벨리스크 상호작용 시 상점들을 활성화하고 Shop Zone 클리어를 통보하는 함수
// InInstigator : 오벨리스크와 상호작용한 플레이어 액터
void UCPP_ClearComponent_ShopActivated::HandleObeliskTriggered(AActor* /*InInstigator*/)
{
    if (!HasZoneAuthority() || !IsClearConditionActive())
    {
        return;
    }

    SetShopInteractionEnabled(true);
    MarkClearSatisfied();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 소유한 Shop Zone이 서버 권한을 가지고 있는지 확인하는 함수
// 반환값 : 서버 권한을 가지고 있으면 true
bool UCPP_ClearComponent_ShopActivated::HasZoneAuthority() const
{
    const AActor* OwnerActor = GetOwner();
    return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 지정된 오벨리스크에서 상호작용 컴포넌트를 찾는 함수
// 반환값 : 오벨리스크의 상호작용 컴포넌트, 찾지 못하면 nullptr
UInteractableComponent* UCPP_ClearComponent_ShopActivated::GetObeliskInteractable() const
{
    return IsValid(TriggerObelisk)
        ? TriggerObelisk->FindComponentByClass<UInteractableComponent>()
        : nullptr;
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 오벨리스크의 상호작용 가능 상태를 서버에서 변경하는 함수
// bEnabled : 상호작용 허용 여부
void UCPP_ClearComponent_ShopActivated::SetObeliskInteractionEnabled(bool bEnabled) const
{
    if (UInteractableComponent* ObeliskInteractable = GetObeliskInteractable())
    {
        ObeliskInteractable->SetInteractionEnabled(bEnabled);
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 등록된 모든 상점의 상호작용 가능 상태를 서버에서 변경하는 함수
// bEnabled : 상호작용 허용 여부
void UCPP_ClearComponent_ShopActivated::SetShopInteractionEnabled(bool bEnabled) const
{
    for (AMyShopActor* ShopActor : ShopActors)
    {
        if (!IsValid(ShopActor))
        {
            continue;
        }

        if (UInteractableComponent* ShopInteractable = ShopActor->FindComponentByClass<UInteractableComponent>())
        {
            ShopInteractable->SetInteractionEnabled(bEnabled);
        }
    }
}

//////////////////////////////////////////////////////////////////////
// - Codex -
// 오벨리스크의 기믹 트리거 이벤트 구독을 해제하는 함수
void UCPP_ClearComponent_ShopActivated::UnbindObelisk()
{
    if (IsValid(TriggerObelisk))
    {
        TriggerObelisk->OnGimmickTriggered.RemoveDynamic(
            this,
            &UCPP_ClearComponent_ShopActivated::HandleObeliskTriggered);
    }
}
