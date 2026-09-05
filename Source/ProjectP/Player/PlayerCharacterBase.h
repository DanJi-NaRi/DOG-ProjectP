////////////////////////////
//! \file PlayerCharacterBase.h
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Types/PlayerLifeTypes.h"
#include "PlayerCharacterBase.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UCurveTable;
class UMyBasicControlComponent;
class UMySkillControlComponent;
class UMyAttributeSet;
class UMaterialInterface;
class UPlayerCameraComponent;
class UPlayerInteractionComponent;
class UPlayerMovementComponent;
class UWeightCarryComponent;
class USpringArmComponent;
class AMyPlayerState;
struct FRealCurve;
struct FOnAttributeChangeData;



////////////////////////////
//! \class APlayerCharacterBase
//! \authors HanUl, 박준혁, 장효제
//! \brief 플레이어 Character의 공통 Avatar 기반 클래스다.
//! \note 스킬/인디케이터/캐릭터별 전투 로직은 별도 컴포넌트나 Ability에서 담당한다.
UCLASS()
class PROJECTP_API APlayerCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APlayerCharacterBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;


	UFUNCTION(BlueprintPure, Category = "Player|MyGAS|Attribute")
	UMyAttributeSet* GetMyAttributeSet() const;


	UFUNCTION(BlueprintPure, Category = "Player|MyGAS|Attribute")
	float GetHealth() const;


	UFUNCTION(BlueprintPure, Category = "Player|MyGAS|Attribute")
	float GetMaxHealth() const;


	UFUNCTION(BlueprintPure, Category = "Player|MyGAS|Attribute")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category = "Player|Life")
	EPlayerLifeState GetLifeState() const;

	UFUNCTION(BlueprintPure, Category = "Player|Life")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Player|Life")
	bool IsDead() const;

	//! 서버에서 기존 Health 기반 사망 경로를 통해 플레이어를 즉시 사망 처리한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Life")
	void ForceKill();

	//! 서버에서 사망 순간 위치로 이동한 뒤 지정한 최대 체력 비율로 동일 Pawn을 부활시킨다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Life")
	bool ReviveAtLastDeathLocation(float ReviveHealthPercent);


	UFUNCTION(BlueprintCallable, Category = "Player|Reconnect")
	void SetReconnectInactive(bool bNewInactive);


	UFUNCTION(BlueprintPure, Category = "Player|Reconnect")
	bool IsReconnectInactive() const;


	UFUNCTION(BlueprintCallable, Category = "Player|Character")
	void SetSelectedCharacterId(int32 NewSelectedCharacterId);


    UFUNCTION(BlueprintCallable, Category = "Player|Character")
    void PreviewSelectedCharacterMaterial(int32 PreviewSelectedCharacterId);


	UFUNCTION(BlueprintPure, Category = "Player|Character")
	int32 GetSelectedCharacterId() const;

	////////////////////////////
	//! \brief 서버에서 캐릭터 레벨을 설정하고 레벨 스탯 CurveTable 값을 어트리뷰트에 적용한다. 체력은 비율을 유지한다.
	//! \param NewLevel 새 캐릭터 레벨(1~MaxCharacterLevel로 보정)
	UFUNCTION(BlueprintCallable, Category = "Player|Level")
	void SetCharacterLevel(int32 NewLevel);

	UFUNCTION(BlueprintPure, Category = "Player|Level")
	int32 GetCharacterLevel() const;

	////////////////////////////
	//! \brief 서버에서 경험치를 추가하고, 레벨 스탯 CurveTable의 Exp 요구량을 채우면 레벨업한다.
	//!        잔여 경험치는 이월되며 한 번에 여러 레벨을 올릴 수 있다. 최대 레벨 도달 시 잔여 경험치는 버려진다.
	//! \param Amount 추가할 경험치(0 이하 무시)
	UFUNCTION(BlueprintCallable, Category = "Player|Level")
	void AddExperience(int32 Amount);

	////////////////////////////
	//! \brief 현재 레벨에서 다음 레벨까지 필요한 경험치 요구량을 반환한다.
	//! \return 요구 경험치. 최대 레벨이거나 Exp 커브가 없으면 0
	UFUNCTION(BlueprintPure, Category = "Player|Level")
	int32 GetExpRequiredForNextLevel() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

public:


	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Camera
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;


	// Player Components
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component")
	TObjectPtr<UPlayerMovementComponent> PlayerMovementComponent;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component")
	TObjectPtr<UPlayerCameraComponent> PlayerCameraComponent;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component")
	TObjectPtr<UMyBasicControlComponent> MyBasicControlComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component")
	TObjectPtr<UMySkillControlComponent> MySkillControlComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component")
	TObjectPtr<UPlayerInteractionComponent> PlayerInteractionComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Component")
	TObjectPtr<UWeightCarryComponent> WeightCarryComponent;

private:

	void InitAbilityActorInfo();
	void BindLifeStateDelegate(AMyPlayerState* MyPlayerState);
	void UnbindLifeStateDelegate();
	void HandleLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState);
	void ApplyLifeState(EPlayerLifeState NewLifeState);
	void BindCurseStateDelegate(AMyPlayerState* MyPlayerState);
	void UnbindCurseStateDelegate();
	void HandleCurseStateChanged(bool bWasCursed, bool bIsCursed);
	void ApplyCurseState(bool bIsCursed);
	void HandleTemporaryCurseExpired();
	void ResetCurseStateOnServer();
	void ApplyDefaultCharacterTagsToAbilitySystem(UAbilitySystemComponent* ASC);
	void ApplySelectedCharacterMaterial();
	UMaterialInterface* ResolveSelectedCharacterMaterial(int32 InSelectedCharacterId) const;
	void ApplyLevelStatsFromTable(int32 Level);
	const FRealCurve* FindLevelCurveBySuffix(const TCHAR* Suffix) const;

	// MoveSpeed 어트리뷰트 → CharacterMovement(MaxWalkSpeed) 연결
	void BindMoveSpeedToMovement(UAbilitySystemComponent* InASC);
	void ApplyMoveSpeedToMovement(float NewMoveSpeed);
	void OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data);

	//! MoveSpeed 어트리뷰트 변경 델리게이트 핸들 (리스폰/재초기화 시 중복 바인딩 방지)
	FDelegateHandle MoveSpeedChangedDelegateHandle;

	TWeakObjectPtr<AMyPlayerState> BoundLifeStatePlayerState;
	FDelegateHandle LifeStateChangedDelegateHandle;
	bool bDeathStateApplied = false;
	FTransform LastDeathTransform = FTransform::Identity;
	bool bHasLastDeathTransform = false;
	TWeakObjectPtr<AMyPlayerState> BoundCurseStatePlayerState;
	FDelegateHandle CurseStateChangedDelegateHandle;
	FTimerHandle TemporaryCurseTimerHandle;
	bool bCurseStateApplied = false;

	UFUNCTION()
	void OnRep_SelectedCharacterId();

	// 카메라 기본값
	float DefaultArmLength = 2000.0f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player|Reconnect", meta = (AllowPrivateAccess = "true"))
	bool bReconnectInactive = false;

protected:
	//! \brief 저주 상태의 임시 행동 정지 테스트 지속시간(초).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Curse", meta = (ClampMin = "0.1"))
	float CurseTestDuration = 10.0f;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedCharacterId, BlueprintReadOnly, Category = "Player|Character")
	int32 SelectedCharacterId = -1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|Material")
	FName CharacterMaterialSlotName = TEXT("Halo");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|Material")
	TObjectPtr<UMaterialInterface> Character100Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|Material")
	TObjectPtr<UMaterialInterface> Character200Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|Material")
	TObjectPtr<UMaterialInterface> Character300Material;

	//! \brief Dash Ghost에 공통으로 적용할 캐릭터별 머티리얼이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|DashTrail")
	TObjectPtr<UMaterialInterface> DashGhostMaterial;

	//! \brief Dash Ghost 머티리얼을 적용할 Skeletal Mesh 머티리얼 슬롯 인덱스 목록이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|DashTrail", meta = (ClampMin = "0"))
	TArray<int32> DashGhostMaterialIndices;

	//! \brief 이 캐릭터의 레벨 스탯 CurveTable. 캐릭터별 BP에서 자기 테이블을 지정한다.
	//!        행 이름은 접미사 MaxHealth / Attack / Defense 로 찾는다(예: Nefer.MaxHealth).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|LevelStat")
	TObjectPtr<UCurveTable> LevelStatTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Character|LevelStat", meta = (ClampMin = "1"))
	int32 MaxCharacterLevel = 15;

	//! 캐릭터 레벨업 1회당 지급할 스킬포인트 수. 스킬 강화(Definition 교체)에 소비된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Skill", meta = (ClampMin = "0"))
	int32 SkillPointsPerLevelUp = 1;

	//! 누적으로 지급 가능한 스킬포인트 총량. 기본 8 = 강화 대상 4스킬 × 각 2단계 업그레이드. 누적 지급이 이 값에 도달하면 레벨업해도 더 지급하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Skill", meta = (ClampMin = "0"))
	int32 MaxTotalSkillPoints = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming|Identity")
	FGameplayTagContainer DefaultCharacterTags;

	//! \brief PlayerState가 소유한 ASC를 런타임에 캐시한 참조이다.
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	//! \brief PlayerState가 소유한 AttributeSet을 런타임에 캐시한 참조이다.
	UPROPERTY(Transient)
	TObjectPtr<UMyAttributeSet> CachedAttributeSet;
};
