////////////////////////////
//! \page MyPlayerState.cpp
//! \brief Re Duat PlayerState의 ASC와 AttributeSet 생성 및 조회를 구현한다.
#include "MyPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Item/MyInventoryComponent.h"
#include "MyAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "../MyGameplayTags.h"

////////////////////////////
//! \editor 준혁 - 메소/아이템 관리용 인벤토리 컴포넌트 생성 추가
//! \brief PlayerState-owned ASC와 MyAttributeSet을 생성하고 플레이어용 복제 설정을 적용한다.
AMyPlayerState::AMyPlayerState()
{
	SetNetUpdateFrequency(100.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMyAttributeSet>(TEXT("AttributeSet"));

	InventoryComponent = CreateDefaultSubobject<UMyInventoryComponent>(TEXT("InventoryComponent"));
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 초기화 이후 서버에서 플레이어 진영(Faction.Player) 태그를 ASC에 부여한다.
//! \param 없음
//! \return 없음
void AMyPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetLocalRole() == ROLE_Authority && AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(MyGameplayTags::Faction_Player, 1, EGameplayTagReplicationState::TagOnly);
		BindHealthChangedDelegate();
		BindCurseGaugeChangedDelegate();
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 복제할 프로퍼티를 등록하는 함수
// OutLifetimeProps : 언리얼 네트워크 복제 시스템에 등록될 프로퍼티 목록
void AMyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyPlayerState, UserIndex);
	DOREPLIFETIME(AMyPlayerState, Username);
	DOREPLIFETIME(AMyPlayerState, bAuthVerified);
	DOREPLIFETIME(AMyPlayerState, LifeState);
	DOREPLIFETIME(AMyPlayerState, bIsCursed);
    DOREPLIFETIME(AMyPlayerState, SelectedCharacterId);
	DOREPLIFETIME(AMyPlayerState, CharacterLevel);
	DOREPLIFETIME(AMyPlayerState, CharacterExp);
	DOREPLIFETIME(AMyPlayerState, SkillPoints);
	DOREPLIFETIME(AMyPlayerState, SkillLevels);
}

////////////////////////////
//! \brief 이 PlayerState가 소유한 AbilitySystemComponent를 반환한다.
//! \return AbilitySystemComponent 포인터
UAbilitySystemComponent* AMyPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

////////////////////////////
//! \brief 이 PlayerState가 소유한 Re Duat AttributeSet을 반환한다.
//! \return MyAttributeSet 포인터
UMyAttributeSet* AMyPlayerState::GetMyAttributeSet() const
{
	return AttributeSet;
}

////////////////////////////
//! \author 준혁
//! \brief 이 PlayerState가 소유한 인벤토리 컴포넌트를 반환한다.
//! \return InventoryComponent 포인터
UMyInventoryComponent* AMyPlayerState::GetInventoryComponent() const
{
	return InventoryComponent;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인증된 유저의 DB user_Index를 반환하는 함수
// Return Value : 인증된 유저의 DB user_Index, 인증되지 않았으면 -1
int32 AMyPlayerState::GetUserIndex() const
{
	return UserIndex;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인증된 유저 이름을 반환하는 함수
// Return Value : 인증된 유저 이름
const FString& AMyPlayerState::GetUsername() const
{
	return Username;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 서버 인증이 완료되었는지 확인하는 함수
// Return Value : 인증 완료 여부
bool AMyPlayerState::IsAuthVerified() const
{
	return bAuthVerified;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 플레이어의 서버 권위 생명 상태를 반환한다.
//! \param 없음
//! \return 현재 생명 상태
EPlayerLifeState AMyPlayerState::GetLifeState() const
{
	return LifeState;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 플레이어가 생존 상태인지 확인한다.
//! \param 없음
//! \return Alive 상태이면 true
bool AMyPlayerState::IsAlive() const
{
	return LifeState == EPlayerLifeState::Alive;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 플레이어가 사망 상태인지 확인한다.
//! \param 없음
//! \return Dead 상태이면 true
bool AMyPlayerState::IsDead() const
{
	return LifeState == EPlayerLifeState::Dead;
}

////////////////////////////
//! \author HanSeul
//! \brief 현재 플레이어가 저주 상태인지 확인한다.
//! \param 없음
//! \return 저주 상태이면 true
bool AMyPlayerState::IsCursed() const
{
	return bIsCursed;
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 플레이어의 저주 상태를 변경하고 상태 변경을 통지한다.
//! \param bNewCursed 새로 적용할 저주 상태
//! \return 없음
void AMyPlayerState::SetCurseState(bool bNewCursed)
{
	if (!HasAuthority() || bIsCursed == bNewCursed)
	{
		return;
	}

	const bool bWasCursed = bIsCursed;
	bIsCursed = bNewCursed;
	OnCurseStateChanged.Broadcast(bWasCursed, bIsCursed);
	ForceNetUpdate();
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 플레이어의 생명 상태를 변경하고 상태 변경을 통지한다. 같은 상태 재요청은 무시한다.
//! \param NewLifeState 새로 적용할 생명 상태
//! \return 없음
void AMyPlayerState::SetLifeState(EPlayerLifeState NewLifeState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		if (NewLifeState == EPlayerLifeState::Dead)
		{
			if (!AbilitySystemComponent->HasMatchingGameplayTag(MyGameplayTags::State_Player_Dead))
			{
				AbilitySystemComponent->AddLooseGameplayTag(
					MyGameplayTags::State_Player_Dead,
					1,
					EGameplayTagReplicationState::TagOnly);
			}
		}
		else if (AbilitySystemComponent->HasMatchingGameplayTag(MyGameplayTags::State_Player_Dead))
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(
				MyGameplayTags::State_Player_Dead,
				1,
				EGameplayTagReplicationState::TagOnly);
		}
	}

	if (LifeState == NewLifeState)
	{
		return;
	}

	const EPlayerLifeState OldLifeState = LifeState;
	LifeState = NewLifeState;
	OnLifeStateChanged.Broadcast(OldLifeState, LifeState);
	ForceNetUpdate();
}

////////////////////////////
//! \author HanUl
//! \brief 서버 ASC의 Health 변경 델리게이트를 중복 없이 등록한다.
//! \param 없음
//! \return 없음
void AMyPlayerState::BindHealthChangedDelegate()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
		HealthChangedDelegateHandle.Reset();
	}

	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AMyPlayerState::HandleHealthChanged);
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 Health가 양수에서 0 이하로 내려가는 순간 플레이어를 Dead 상태로 전환한다.
//! \param ChangeData Health 변경 전후 값
//! \return 없음
void AMyPlayerState::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || LifeState == EPlayerLifeState::Dead)
	{
		return;
	}

	if (ChangeData.OldValue > 0.0f && ChangeData.NewValue <= 0.0f)
	{
		SetLifeState(EPlayerLifeState::Dead);
	}
}

////////////////////////////
//! \author HanSeul
//! \brief 서버 ASC의 CurseGauge 변경 델리게이트를 중복 없이 등록한다.
//! \param 없음
//! \return 없음
void AMyPlayerState::BindCurseGaugeChangedDelegate()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (CurseGaugeChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetCurseGaugeAttribute())
			.Remove(CurseGaugeChangedDelegateHandle);
		CurseGaugeChangedDelegateHandle.Reset();
	}

	CurseGaugeChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UMyAttributeSet::GetCurseGaugeAttribute())
		.AddUObject(this, &AMyPlayerState::HandleCurseGaugeChanged);
}

////////////////////////////
//! \author HanSeul
//! \brief 서버에서 CurseGauge가 최대값에 처음 도달하는 순간 저주 상태로 전환한다.
//! \param ChangeData CurseGauge 변경 전후 값
//! \return 없음
void AMyPlayerState::HandleCurseGaugeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || !IsAlive() || bIsCursed)
	{
		return;
	}

	if (ChangeData.OldValue < UMyAttributeSet::CurseGaugeMax
		&& ChangeData.NewValue >= UMyAttributeSet::CurseGaugeMax)
	{
		SetCurseState(true);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 클라이언트에서 생명 상태 복제를 수신하면 상태 변경을 통지한다.
//! \param OldLifeState 복제 이전 생명 상태
//! \return 없음
void AMyPlayerState::OnRep_LifeState(EPlayerLifeState OldLifeState)
{
	OnLifeStateChanged.Broadcast(OldLifeState, LifeState);
}

////////////////////////////
//! \author HanSeul
//! \brief 클라이언트에서 저주 상태 복제를 수신하면 상태 변경을 통지한다.
//! \param bWasCursed 복제 이전 저주 상태
//! \return 없음
void AMyPlayerState::OnRep_IsCursed(bool bWasCursed)
{
	OnCurseStateChanged.Broadcast(bWasCursed, bIsCursed);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전 입장 시 선택 확정된 캐릭터 ID를 반환하는 함수
// Return Value : 선택 캐릭터 ID, 선택되지 않았으면 -1
int32 AMyPlayerState::GetSelectedCharacterId() const
{
    return SelectedCharacterId;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 캐릭터 레벨을 반환한다.
//! \param 없음
//! \return 캐릭터 레벨(1 이상)
int32 AMyPlayerState::GetCharacterLevel() const
{
	return CharacterLevel;
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 레벨 변경 시 UI 갱신용 OnLevelDataChanged 브로드캐스트 추가
//! \brief 서버에서 캐릭터 레벨 값을 갱신한다. 레벨 스탯 적용은 APlayerCharacterBase::SetCharacterLevel이 담당한다.
//! \param NewCharacterLevel 새 캐릭터 레벨(1 이상으로 보정)
//! \return 없음
void AMyPlayerState::SetCharacterLevel(int32 NewCharacterLevel)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	const int32 ClampedLevel = FMath::Max(NewCharacterLevel, 1);
	if (CharacterLevel == ClampedLevel)
	{
		return;
	}

	CharacterLevel = ClampedLevel;
	OnLevelDataChanged.Broadcast();
}

////////////////////////////
//! \author HanUl
//! \brief 현재 레벨의 누적 경험치를 반환한다.
//! \param 없음
//! \return 누적 경험치(0 이상)
int32 AMyPlayerState::GetCharacterExp() const
{
	return CharacterExp;
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 경험치 변경 시 UI 갱신용 OnLevelDataChanged 브로드캐스트 추가
//! \brief 서버에서 현재 레벨의 누적 경험치 값을 갱신한다. 획득/레벨업 판정은 APlayerCharacterBase::AddExperience가 담당한다.
//! \param NewCharacterExp 새 누적 경험치(0 이상으로 보정)
//! \return 없음
void AMyPlayerState::SetCharacterExp(int32 NewCharacterExp)
{
	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	const int32 ClampedExp = FMath::Max(NewCharacterExp, 0);
	if (CharacterExp == ClampedExp)
	{
		return;
	}

	CharacterExp = ClampedExp;
	OnLevelDataChanged.Broadcast();
}

////////////////////////////
//! \author 준혁
//! \brief 클라이언트에서 레벨 복제를 수신하면 UI 갱신용 알림을 브로드캐스트한다.
//! \param 없음
//! \return 없음
void AMyPlayerState::OnRep_CharacterLevel()
{
	OnLevelDataChanged.Broadcast();
}

////////////////////////////
//! \author 준혁
//! \brief 클라이언트에서 경험치 복제를 수신하면 UI 갱신용 알림을 브로드캐스트한다.
//! \param 없음
//! \return 없음
void AMyPlayerState::OnRep_CharacterExp()
{
	OnLevelDataChanged.Broadcast();
}

////////////////////////////
//! \author HanUl
//! \brief 현재 미사용 스킬포인트를 반환한다.
//! \param 없음
//! \return 스킬포인트(0 이상)
int32 AMyPlayerState::GetSkillPoints() const
{
	return SkillPoints;
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 스킬포인트를 가감하고 변경을 통지한다. 결과는 0 미만으로 내려가지 않는다.
//! \param Delta 가감할 스킬포인트(음수 허용)
//! \return 없음
void AMyPlayerState::AddSkillPoints(int32 Delta)
{
	if (GetLocalRole() != ROLE_Authority || Delta == 0)
	{
		return;
	}

	const int32 NewPoints = FMath::Max(SkillPoints + Delta, 0);
	if (NewPoints == SkillPoints)
	{
		return;
	}

	SkillPoints = NewPoints;
	OnSkillProgressChanged.Broadcast();
	ForceNetUpdate();
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 스킬포인트를 소비 시도한다. 잔량이 부족하면 아무것도 소비하지 않는다.
//! \param Amount 소비할 스킬포인트(양수)
//! \return 소비에 성공하면 true
bool AMyPlayerState::ConsumeSkillPoints(int32 Amount)
{
	if (GetLocalRole() != ROLE_Authority || Amount <= 0 || SkillPoints < Amount)
	{
		return false;
	}

	SkillPoints -= Amount;
	OnSkillProgressChanged.Broadcast();
	ForceNetUpdate();
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 특정 스킬의 현재 강화 레벨을 반환한다. 저장된 엔트리가 없으면 1단계로 간주한다.
//! \param SkillId 조회할 스킬 ID
//! \return 현재 강화 레벨(1 이상)
int32 AMyPlayerState::GetSkillLevel(FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return 1;
	}

	for (const FMySkillLevelEntry& Entry : SkillLevels)
	{
		if (Entry.SkillId == SkillId)
		{
			return Entry.Level;
		}
	}

	return 1;
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 특정 스킬의 강화 레벨을 설정하고 변경을 통지한다(1 이상으로 보정).
//! \param SkillId 대상 스킬 ID
//! \param NewLevel 새 강화 레벨
//! \return 없음
void AMyPlayerState::SetSkillLevel(FName SkillId, int32 NewLevel)
{
	if (GetLocalRole() != ROLE_Authority || SkillId.IsNone())
	{
		return;
	}

	const int32 ClampedLevel = FMath::Max(NewLevel, 1);
	for (FMySkillLevelEntry& Entry : SkillLevels)
	{
		if (Entry.SkillId == SkillId)
		{
			if (Entry.Level == ClampedLevel)
			{
				return;
			}

			Entry.Level = ClampedLevel;
			OnSkillProgressChanged.Broadcast();
			ForceNetUpdate();
			return;
		}
	}

	FMySkillLevelEntry NewEntry;
	NewEntry.SkillId = SkillId;
	NewEntry.Level = ClampedLevel;
	SkillLevels.Add(NewEntry);
	OnSkillProgressChanged.Broadcast();
	ForceNetUpdate();
}

////////////////////////////
//! \author HanUl
//! \brief 지금까지 지급된 스킬포인트 총량을 반환한다(현재 보유 + 사용분). 사용분은 강화 누적 횟수(각 스킬 레벨-1의 합)로 계산한다.
//! \param 없음
//! \return 누적 지급된 스킬포인트 총량
int32 AMyPlayerState::GetTotalSkillPointsGranted() const
{
	int32 SpentPoints = 0;
	for (const FMySkillLevelEntry& Entry : SkillLevels)
	{
		SpentPoints += FMath::Max(0, Entry.Level - 1);
	}

	return SkillPoints + SpentPoints;
}

////////////////////////////
//! \author HanUl
//! \brief 클라이언트에서 스킬포인트/강화 레벨 복제를 수신하면 UI·스킬 컴포넌트 갱신용 알림을 브로드캐스트한다.
//! \param 없음
//! \return 없음
void AMyPlayerState::OnRep_SkillProgress()
{
	OnSkillProgressChanged.Broadcast();
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 플레이어 이름만 설정하는 함수
// NewUsername : 새로 설정할 플레이어 이름
void AMyPlayerState::SetUsername(const FString& NewUsername)
{
    Username = NewUsername;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인증된 유저 정보를 설정하는 함수
// NewUserIndex : 인증된 유저의 DB user_Index
// NewUsername : 인증된 유저 이름
void AMyPlayerState::SetAuthenticatedUser(int32 NewUserIndex, const FString& NewUsername)
{
	UserIndex = NewUserIndex;
	Username = NewUsername;
	bAuthVerified = true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 던전에서 사용할 선택 캐릭터 ID를 설정하는 함수
// NewSelectedCharacterId : 던전 입장 전 파티에서 확정한 캐릭터 ID
void AMyPlayerState::SetSelectedCharacterId(int32 NewSelectedCharacterId)
{
	if (HasAuthority() && AbilitySystemComponent)
	{
		const FGameplayTag CharacterTags[] = {
			MyGameplayTags::Character_Player_Nefer,
			MyGameplayTags::Character_Player_Inpu,
			MyGameplayTags::Character_Player_Heru,
		};
		for (const FGameplayTag CharacterTag : CharacterTags)
		{
			if (AbilitySystemComponent->HasMatchingGameplayTag(CharacterTag))
			{
				AbilitySystemComponent->RemoveLooseGameplayTag(
					CharacterTag,
					1,
					EGameplayTagReplicationState::TagOnly);
			}
		}

		const FGameplayTag NewCharacterTag = MyGameplayTags::GetPlayerCharacterTag(NewSelectedCharacterId);
		if (NewCharacterTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(
				NewCharacterTag,
				1,
				EGameplayTagReplicationState::TagOnly);
		}
	}

    SelectedCharacterId = NewSelectedCharacterId;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 인증된 유저 정보를 초기화하는 함수
void AMyPlayerState::ClearAuthenticatedUser()
{
	UserIndex = -1;
	Username.Empty();
	bAuthVerified = false;
	SetSelectedCharacterId(-1);
}
