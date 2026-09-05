#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "../../GAS/MySkillDebugShape.h"
#include "MySkillControlComponent.generated.h"

class ACharacter;
class APlayerController;
class UAbilitySystemComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputComponent;
class UMySkillDefinitionDataAsset;
class UMySkillUpgradeLadderDataAsset;
class UPlayerMovementComponent;
class AMyPlayerState;

UENUM(BlueprintType)
enum class EMySkillInputRouteEvent : uint8
{
	Pressed,
	Released,
	Canceled
};

////////////////////////////
//! \struct FMySkillInputContext
//! \author HanUl
//! \brief 스킬 입력 순간의 조준 방향, 대상 위치, 이동 방향을 GA로 전달하기 위한 공통 입력 컨텍스트다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMySkillInputContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	bool bHasAimYaw = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	bool bHasAimWorldLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	FVector AimWorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	bool bShouldFaceAimDirection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	bool bHasMoveDirection = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	FVector MoveDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Skill|Input")
	bool bRequiresServerActivationContext = false;
};

////////////////////////////
//! \struct FMySkillSlotSpec
//! \author HanUl
//! \brief SkillControlComponent에서 한 입력 슬롯에 연결할 SkillDefinition과 Grant 결과를 저장한다.
USTRUCT(BlueprintType)
struct PROJECTP_API FMySkillSlotSpec
{
	GENERATED_BODY()

public:
	const UMySkillDefinitionDataAsset* GetSkillDefinition() const;
	FGameplayTag GetInputTag() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UMySkillDefinitionDataAsset> SkillDefinition;

	UPROPERTY(Transient)
	FGameplayAbilitySpecHandle GrantedAbilityHandle;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECTP_API UMySkillControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMySkillControlComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeSkillControl(UAbilitySystemComponent* InAbilitySystemComponent);
	void BindSkillInput(UInputComponent* PlayerInputComponent);

	//! \brief 서버가 판정한 스킬 디버그 도형을 소유 클라이언트 화면에 표시한다. MySkillDebugDraw::DrawShapeForOwner가 호출한다.
	UFUNCTION(Client, Unreliable)
	void ClientDrawSkillDebugShape(FMySkillDebugShape Shape);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	FMySkillSlotSpec BasicAttackSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	FMySkillSlotSpec QSkillSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	FMySkillSlotSpec ESkillSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	FMySkillSlotSpec RSkillSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	FMySkillSlotSpec CSkillSlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Slots")
	FMySkillSlotSpec MoveSkillSlot;

	//! \brief 이 캐릭터의 스킬 강화 사다리(SkillId → 단계별 Definition). Q·E·R·C 슬롯의 레벨 교체에 사용한다. 캐릭터 BP에서 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Upgrade")
	TObjectPtr<UMySkillUpgradeLadderDataAsset> UpgradeLadder;

	// Skill Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill_Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill_Q;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill_E;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill_R;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill_C; // 궁극기
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill_Move; // 이동기


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Aim")
	TEnumAsByte<ECollisionChannel> MouseAimTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Aim", meta = (ClampMin = "0.0"))
	float MouseAimFacingInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Aim", meta = (ClampMin = "0.0"))
	float MouseAimFacingToleranceDegrees = 1.0f;

private:

	void GrantSkillSlots();
	void GrantSkillSlot(FMySkillSlotSpec& SkillSlot, const TCHAR* SlotDebugName);
	void ClearGrantedSkillSlots();
	void ClearGrantedSkillSlot(FMySkillSlotSpec& SkillSlot);
	void BindSkillSlotInput(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* InputAction, FMySkillSlotSpec& SkillSlot);
	bool RouteSkillInputSlot(const FMySkillSlotSpec& SkillSlot, EMySkillInputRouteEvent InputEvent);
	bool RouteSkillInputByInputTag(FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, const FMySkillInputContext& InputContext);
	bool ApplySkillInputEventByInputTag(FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, bool bActivateOnPressed, const FMySkillInputContext& InputContext);
	bool ApplySkillInputEventToSpec(FGameplayAbilitySpec& AbilitySpec, FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, bool bActivateOnPressed, const FMySkillInputContext& InputContext);
		bool TryActivateAbilityWithInputContext(FGameplayAbilitySpec& AbilitySpec, FGameplayTag InputTag, const FMySkillInputContext& InputContext);
		FGameplayEventData BuildGameplayEventData(FGameplayTag InputTag, const FMySkillInputContext& InputContext) const;
		bool IsSkillInputBlocked() const;
		bool IsSkillSlotOnCooldown(const FMySkillSlotSpec& SkillSlot) const;
		bool IsInputTagOnCooldown(FGameplayTag InputTag) const;
		FMySkillInputContext BuildSkillInputContext(const FMySkillSlotSpec& SkillSlot) const;
		bool ResolveMoveDirectionContext(FMySkillInputContext& OutInputContext) const;
		bool ResolveMouseAimContext(FMySkillInputContext& OutInputContext) const;
		bool ResolveControllerForwardAimContext(FMySkillInputContext& OutInputContext) const;
		bool ResolveCurrentFacingAimContext(FMySkillInputContext& OutInputContext) const;
	bool ResolveMouseAimPoint(APlayerController* PlayerController, FVector& OutAimPoint) const;
	void RequestOwnerFacingFromInputContext(const FMySkillInputContext& InputContext);
	FGameplayAbilitySpec* FindAbilitySpecByInputTag(FGameplayTag InputTag) const;
	const FMySkillSlotSpec* FindSkillSlotByInputTag(FGameplayTag InputTag) const;
	bool CacheOwnerCharacter();

	//! \brief 스킬 입력도 조작이다. 형제 컴포넌트의 보고를 재사용한다.
	void ReportSkillInputAsPlayerInput();

	void HandleSkillPressed(const FInputActionValue& Value, FMySkillSlotSpec* SkillSlot);
	void HandleSkillReleased(const FInputActionValue& Value, FMySkillSlotSpec* SkillSlot);
	void HandleSkillCanceled(const FInputActionValue& Value, FMySkillSlotSpec* SkillSlot);

	// Skill Upgrade (스킬 강화 = 슬롯 Definition 단계 교체)
	bool IsUpgradeableSlot(const FMySkillSlotSpec* SkillSlot) const;
	bool IsUpgradeModifierHeld() const;
	void RequestSkillUpgrade(FGameplayTag InputTag);
	void PerformSkillUpgrade(FGameplayTag InputTag);
	void ApplySkillLevelDefinition(FMySkillSlotSpec& SkillSlot, const UMySkillDefinitionDataAsset* NewDefinition, const TCHAR* SlotDebugName);
	const UMySkillDefinitionDataAsset* ResolveLeveledDefinition(const FMySkillSlotSpec& SkillSlot, const UMySkillDefinitionDataAsset* BaseDefinition) const;
	void RefreshUpgradeableSlotDefinitionsFromProgress();
	AMyPlayerState* GetOwnerPlayerState() const;
	FMySkillSlotSpec* FindMutableSkillSlotByInputTag(FGameplayTag InputTag);
	void BindToSkillProgress();
	void UnbindFromSkillProgress();
	void HandleSkillProgressChanged();

	UFUNCTION(Server, Reliable)
	void ServerRouteSkillInputByInputTag(FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, FMySkillInputContext InputContext);

	UFUNCTION(Server, Reliable)
	void ServerRequestSkillUpgrade(FGameplayTag InputTag);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerMovementComponent> PlayerMovementComponent;

	//! 스킬 진행도(스킬포인트/레벨) 변경 델리게이트 바인딩 대상 PlayerState (중복 바인딩 방지)
	TWeakObjectPtr<AMyPlayerState> BoundSkillProgressPlayerState;
	FDelegateHandle SkillProgressChangedHandle;

};
