////////////////////////////
//! \page MyGameplayTags.h
//! \brief ProjectP C++ 코드에서 참조할 Native GameplayTag 선언 파일이다.
#pragma once

#include "NativeGameplayTags.h"

////////////////////////////
//! \namespace MyGameplayTags
//! \brief 스킬, 입력, 쿨타임, 상태, 이벤트, SetByCaller, GameplayCue  등 Native GameplayTag를 C++ 상수로 제공한다.
//!  UI.Layer도 추가!
//! \note C++ 변수명은 식별자 규칙 때문에 언더스코어를 사용하며, 실제 GameplayTag 문자열은 MyGameplayTags.cpp에서 명시적으로 정의한다.
namespace MyGameplayTags
{
	// 식별
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Player);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Enemy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Enemy_Boss);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Faction_Objective);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Target_Destructible);

	// 신 정체성 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Horus);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Isis);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Anubis);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Thoth);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Hathor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Sekhmet);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Nephthys);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Ra);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(God_Set);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Event_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Event_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Event_OutRange);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Event_SeeTarget);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Event_TargetChange);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AI_Event_TargetLost);


	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Primary);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_EnergyBolt);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_HealingBolt);

	//! 아이템 사용 쿨타임. 같은 태그를 쓰는 아이템끼리 쿨타임을 공유한다 (포션 3종 공유)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item_Potion);

	//! 스탯강화(영약류) 아이템 공유 쿨타임
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item_Elixir);

	//! 버프 계열 태그. 같은 계열 태그의 스탯 버프는 중첩되지 않고 기존 것이 제거된 뒤 새로 적용된다 (계열 덮어쓰기)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_BuffGroup_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_BuffGroup_Critical);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_BuffGroup_Defense);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_BuffGroup_MoveSpeed);
	
	// 캐릭터 태그 (스트리밍 시스템에서 대상을 지시하기 위한 태그)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Enemy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Enemy_Boss);

	// 공용 스킬 태그 (어빌리티가 아닌 조작도 스킬 사용 사실로 다룬다)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Common_Jump);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Common_Jump_Moving);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Common_Jump_InPlace);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Player);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Player_Nefer);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Player_Inpu);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Player_Heru);

	//! \author 장효제
	//! \brief 선택 캐릭터 ID를 Streaming과 Mission이 공유하는 구체 CharacterTag로 변환한다.
	//! \param SelectedCharacterId 파티에서 확정한 캐릭터 ID다.
	//! \return 100/200/300에 대응하는 태그이며 알 수 없는 ID면 빈 태그다.
	PROJECTP_API FGameplayTag GetPlayerCharacterTag(int32 SelectedCharacterId);

	///// Skill Cooldown
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HRU_BasicAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HRU_SK_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HRU_SK_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HRU_SK_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HRU_SK_C);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_HRU_Move);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_INP_BasicAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_INP_SK_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_INP_SK_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_INP_SK_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_INP_SK_C);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_INP_Move);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_NFR_BasicAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_NFR_SK_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_NFR_SK_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_NFR_SK_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_NFR_SK_C);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Skill_NFR_Move); 


	/////////
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Dot);

	// 공격자 본인 카메라에만 재생할 적중 피드백 등급
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CameraFeedback_AttackerHit_Basic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CameraFeedback_AttackerHit_Skill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CameraFeedback_AttackerHit_Ultimate);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Coefficient);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_CurseGauge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_DamageTakenMultiplier);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Shield);

	//! 스탯강화 아이템용 SetByCaller 태그. 공용 스탯 GE의 Add 모디파이어와 1:1 대응한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_AttackPower);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_Defense);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_MaxHealth);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_MoveSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_AttackSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_CritChance);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Stat_CooldownReduction);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Abilities_Changed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_BarrageSmash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_ChainBurst);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_DrawIn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_JudgementDrop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_SanctuaryJudgement);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combo_SoulDecay);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_KillConfirmed);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Heru_Charge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Inpu_ShieldRush);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Inpu_AegisVortex);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Inpu_BulwarkFissure);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Inpu_BulwarkOfJudgement);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Move_DashTrail);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Nefer_PowerOfDecay);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Nefer_JudgementOfIsis);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Ability_Nefer_RevelationOfPriest);


	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_CC_Knockback);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_CC_Stagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Combo_ChainBurst);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Shield_Apply);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_Decay_Apply);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_Mark_Apply);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Status_Slow_Apply);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Boss_SandStormTarget);

	// skill input
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill_Basic);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill_C);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill_Move);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Combo_BarrageSmash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Combo_ChainBurst);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Combo_DrawIn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Combo_JudgementDrop);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Combo_SanctuaryJudgement);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Combo_SoulDecay);

	// skill name
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_Barrage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_BasicAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_Charge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_Descent);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_Plunge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_Thrust);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Heru_Whirl);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_AegisVortex);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_BasicAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_BulwarkFissure);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_BulwarkOfJudgement);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_Crushing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_ScaleSmash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_ShieldRush);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_SoulHook);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Inpu_SoulRestitution);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_BasicAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_PowerOfDecay);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_SanctuaryOfIsis);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_IsisOfSanctuary);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_Revelation);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_RevelationOfPriest);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_JudgementOfIsis);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Skill_Nefer_Move);
	///////////////////


	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Enemy_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Player_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Skill_BlockMoveInput);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Skill_BlockSkillInput);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Skill_NFR_Move_Recharging);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Buff_SuperArmor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Buff_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_CC_Knockback);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_CC_Pull);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_CC_Stagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Decay);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Decay);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Slow);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Debuff_Verdict);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Special_SoulRestitution);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Mark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Boss_SandStormTarget);

	// Domain-neutral Game Activity

	// Mission 분류
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Mission_AntiAFK);

	// Streaming
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_Item);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_CountEvent);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Item_Used);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Item_Purchased);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_World_PotBroken);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_World_PlayerDied);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Donation_Granted);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_State);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_State_Player_Moving);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_State_Player_Idle);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_State_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_State_Gimmick_Active);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_State_Plate_Pressed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_PlayerInput);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_Gimmick);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_Meso);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_Zone);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_Zone_Event);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Channel_UI_Chat);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Combat_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Player_Input);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Combat_SkillUsed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Combat_Kill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Gimmick_PartyReset);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_SmallTalk);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Zone_Cleared);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Zone_Activated);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Zone_ClearedReentered);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Mission_Start);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Mission_Completed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_AntiAFK_Resumed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Meso_Earned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Streaming_Event_Meso_Spent);

	//
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_CombatReward);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_ShopPurchase);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_Revive);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_Streaming);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_Streaming_Donation);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_Streaming_Mission);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Meso_Source_Cheat);


	// 테스트/디버그·수동 요청 전용 Streaming 이벤트 출처(실제 게임 이벤트 태그를 거짓 출처로 재사용하지 않기 위함)

	// UI Layer
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Lobby);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_HUD);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Menu);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Modal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Dialogue);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Toast);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(UI_Layer_Overlay);
}
