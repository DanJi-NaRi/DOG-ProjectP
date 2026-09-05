#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_BreachChanceObject.generated.h"

class USceneComponent;

////////////////////////////
//! \class ACPP_BreachChanceObject
//! \brief 돌파 저지에서 남은 기회 하나를 표현하는 범용 복제 액터. 외형과 연출은 Blueprint 자식이 담당한다.
UCLASS(Blueprintable)
class PROJECTP_API ACPP_BreachChanceObject : public AActor
{
	GENERATED_BODY()

public:
	ACPP_BreachChanceObject();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Breach Chance|State")
	void ConsumeChance();

	UFUNCTION(BlueprintCallable, Category = "Breach Chance|State")
	void RestoreChance();

	UFUNCTION(BlueprintPure, Category = "Breach Chance|State")
	bool IsConsumed() const { return bConsumed; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breach Chance|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UFUNCTION(BlueprintImplementableEvent, Category = "Breach Chance|Visual")
	void BP_ApplyConsumedState();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breach Chance|Visual")
	void BP_ApplyRestoredState();

	UFUNCTION(BlueprintImplementableEvent, Category = "Breach Chance|Visual")
	void BP_PlayConsumeEffects();

private:
	UFUNCTION()
	void OnRep_Consumed();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayConsumeEffects();

	UPROPERTY(ReplicatedUsing = OnRep_Consumed, VisibleInstanceOnly, BlueprintReadOnly, Category = "Breach Chance|State", meta = (AllowPrivateAccess = "true"))
	bool bConsumed = false;
};
