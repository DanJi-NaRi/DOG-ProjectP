#include "CPP_BossAIController.h"

#include "Boss/Core/CPP_BossCharacter.h"

ACPP_BossAIController::ACPP_BossAIController()
{
}

void ACPP_BossAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledBoss = Cast<ACPP_BossCharacter>(InPawn);
}

void ACPP_BossAIController::OnUnPossess()
{
	ControlledBoss = nullptr;

	Super::OnUnPossess();
}

ACPP_BossCharacter* ACPP_BossAIController::GetControlledBoss() const
{
	return ControlledBoss;
}
