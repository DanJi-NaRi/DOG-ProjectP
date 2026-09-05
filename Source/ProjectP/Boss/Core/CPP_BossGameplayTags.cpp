#include "CPP_BossGameplayTags.h"

namespace BossGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Boss_Phase_1, "Boss.Phase.1");
	UE_DEFINE_GAMEPLAY_TAG(Boss_Phase_2, "Boss.Phase.2");

	UE_DEFINE_GAMEPLAY_TAG(Boss_State_Locked, "Boss.State.Locked");
	UE_DEFINE_GAMEPLAY_TAG(Boss_State_Dead, "Boss.State.Dead");

	UE_DEFINE_GAMEPLAY_TAG(Boss_Encounter_PhaseTransition, "Boss.Encounter.PhaseTransition");
	UE_DEFINE_GAMEPLAY_TAG(Boss_Encounter_FinalJudgment, "Boss.Encounter.FinalJudgment");

	UE_DEFINE_GAMEPLAY_TAG(Event_Boss_AttackWindow, "Event.Boss.AttackWindow");
	UE_DEFINE_GAMEPLAY_TAG(Event_Boss_Dash, "Event.Boss.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Event_Boss_Telegraph_Begin, "Event.Boss.Telegraph.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Event_Boss_Telegraph_End, "Event.Boss.Telegraph.End");

	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Curse, "Status.Debuff.Curse");
}
