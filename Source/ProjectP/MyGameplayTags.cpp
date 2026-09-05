////////////////////////////
//! \page MyGameplayTags.cpp
//! \author 장효제
//! \brief ProjectP Native GameplayTag의 실제 문자열 매핑을 정의한다.
//! \note UE_DEFINE_GAMEPLAY_TAG의 두 번째 인자가 실제 GameplayTag 경로이며, C++ 변수명의 언더스코어는 점으로 자동 변환되지 않는다.

#include "MyGameplayTags.h"

namespace MyGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Faction_Player, "Faction.Player");
	UE_DEFINE_GAMEPLAY_TAG(Faction_Enemy, "Faction.Enemy");
	UE_DEFINE_GAMEPLAY_TAG(Faction_Enemy_Boss, "Faction.Enemy.Boss");
	UE_DEFINE_GAMEPLAY_TAG(Faction_Objective, "Faction.Objective");
	UE_DEFINE_GAMEPLAY_TAG(Target_Destructible, "Target.Destructible");

	// 신 정체성 태그
	UE_DEFINE_GAMEPLAY_TAG(God_Horus, "God.Horus");
	UE_DEFINE_GAMEPLAY_TAG(God_Isis, "God.Isis");
	UE_DEFINE_GAMEPLAY_TAG(God_Anubis, "God.Anubis");
	UE_DEFINE_GAMEPLAY_TAG(God_Thoth, "God.Thoth");
	UE_DEFINE_GAMEPLAY_TAG(God_Hathor, "God.Hathor");
	UE_DEFINE_GAMEPLAY_TAG(God_Sekhmet, "God.Sekhmet");
	UE_DEFINE_GAMEPLAY_TAG(God_Nephthys, "God.Nephthys");
	UE_DEFINE_GAMEPLAY_TAG(God_Ra, "God.Ra");
	UE_DEFINE_GAMEPLAY_TAG(God_Set, "God.Set");

	UE_DEFINE_GAMEPLAY_TAG(AI_Event_Dead, "AI.Event.Dead");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_Hit, "AI.Event.Hit");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_OutRange, "AI.Event.OutRange");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_SeeTarget, "AI.Event.SeeTarget");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_TargetChange, "AI.Event.TargetChange");
	UE_DEFINE_GAMEPLAY_TAG(AI_Event_TargetLost, "AI.Event.TargetLost");

	//캐릭터 (스트리밍 시스템에서 대상을 지시하기 위한 태그)
	UE_DEFINE_GAMEPLAY_TAG(Character_Enemy,"Character.Enemy");
	UE_DEFINE_GAMEPLAY_TAG(Character_Enemy_Boss,"Character.Enemy.Boss");

	//공용 스킬 (어빌리티가 아닌 조작도 스킬 사용 사실로 다룬다)
	UE_DEFINE_GAMEPLAY_TAG(Skill_Common_Jump,"Skill.Common.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Common_Jump_Moving,"Skill.Common.Jump.Moving");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Common_Jump_InPlace,"Skill.Common.Jump.InPlace");
	UE_DEFINE_GAMEPLAY_TAG(Character_Player,"Character.Player");
	UE_DEFINE_GAMEPLAY_TAG(Character_Player_Nefer,"Character.Player.Nefer");
	UE_DEFINE_GAMEPLAY_TAG(Character_Player_Inpu,"Character.Player.Inpu");
	UE_DEFINE_GAMEPLAY_TAG(Character_Player_Heru,"Character.Player.Heru");

	FGameplayTag GetPlayerCharacterTag(int32 SelectedCharacterId)
	{
		switch (SelectedCharacterId)
		{
		case 100:
			return Character_Player_Nefer;
		case 200:
			return Character_Player_Inpu;
		case 300:
			return Character_Player_Heru;
		default:
			return FGameplayTag();
		}
	}

	// 아이템 사용 쿨타임 (같은 태그 = 쿨타임 공유 그룹)
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Item_Potion, "Cooldown.Item.Potion");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Item_Elixir, "Cooldown.Item.Elixir");

	// 버프 계열 태그 (같은 태그 = 덮어쓰기 그룹, 데이터테이블 BuffGroupTag에서 사용)
	UE_DEFINE_GAMEPLAY_TAG(Item_BuffGroup_Attack, "Item.BuffGroup.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Item_BuffGroup_Critical, "Item.BuffGroup.Critical");
	UE_DEFINE_GAMEPLAY_TAG(Item_BuffGroup_Defense, "Item.BuffGroup.Defense");
	UE_DEFINE_GAMEPLAY_TAG(Item_BuffGroup_MoveSpeed, "Item.BuffGroup.MoveSpeed");
	
	// Skill CoolDown Tag
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_HRU_BasicAttack, "Cooldown.Skill.HRU_BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_HRU_SK_Q, "Cooldown.Skill.HRU_SK_Q");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_HRU_SK_E, "Cooldown.Skill.HRU_SK_E");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_HRU_SK_R, "Cooldown.Skill.HRU_SK_R");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_HRU_SK_C, "Cooldown.Skill.HRU_SK_C");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_HRU_Move, "Cooldown.Skill.HRU_Move");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_INP_BasicAttack, "Cooldown.Skill.INP_BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_INP_SK_Q, "Cooldown.Skill.INP_SK_Q");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_INP_SK_E, "Cooldown.Skill.INP_SK_E");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_INP_SK_R, "Cooldown.Skill.INP_SK_R");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_INP_SK_C, "Cooldown.Skill.INP_SK_C");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_INP_Move, "Cooldown.Skill.INP_Move");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_NFR_BasicAttack, "Cooldown.Skill.NFR_BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_NFR_SK_Q, "Cooldown.Skill.NFR_SK_Q");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_NFR_SK_E, "Cooldown.Skill.NFR_SK_E");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_NFR_SK_R, "Cooldown.Skill.NFR_SK_R");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_NFR_SK_C, "Cooldown.Skill.NFR_SK_C");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Skill_NFR_Move, "Cooldown.Skill.NFR_Move");

	///////////////////////////////////////////
	UE_DEFINE_GAMEPLAY_TAG(Damage_Type_Dot, "Damage.Type.Dot");

	UE_DEFINE_GAMEPLAY_TAG(CameraFeedback_AttackerHit_Basic, "CameraFeedback.AttackerHit.Basic");
	UE_DEFINE_GAMEPLAY_TAG(CameraFeedback_AttackerHit_Skill, "CameraFeedback.AttackerHit.Skill");
	UE_DEFINE_GAMEPLAY_TAG(CameraFeedback_AttackerHit_Ultimate, "CameraFeedback.AttackerHit.Ultimate");

	UE_DEFINE_GAMEPLAY_TAG(Data_Coefficient, "Data.Coefficient");
	UE_DEFINE_GAMEPLAY_TAG(Data_Cooldown, "Data.Cooldown");
	UE_DEFINE_GAMEPLAY_TAG(Data_CurseGauge, "Data.CurseGauge");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_DamageTakenMultiplier, "Data.DamageTakenMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(Data_Duration, "Data.Duration");
	UE_DEFINE_GAMEPLAY_TAG(Data_Heal, "Data.Heal");
	UE_DEFINE_GAMEPLAY_TAG(Data_Shield, "Data.Shield");

	// 스탯강화 아이템용 SetByCaller 태그 (공용 스탯 GE의 Add 모디파이어와 1:1 대응)
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_AttackPower, "Data.Stat.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_Defense, "Data.Stat.Defense");
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_MaxHealth, "Data.Stat.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_MoveSpeed, "Data.Stat.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_AttackSpeed, "Data.Stat.AttackSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_CritChance, "Data.Stat.CritChance");
	UE_DEFINE_GAMEPLAY_TAG(Data_Stat_CooldownReduction, "Data.Stat.CooldownReduction");

	UE_DEFINE_GAMEPLAY_TAG(Event_Abilities_Changed, "Event.Abilities.Changed");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combo_BarrageSmash, "Event.Combo.BarrageSmash");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combo_ChainBurst, "Event.Combo.ChainBurst");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combo_DrawIn, "Event.Combo.DrawIn");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combo_JudgementDrop, "Event.Combo.JudgementDrop");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combo_SanctuaryJudgement, "Event.Combo.SanctuaryJudgement");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combo_SoulDecay, "Event.Combo.SoulDecay");
	UE_DEFINE_GAMEPLAY_TAG(Event_Skill_KillConfirmed, "Event.Skill.KillConfirmed");

	//
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Heru_Charge, "GameplayCue.Ability.Heru.Charge");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Inpu_ShieldRush, "GameplayCue.Ability.Inpu.ShieldRush");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Inpu_AegisVortex, "GameplayCue.Ability.Inpu.AegisVortex");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Inpu_BulwarkFissure, "GameplayCue.Ability.Inpu.BulwarkFissure");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Inpu_BulwarkOfJudgement, "GameplayCue.Ability.Inpu.BulwarkOfJudgement");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Move_DashTrail, "GameplayCue.Ability.Move.DashTrail");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Nefer_PowerOfDecay, "GameplayCue.Ability.Nefer.PowerOfDecay");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Nefer_JudgementOfIsis, "GameplayCue.Ability.Nefer.JudgementOfIsis");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Ability_Nefer_RevelationOfPriest, "GameplayCue.Ability.Nefer.RevelationOfPriest");


	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_CC_Knockback, "GameplayCue.CC.Knockback");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_CC_Stagger, "GameplayCue.CC.Stagger");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Combo_ChainBurst, "GameplayCue.Combo.ChainBurst");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Shield_Apply, "GameplayCue.Shield.Apply");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Status_Decay_Apply, "GameplayCue.Status.Decay.Apply");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Status_Mark_Apply, "GameplayCue.Status.Mark.Apply");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Status_Slow_Apply, "GameplayCue.Status.Slow.Apply");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Boss_SandStormTarget, "GameplayCue.Boss.SandStormTarget");
	
	// Skill Input Tag
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill_Basic, "Input.Skill.Basic");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill_Q, "Input.Skill.Q");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill_E, "Input.Skill.E");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill_R, "Input.Skill.R");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill_C, "Input.Skill.C");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill_Move, "Input.Skill.Move");
	
	///////////


	UE_DEFINE_GAMEPLAY_TAG(Skill_Combo_BarrageSmash, "Skill.Combo.BarrageSmash");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Combo_ChainBurst, "Skill.Combo.ChainBurst");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Combo_DrawIn, "Skill.Combo.DrawIn");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Combo_JudgementDrop, "Skill.Combo.JudgementDrop");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Combo_SanctuaryJudgement, "Skill.Combo.SanctuaryJudgement");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Combo_SoulDecay, "Skill.Combo.SoulDecay");

	// Skill Name Tag

	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_Barrage, "Skill.Heru.Barrage");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_BasicAttack, "Skill.Heru.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_Charge, "Skill.Heru.Charge");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_Descent, "Skill.Heru.Descent");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_Plunge, "Skill.Heru.Plunge");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_Thrust, "Skill.Heru.Thrust");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Heru_Whirl, "Skill.Heru.Whirl");

	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_AegisVortex, "Skill.Inpu.AegisVortex");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_BasicAttack, "Skill.Inpu.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_BulwarkFissure, "Skill.Inpu.BulwarkFissure");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_BulwarkOfJudgement, "Skill.Inpu.BulwarkOfJudgement");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_Crushing, "Skill.Inpu.Crushing");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_ScaleSmash, "Skill.Inpu.ScaleSmash");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_ShieldRush, "Skill.Inpu.ShieldRush");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_SoulHook, "Skill.Inpu.SoulHook");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Inpu_SoulRestitution, "Skill.Inpu.SoulRestitution");

	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_BasicAttack, "Skill.Nefer.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_PowerOfDecay, "Skill.Nefer.PowerOfDecay");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_SanctuaryOfIsis, "Skill.Nefer.SanctuaryOfIsis");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_IsisOfSanctuary, "Skill.Nefer.IsisOfSanctuary");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_Revelation, "Skill.Nefer.Revelation");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_RevelationOfPriest, "Skill.Nefer.RevelationOfPriest");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_JudgementOfIsis, "Skill.Nefer.JudgementOfIsis");
	UE_DEFINE_GAMEPLAY_TAG(Skill_Nefer_Move, "Skill.Nefer.Move");

	/////////////////////////////////
	// State는 System logic상 필요한 것들.
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Attacking, "State.Enemy.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dead, "State.Player.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Skill_BlockMoveInput, "State.Skill.BlockMoveInput");
	UE_DEFINE_GAMEPLAY_TAG(State_Skill_BlockSkillInput, "State.Skill.BlockSkillInput");
	UE_DEFINE_GAMEPLAY_TAG(State_Skill_NFR_Move_Recharging, "State.Skill.NFR_Move.Recharging");

	// Status 게임 효과에 의한 것.
	UE_DEFINE_GAMEPLAY_TAG(Status_Buff_SuperArmor, "Status.Buff.SuperArmor");
	UE_DEFINE_GAMEPLAY_TAG(Status_CC_Knockback, "Status.CC.Knockback");
	UE_DEFINE_GAMEPLAY_TAG(Status_CC_Pull, "Status.CC.Pull");
	UE_DEFINE_GAMEPLAY_TAG(Status_CC_Stagger, "Status.CC.Stagger");
	UE_DEFINE_GAMEPLAY_TAG(Status_Decay, "Status.Decay");
	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Decay, "Status.Debuff.Decay");
	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Slow, "Status.Debuff.Slow");
	UE_DEFINE_GAMEPLAY_TAG(Status_Debuff_Verdict, "Status.Debuff.Verdict");
	UE_DEFINE_GAMEPLAY_TAG(Status_Buff_Heal, "Status.Buff.Heal");
	UE_DEFINE_GAMEPLAY_TAG(Status_Special_SoulRestitution, "Status.Special.SoulRestitution");
	UE_DEFINE_GAMEPLAY_TAG(Status_Mark, "Status.Mark");
	UE_DEFINE_GAMEPLAY_TAG(Status_Boss_SandStormTarget, "Status.Boss.SandStormTarget");


	// 미션 - 이것도 스트리밍용
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Mission_AntiAFK, "Streaming.Mission.AntiAFK");

	// 스트리밍 - 채널 
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_Item, "Streaming.Channel.Item");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_CountEvent, "Streaming.Channel.CountEvent");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Item_Used, "Streaming.Event.Item.Used");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Item_Purchased, "Streaming.Event.Item.Purchased");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_World_PotBroken, "Streaming.Event.World.PotBroken");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_World_PlayerDied, "Streaming.Event.World.PlayerDied");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Donation_Granted, "Streaming.Event.Donation.Granted");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_State, "Streaming.Channel.State");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_State_Player_Moving, "Streaming.State.Player.Moving");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_State_Player_Idle, "Streaming.State.Player.Idle");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_State_Combat, "Streaming.State.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_State_Gimmick_Active, "Streaming.State.Gimmick.Active");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_State_Plate_Pressed, "Streaming.State.Plate.Pressed");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_Combat, "Streaming.Channel.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_PlayerInput, "Streaming.Channel.PlayerInput");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_Gimmick, "Streaming.Channel.Gimmick");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_Meso, "Streaming.Channel.Meso");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_Zone, "Streaming.Channel.Zone");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_Zone_Event, "Streaming.Channel.Zone.Event");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Channel_UI_Chat, "Streaming.Channel.UI.Chat");

	// 스트리밍 - 이벤트
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Combat_Hit, "Streaming.Event.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Player_Input, "Streaming.Event.Player.Input");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Combat_SkillUsed, "Streaming.Event.Combat.SkillUsed");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Combat_Kill, "Streaming.Event.Combat.Kill");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Gimmick_PartyReset, "Streaming.Event.Gimmick.PartyReset");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_SmallTalk, "Streaming.Event.SmallTalk");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Zone_Cleared, "Streaming.Event.Zone.Cleared");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Zone_Activated, "Streaming.Event.Zone.Activated");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Zone_ClearedReentered, "Streaming.Event.Zone.ClearedReentered");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Meso_Earned, "Streaming.Event.Meso.Earned");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Meso_Spent, "Streaming.Event.Meso.Spent");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Mission_Start, "Streaming.Event.Mission.Start");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_Mission_Completed, "Streaming.Event.Mission.Completed");
	UE_DEFINE_GAMEPLAY_TAG(Streaming_Event_AntiAFK_Resumed, "Streaming.Event.AntiAFK.Resumed");


	// 스트리밍 용이긴 한데... 메소의 출처
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_CombatReward, "Meso.Source.CombatReward");
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_ShopPurchase, "Meso.Source.ShopPurchase");
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_Revive, "Meso.Source.Revive");
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_Streaming, "Meso.Source.Streaming");
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_Streaming_Donation, "Meso.Source.Streaming.Donation");
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_Streaming_Mission, "Meso.Source.Streaming.Mission");
	UE_DEFINE_GAMEPLAY_TAG(Meso_Source_Cheat, "Meso.Source.Cheat");



	///// UI 레이어
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Lobby, "UI.Layer.Lobby");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_HUD, "UI.Layer.HUD");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Menu, "UI.Layer.Menu");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Modal, "UI.Layer.Modal");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Dialogue, "UI.Layer.Dialogue");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Toast, "UI.Layer.Toast");
	UE_DEFINE_GAMEPLAY_TAG(UI_Layer_Overlay, "UI.Layer.Overlay");
}
