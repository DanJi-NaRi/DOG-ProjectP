////////////////////////////
//! \file MySkillDefinitionFragment.cpp
//! \brief SkillDefinition Fragment 구현 파일이다.

#include "MySkillDefinitionFragment.h"

////////////////////////////
//! \author HanUl
//! \brief 연쇄 가능한 추가 대상 수를 반환한다.
//! \param 없음
//! \return 첫 적중 대상 이후 추가로 연쇄할 수 있는 대상 수
int32 UMySkillChainFragment::GetMaxAdditionalTargets() const
{
	return FMath::Max(MaxAdditionalTargets, 0);
}

////////////////////////////
//! \author HanUl
//! \brief 연쇄 대상 탐색 반경을 반환한다.
//! \param 없음
//! \return 탐색 반경
float UMySkillChainFragment::GetSearchRadius() const
{
	return FMath::Max(SearchRadius, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 같은 대상에게 연쇄 피해를 한 번만 줄지 반환한다.
//! \param 없음
//! \return 같은 대상 중복 적중을 막으면 true
bool UMySkillChainFragment::ShouldHitEachTargetOnce() const
{
	return bHitEachTargetOnce;
}

////////////////////////////
//! \author HanUl
//! \brief 기존 상태 보유 여부를 확인할 GameplayTag 목록을 반환한다.
//! \param 없음
//! \return 상태 GameplayTag 목록
const FGameplayTagContainer& UMySkillExistingStatusBonusFragment::GetStatusTags() const
{
	return StatusTags;
}

////////////////////////////
//! \author HanUl
//! \brief 기존 상태 보유 대상에게 줄 즉시 추가 피해 계수를 반환한다.
//! \param 없음
//! \return 공격력에 곱할 추가 피해 계수
float UMySkillExistingStatusBonusFragment::GetBonusDamageCoefficient() const
{
	return FMath::Max(BonusDamageCoefficient, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 균열 끝 지점의 판정 폭을 반환한다.
//! \param 없음
//! \return 끝 폭(cm, 0 이상)
float UMyBulwarkFissureFragment::GetEndWidth() const
{
	return FMath::Max(EndWidth, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 균열이 시작되는 캐릭터 전방 오프셋을 반환한다.
//! \param 없음
//! \return 전방 시작 오프셋(cm, 0 이상)
float UMyBulwarkFissureFragment::GetStartForwardOffset() const
{
	return FMath::Max(StartForwardOffset, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 판정 박스의 절반 높이를 반환한다.
//! \param 없음
//! \return 판정 박스 절반 높이(cm, 0 이상)
float UMyBulwarkFissureFragment::GetTraceHeight() const
{
	return FMath::Max(TraceHeight, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 균열 전파 판정 서브틱 간격을 반환한다.
//! \param 없음
//! \return 서브틱 간격(초, 0.01 이상)
float UMyBulwarkFissureFragment::GetSubTickInterval() const
{
	return FMath::Max(SubTickInterval, 0.01f);
}

////////////////////////////
//! \author HanUl
//! \brief 밀어내기 파동 횟수를 반환한다.
//! \param 없음
//! \return 파동 횟수(1 이상)
int32 UMyAegisVortexFragment::GetPulseCount() const
{
	return FMath::Max(PulseCount, 1);
}

////////////////////////////
//! \author HanUl
//! \brief 파동이 대상을 당길지 밀어낼지를 반환한다.
//! \param 없음
//! \return 이동 방향 모드(Pull 또는 Push)
EMyAegisVortexMoveMode UMyAegisVortexFragment::GetMoveMode() const
{
	return MoveMode;
}

////////////////////////////
//! \author HanUl
//! \brief 파동당 이동 거리를 반환한다.
//! \param 없음
//! \return 이동 거리(cm, 0 이상)
float UMyAegisVortexFragment::GetMoveDistance() const
{
	return FMath::Max(MoveDistance, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 피니셔 지면 강타 판정 반경을 반환한다.
//! \param 없음
//! \return 피니셔 반경(cm, 0 이상)
float UMyAegisVortexFragment::GetFinisherRadius() const
{
	return FMath::Max(FinisherRadius, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 마지막 파동과 피니셔 강타 사이의 지연을 반환한다.
//! \param 없음
//! \return 피니셔 지연(초, 0 이상)
float UMyAegisVortexFragment::GetFinisherDelay() const
{
	return FMath::Max(FinisherDelay, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 보호막량 비율(%)을 반환한다.
//! \param 없음
//! \return 보호막 비율(%, 0 이상)
float UMyBulwarkOfJudgementFragment::GetShieldPercentOfMaxHealth() const
{
	return FMath::Max(ShieldPercentOfMaxHealth, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 보호막 받은 1명당 강타 피해 계수 가산값을 반환한다.
//! \param 없음
//! \return 보호막 1명당 추가 계수(0 이상)
float UMyBulwarkOfJudgementFragment::GetPerShieldedCoefficient() const
{
	return FMath::Max(PerShieldedCoefficient, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 적중 지점 폭발 판정 반경을 반환한다.
//! \param 없음
//! \return 폭발 반경(cm, 0 이상)
float UMyProjectileExplosionFragment::GetExplosionRadius() const
{
	return FMath::Max(ExplosionRadius, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 폭발 피해 계수를 반환한다.
//! \param 없음
//! \return 폭발 피해 계수(0 이상)
float UMyProjectileExplosionFragment::GetExplosionDamageCoefficient() const
{
	return FMath::Max(ExplosionDamageCoefficient, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 한 번의 발사에서 나가는 투사체 개수를 반환한다.
//! \param 없음
//! \return 투사체 개수(1 이상)
int32 UMyProjectileSpreadFragment::GetProjectileCount() const
{
	return FMath::Max(ProjectileCount, 1);
}

////////////////////////////
//! \author HanUl
//! \brief 확산 각도의 해석 기준을 반환한다.
//! \param 없음
//! \return 확산 모드(AngleStep 또는 TotalAngle)
EMyProjectileSpreadMode UMyProjectileSpreadFragment::GetSpreadMode() const
{
	return SpreadMode;
}

////////////////////////////
//! \author HanUl
//! \brief 확산 각도를 반환한다.
//! \param 없음
//! \return 확산 각도(도, 0 이상)
float UMyProjectileSpreadFragment::GetSpreadAngleDegrees() const
{
	return FMath::Max(SpreadAngleDegrees, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 투사체 1발당 피해 계수 배율을 반환한다.
//! \param 없음
//! \return 발당 피해 계수 배율(0 이상)
float UMyProjectileSpreadFragment::GetPerProjectileDamageScale() const
{
	return FMath::Max(PerProjectileDamageScale, 0.0f);
}

////////////////////////////
//! \author HanUl
//! \brief 조준 방향을 기준으로 지정한 순번 투사체가 틀어질 Yaw 오프셋을 계산한다.
//! \param ProjectileIndex 투사체 순번(0부터 시작)
//! \return 조준 방향 기준 Yaw 오프셋(도). 개수가 1이면 항상 0이라 직선으로 나간다.
float UMyProjectileSpreadFragment::GetYawOffsetDegrees(int32 ProjectileIndex) const
{
	const int32 Count = GetProjectileCount();
	if (Count <= 1)
	{
		return 0.0f;
	}

	const float SpreadAngle = GetSpreadAngleDegrees();
	const float AngleStep = SpreadMode == EMyProjectileSpreadMode::AngleStep
		? SpreadAngle
		: SpreadAngle / static_cast<float>(Count - 1);

	// 순번을 중앙 기준 대칭 좌표로 바꿔 간격을 곱한다. 홀수면 가운데 1발이 정확히 조준 방향으로 나간다.
	const int32 SafeIndex = FMath::Clamp(ProjectileIndex, 0, Count - 1);
	return (static_cast<float>(SafeIndex) - static_cast<float>(Count - 1) * 0.5f) * AngleStep;
}
