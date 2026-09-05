////////////////////////////
//! \page MyPlayerState.h
//! \brief ReDuat(가제) 플레이어의 AbilitySystemComponent와 AttributeSet을 소유하는 PlayerState 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "Player/Types/PlayerLifeTypes.h"
#include "MyPlayerState.generated.h"

class UAbilitySystemComponent;
class UMyAttributeSet;
class UMyInventoryComponent;
struct FOnAttributeChangeData;

//! 레벨/경험치 값이 변경될 때(서버 갱신 또는 클라이언트 복제 수신) UI 등에 알리는 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnPlayerLevelDataChangedSignature);
//! 스킬포인트/스킬 강화 레벨이 변경될 때(서버 갱신 또는 클라이언트 복제 수신) UI·스킬 컴포넌트에 알리는 델리게이트
DECLARE_MULTICAST_DELEGATE(FOnPlayerSkillProgressChangedSignature);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerLifeStateChangedSignature,
	EPlayerLifeState /*OldLifeState*/,
	EPlayerLifeState /*NewLifeState*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerCurseStateChangedSignature,
	bool /*bWasCursed*/,
	bool /*bIsCursed*/);

////////////////////////////
//! \struct FMySkillLevelEntry
//! \author HanUl
//! \brief 한 스킬의 현재 강화 레벨을 SkillId로 저장하는 복제용 엔트리다. 엔트리가 없으면 1단계로 간주한다.
USTRUCT(BlueprintType)
struct FMySkillLevelEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Player|Skill")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "Player|Skill")
	int32 Level = 1;
};

////////////////////////////
//! \class AMyPlayerState
//! \brief Re Duat 플레이어의 ASC와 AttributeSet을 소유하는 PlayerState이다.
//! \note 플레이어 Character는 ASC를 직접 생성하지 않고 이 PlayerState의 ASC를 Avatar로 사용한다.
UCLASS()
class PROJECTP_API AMyPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AMyPlayerState();

	virtual void PostInitializeComponents() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UMyAttributeSet* GetMyAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "Player|Inventory")
	UMyInventoryComponent* GetInventoryComponent() const;

	UFUNCTION(BlueprintPure, Category = "Player|Auth")
	int32 GetUserIndex() const;

	UFUNCTION(BlueprintPure, Category = "Player|Auth")
	const FString& GetUsername() const;

	UFUNCTION(BlueprintPure, Category = "Player|Auth")
	bool IsAuthVerified() const;

	UFUNCTION(BlueprintPure, Category = "Player|Life")
	EPlayerLifeState GetLifeState() const;

	UFUNCTION(BlueprintPure, Category = "Player|Life")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Player|Life")
	bool IsDead() const;

	//! 서버에서 플레이어의 생명 상태를 변경한다. 부활 구현에서도 이 함수를 공용 진입점으로 사용한다.
	void SetLifeState(EPlayerLifeState NewLifeState);

	UFUNCTION(BlueprintPure, Category = "Player|Curse")
	bool IsCursed() const;

	//! 서버에서 플레이어의 저주 상태를 변경한다.
	void SetCurseState(bool bNewCursed);

    UFUNCTION(BlueprintPure, Category = "Player|Character")
    int32 GetSelectedCharacterId() const;

	UFUNCTION(BlueprintPure, Category = "Player|Level")
	int32 GetCharacterLevel() const;

	////////////////////////////
	//! \brief 서버에서 캐릭터 레벨 값을 갱신한다. 레벨 스탯 적용은 APlayerCharacterBase::SetCharacterLevel이 담당한다.
	//! \param NewCharacterLevel 새 캐릭터 레벨(1 이상)
	void SetCharacterLevel(int32 NewCharacterLevel);

	UFUNCTION(BlueprintPure, Category = "Player|Level")
	int32 GetCharacterExp() const;

	////////////////////////////
	//! \brief 서버에서 현재 레벨의 누적 경험치 값을 갱신한다. 획득/레벨업 판정은 APlayerCharacterBase::AddExperience가 담당한다.
	//! \param NewCharacterExp 새 누적 경험치(0 이상)
	void SetCharacterExp(int32 NewCharacterExp);

	UFUNCTION(BlueprintPure, Category = "Player|Skill")
	int32 GetSkillPoints() const;

	////////////////////////////
	//! \brief 서버에서 스킬포인트를 가감한다(레벨업 지급/강화 소비 등). 결과는 0 미만으로 내려가지 않는다.
	//! \param Delta 가감할 스킬포인트(음수 허용)
	void AddSkillPoints(int32 Delta);

	////////////////////////////
	//! \brief 서버에서 스킬포인트를 소비 시도한다. 잔량이 부족하면 아무것도 소비하지 않는다.
	//! \param Amount 소비할 스킬포인트(양수)
	//! \return 소비에 성공하면 true
	bool ConsumeSkillPoints(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Player|Skill")
	int32 GetSkillLevel(FName SkillId) const;

	////////////////////////////
	//! \brief 서버에서 특정 스킬의 현재 강화 레벨을 설정한다(1 이상으로 보정).
	//! \param SkillId 대상 스킬 ID
	//! \param NewLevel 새 강화 레벨
	void SetSkillLevel(FName SkillId, int32 NewLevel);

	//! \brief 지금까지 지급된 스킬포인트 총량(현재 보유 + 강화로 사용한 분)을 반환한다. 누적 지급 상한 판정에 사용한다.
	UFUNCTION(BlueprintPure, Category = "Player|Skill")
	int32 GetTotalSkillPointsGranted() const;

	//! 레벨/경험치 변경 알림. 서버 갱신과 클라이언트 OnRep 양쪽에서 브로드캐스트된다.
	FOnPlayerLevelDataChangedSignature OnLevelDataChanged;

	//! 스킬포인트/스킬 강화 레벨 변경 알림. 서버 갱신과 클라이언트 OnRep 양쪽에서 브로드캐스트된다.
	FOnPlayerSkillProgressChangedSignature OnSkillProgressChanged;

	//! 생명 상태 변경 알림. 서버 갱신과 클라이언트 OnRep 양쪽에서 브로드캐스트된다.
	FOnPlayerLifeStateChangedSignature OnLifeStateChanged;

	//! 저주 상태 변경 알림. 서버 갱신과 클라이언트 OnRep 양쪽에서 브로드캐스트된다.
	FOnPlayerCurseStateChangedSignature OnCurseStateChanged;

    void SetUsername(const FString& NewUsername);
	void SetAuthenticatedUser(int32 NewUserIndex, const FString& NewUsername);
    void SetSelectedCharacterId(int32 NewSelectedCharacterId);
	void ClearAuthenticatedUser();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMyAttributeSet> AttributeSet;

	//! 메소/아이템을 서버 권위로 관리하는 인벤토리 컴포넌트. 폰 사망/리스폰과 무관하게 유지된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMyInventoryComponent> InventoryComponent;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player|Auth", meta = (AllowPrivateAccess = "true"))
	int32 UserIndex = -1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player|Auth", meta = (AllowPrivateAccess = "true"))
	FString Username;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player|Auth", meta = (AllowPrivateAccess = "true"))
	bool bAuthVerified = false;

	//! PlayerState가 소유하는 서버 권위 생명 상태. Pawn 교체나 재접속과 무관하게 유지된다.
	UPROPERTY(ReplicatedUsing = OnRep_LifeState, BlueprintReadOnly, Category = "Player|Life", meta = (AllowPrivateAccess = "true"))
	EPlayerLifeState LifeState = EPlayerLifeState::Alive;

	UPROPERTY(ReplicatedUsing = OnRep_IsCursed, BlueprintReadOnly, Category = "Player|Curse", meta = (AllowPrivateAccess = "true"))
	bool bIsCursed = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player|Character", meta = (AllowPrivateAccess = "true"))
    int32 SelectedCharacterId = -1;

	//! 캐릭터 레벨. 폰 사망/리스폰과 무관하게 유지되며, 레벨 스탯 CurveTable의 X축 값으로 사용된다.
	UPROPERTY(ReplicatedUsing = OnRep_CharacterLevel, BlueprintReadOnly, Category = "Player|Level", meta = (AllowPrivateAccess = "true"))
	int32 CharacterLevel = 1;

	//! 현재 레벨에서 누적된 경험치. 레벨업 시 요구량을 차감하고 남은 값이 이월된다.
	UPROPERTY(ReplicatedUsing = OnRep_CharacterExp, BlueprintReadOnly, Category = "Player|Level", meta = (AllowPrivateAccess = "true"))
	int32 CharacterExp = 0;

	//! 미사용 스킬포인트. 레벨업 시 지급되고 스킬 강화 시 소비된다. 폰 사망/리스폰과 무관하게 유지된다.
	UPROPERTY(ReplicatedUsing = OnRep_SkillProgress, BlueprintReadOnly, Category = "Player|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillPoints = 0;

	//! 스킬별 현재 강화 레벨(기본 1). 엔트리가 없는 스킬은 1단계로 간주한다. 폰 사망/리스폰과 무관하게 유지된다.
	UPROPERTY(ReplicatedUsing = OnRep_SkillProgress, BlueprintReadOnly, Category = "Player|Skill", meta = (AllowPrivateAccess = "true"))
	TArray<FMySkillLevelEntry> SkillLevels;

	UFUNCTION()
	void OnRep_CharacterLevel();

	UFUNCTION()
	void OnRep_CharacterExp();

	UFUNCTION()
	void OnRep_SkillProgress();

	void BindHealthChangedDelegate();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void BindCurseGaugeChangedDelegate();
	void HandleCurseGaugeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void OnRep_LifeState(EPlayerLifeState OldLifeState);

	UFUNCTION()
	void OnRep_IsCursed(bool bWasCursed);

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle CurseGaugeChangedDelegateHandle;
};
