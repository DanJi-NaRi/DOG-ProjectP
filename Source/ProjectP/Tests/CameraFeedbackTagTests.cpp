////////////////////////////
//! \file CameraFeedbackTagTests.cpp
//! \brief 스킬 입력과 공격자 적중 카메라 피드백 태그 사이의 분류 규칙을 검증한다.
//! \author HanUl
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "GAS/MyAbilitySystemLibrary.h"
#include "MyGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCameraFeedbackInputClassificationTest,
	"ProjectP.CameraFeedback.InputClassification",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author HanUl
//! \brief Basic/Q/E/R/C/Move 입력이 요구한 공격자 적중 피드백 등급으로 분류되는지 확인한다.
//! \param Parameters 자동화 테스트 프레임워크 실행 매개변수
//! \return 모든 검증을 수행하면 true
bool FCameraFeedbackInputClassificationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(
		TEXT("기본 공격은 Basic 피드백"),
		UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(MyGameplayTags::Input_Skill_Basic)
			.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Basic));
	TestTrue(
		TEXT("Q 스킬은 Skill 피드백"),
		UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(MyGameplayTags::Input_Skill_Q)
			.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Skill));
	TestTrue(
		TEXT("E 스킬은 Skill 피드백"),
		UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(MyGameplayTags::Input_Skill_E)
			.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Skill));
	TestTrue(
		TEXT("R 스킬은 Skill 피드백"),
		UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(MyGameplayTags::Input_Skill_R)
			.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Skill));
	TestTrue(
		TEXT("C 스킬은 Ultimate 피드백"),
		UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(MyGameplayTags::Input_Skill_C)
			.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Ultimate));
	TestFalse(
		TEXT("이동 스킬은 적중 피드백 없음"),
		UMyAbilitySystemLibrary::ResolveAttackerHitCameraFeedbackTag(MyGameplayTags::Input_Skill_Move).IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCameraFeedbackAssetTagPriorityTest,
	"ProjectP.CameraFeedback.AssetTagPriority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

////////////////////////////
//! \author HanUl
//! \brief Effect AssetTag에 여러 피드백이 섞여도 Ultimate, Skill, Basic 순으로 가장 강한 등급을 선택하는지 확인한다.
//! \param Parameters 자동화 테스트 프레임워크 실행 매개변수
//! \return 모든 검증을 수행하면 true
bool FCameraFeedbackAssetTagPriorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FGameplayTagContainer EffectAssetTags;
	EffectAssetTags.AddTag(MyGameplayTags::CameraFeedback_AttackerHit_Basic);
	EffectAssetTags.AddTag(MyGameplayTags::CameraFeedback_AttackerHit_Skill);
	EffectAssetTags.AddTag(MyGameplayTags::CameraFeedback_AttackerHit_Ultimate);

	TestTrue(
		TEXT("여러 등급 중 Ultimate 우선"),
		UMyAbilitySystemLibrary::FindAttackerHitCameraFeedbackTag(EffectAssetTags)
			.MatchesTagExact(MyGameplayTags::CameraFeedback_AttackerHit_Ultimate));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
