#pragma once

#include "NativeGameplayTags.h"

namespace BossGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_2);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_State_Locked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_State_Dead);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Encounter_PhaseTransition);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Encounter_FinalJudgment);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Boss_AttackWindow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Boss_Dash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Boss_Telegraph_Begin);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Boss_Telegraph_End);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Curse);
}
