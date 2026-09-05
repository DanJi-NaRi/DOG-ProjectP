#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CPP_BossAIController.generated.h"

class ACPP_BossCharacter;

UCLASS()
class PROJECTP_API ACPP_BossAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACPP_BossAIController();

	UFUNCTION(BlueprintPure, Category = "Boss|AI")
	ACPP_BossCharacter* GetControlledBoss() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACPP_BossCharacter> ControlledBoss;
};
