//////////////////////////////////////////////////////////////////////
// - Codex -
// 오벨리스크 상호작용을 상점 활성화와 Shop Zone 클리어로 연결하는 컴포넌트 선언
#pragma once

#include "CoreMinimal.h"
#include "ClearComponent.h"
#include "CPP_ClearComponent_ShopActivated.generated.h"

class ACPP_ObeliskActor;
class AMyShopActor;
class UInteractableComponent;

UCLASS(Blueprintable, ClassGroup = (Zone), meta = (BlueprintSpawnableComponent, DisplayName = "Clear Component (Shop Activated)"))
class PROJECTP_API UCPP_ClearComponent_ShopActivated : public UClearComponent
{
    GENERATED_BODY()

public:
    virtual void ActivateClearCondition_Implementation() override;
    virtual void DeactivateClearCondition_Implementation() override;
    virtual void ResetClearCondition_Implementation() override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zone|Clear|Shop")
    TObjectPtr<ACPP_ObeliskActor> TriggerObelisk;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Zone|Clear|Shop")
    TArray<TObjectPtr<AMyShopActor>> ShopActors;

private:
    UFUNCTION()
    void HandleObeliskTriggered(AActor* InInstigator);

    bool HasZoneAuthority() const;
    UInteractableComponent* GetObeliskInteractable() const;
    void SetObeliskInteractionEnabled(bool bEnabled) const;
    void SetShopInteractionEnabled(bool bEnabled) const;
    void UnbindObelisk();
};
