#include "MySkillControlComponent.h"
#include "MyBasicControlComponent.h"

#include "../../GAS/SkillData/MySkillDefinitionDataAsset.h"
#include "../../GAS/SkillData/MySkillUpgradeLadderDataAsset.h"
#include "../../GAS/MyPlayerState.h"
#include "../../MyGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "MyBasicControlComponent.h"
#include "../PlayerCharacterBase.h"
#include "PlayerMovementComponent.h"

namespace
{
	constexpr float MoveInputContextMaxAgeSeconds = 0.25f;

	// 준혁 추가 : HitResult의 위치 벡터(TraceEnd 등)는 FVector_NetQuantize라 서버 복제 시 각 성분이 정수로 반올림된다.
	//            정규화된 대쉬 방향(성분 0~1)을 그대로 실으면 카메라 기준 임의 각도가 정수 반올림으로 크게 뭉개져
	//            서버가 클라와 다른 방향으로 대쉬 → 궤적이 벌어지며 러버밴딩이 발생한다.
	//            방향을 크게 스케일(+정수화)해 실어 양자화 오차를 무의미하게 만들고 어빌리티에서 다시 정규화한다.
	constexpr float MoveDirectionEncodeScale = 10000.0f;

	////////////////////////////
	//! \author HanUl
	//! \brief Orbit 상태에서 Move(Dash)를 제외한 새 스킬 입력을 로컬에서 차단해야 하는지 확인한다.
	//! \param OwnerActor 스킬 입력 컴포넌트 소유 액터
	//! \param InputTag 입력 스킬 태그
	//! \return Orbit 중이고 Move 입력이 아니면 true
	bool ShouldBlockSkillPressedByOrbit(const AActor* OwnerActor, FGameplayTag InputTag)
	{
		if (!OwnerActor || InputTag == MyGameplayTags::Input_Skill_Move)
		{
			return false;
		}

		const UMyBasicControlComponent* BasicControlComponent = OwnerActor->FindComponentByClass<UMyBasicControlComponent>();
		return BasicControlComponent && BasicControlComponent->IsOrbitMode();
	}

	FPredictionKey GetActivationPredictionKeyForSpec(const FGameplayAbilitySpec& AbilitySpec)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TArray<UGameplayAbility*> AbilityInstances = AbilitySpec.GetAbilityInstances();
		const FGameplayAbilityActivationInfo& ActivationInfo = AbilityInstances.IsEmpty()
			? AbilitySpec.ActivationInfo
			: AbilityInstances.Last()->GetCurrentActivationInfoRef();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		return ActivationInfo.GetActivationPredictionKey();
	}

	EAbilityGenericReplicatedEvent::Type ToGenericReplicatedEvent(EMySkillInputRouteEvent InputEvent)
	{
		return InputEvent == EMySkillInputRouteEvent::Pressed
			? EAbilityGenericReplicatedEvent::InputPressed
			: EAbilityGenericReplicatedEvent::InputReleased;
	}
}

////////////////////////////
//! \author HanUl
//! \brief 슬롯에 연결된 SkillDefinition을 반환한다.
//! \param 없음
//! \return 연결된 SkillDefinition, 없으면 nullptr
const UMySkillDefinitionDataAsset* FMySkillSlotSpec::GetSkillDefinition() const
{
	return SkillDefinition;
}

////////////////////////////
//! \author HanUl
//! \brief 슬롯 SkillDefinition의 InputTag를 반환한다.
//! \param 없음
//! \return 입력 GameplayTag
FGameplayTag FMySkillSlotSpec::GetInputTag() const
{
	return SkillDefinition ? SkillDefinition->GetInputTag() : FGameplayTag();
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 제어 컴포넌트 기본값을 초기화한다.
//! \param 없음
//! \return 없음
UMySkillControlComponent::UMySkillControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 시작 시 필요한 초기화를 수행한다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::BeginPlay()
{
	Super::BeginPlay();
}

////////////////////////////
//! \author HanUl
//! \brief 컴포넌트 종료 시 이 컴포넌트가 부여한 스킬 Ability를 회수한다.
//! \param EndPlayReason 종료 사유
//! \return 없음
void UMySkillControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSkillProgress();
	ClearGrantedSkillSlots();
	Super::EndPlay(EndPlayReason);
}

////////////////////////////
//! \author HanUl
//! \brief ASC가 준비된 뒤 SkillDefinition 슬롯을 ASC에 Grant할 수 있도록 초기화한다.
//! \param InAbilitySystemComponent 스킬 Ability를 부여하고 발동할 ASC
//! \return 없음
void UMySkillControlComponent::InitializeSkillControl(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (CachedAbilitySystemComponent && CachedAbilitySystemComponent != InAbilitySystemComponent)
	{
		ClearGrantedSkillSlots();
	}

	CachedAbilitySystemComponent = InAbilitySystemComponent;
	CacheOwnerCharacter();
	GrantSkillSlots();
	BindToSkillProgress();
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 액션을 바인딩한다.
//! \param PlayerInputComponent 입력 바인딩 대상 컴포넌트
//! \return 없음
void UMySkillControlComponent::BindSkillInput(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	if (!CacheOwnerCharacter())
	{
		return;
	}

	if (IA_Skill_Basic)
	{
		BindSkillSlotInput(EIC, IA_Skill_Basic, BasicAttackSlot);
	}

	if (IA_Skill_Q)
	{
		BindSkillSlotInput(EIC, IA_Skill_Q, QSkillSlot);
	}

	if (IA_Skill_E)
	{
		BindSkillSlotInput(EIC, IA_Skill_E, ESkillSlot);
	}

	if (IA_Skill_R)
	{
		BindSkillSlotInput(EIC, IA_Skill_R, RSkillSlot);
	}

	if (IA_Skill_C)
	{
		BindSkillSlotInput(EIC, IA_Skill_C, CSkillSlot);
	}

	if (IA_Skill_Move)
	{
		BindSkillSlotInput(EIC, IA_Skill_Move, MoveSkillSlot);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 하나의 입력 액션을 스킬 슬롯의 Pressed/Released/Canceled 라우팅에 연결한다.
//! \param EnhancedInputComponent 입력 바인딩 대상 EnhancedInputComponent
//! \param InputAction 슬롯에 대응하는 입력 액션
//! \param SkillSlot 입력을 전달할 스킬 슬롯
//! \return 없음
void UMySkillControlComponent::BindSkillSlotInput(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* InputAction, FMySkillSlotSpec& SkillSlot)
{
	if (!EnhancedInputComponent || !InputAction)
	{
		return;
	}

	// 제자리에서 스킬만 눌러도 조작이다. 잠수로 보면 안 된다.
	EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Started, this, &UMySkillControlComponent::ReportSkillInputAsPlayerInput);
	EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Started, this, &UMySkillControlComponent::HandleSkillPressed, &SkillSlot);
	EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Completed, this, &UMySkillControlComponent::HandleSkillReleased, &SkillSlot);
	EnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Canceled, this, &UMySkillControlComponent::HandleSkillCanceled, &SkillSlot);
}

////////////////////////////
//! \author HanUl
//! \brief 모든 SkillDefinition 슬롯을 ASC에 부여한다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::GrantSkillSlots()
{
	if (!CachedAbilitySystemComponent || !CachedAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	GrantSkillSlot(BasicAttackSlot, TEXT("BasicAttack"));
	GrantSkillSlot(QSkillSlot, TEXT("Q"));
	GrantSkillSlot(ESkillSlot, TEXT("E"));
	GrantSkillSlot(RSkillSlot, TEXT("R"));
	GrantSkillSlot(CSkillSlot, TEXT("C"));
	GrantSkillSlot(MoveSkillSlot, TEXT("Move"));
}

////////////////////////////
//! \author HanUl
//! \brief 단일 SkillDefinition 슬롯을 ASC에 AbilitySpec으로 부여한다.
//! \param SkillSlot 부여할 스킬 슬롯
//! \param SlotDebugName 로그에 표시할 슬롯 이름
//! \return 없음
void UMySkillControlComponent::GrantSkillSlot(FMySkillSlotSpec& SkillSlot, const TCHAR* SlotDebugName)
{
	if (!CachedAbilitySystemComponent || !CachedAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	if (SkillSlot.GrantedAbilityHandle.IsValid())
	{
		return;
	}

	const UMySkillDefinitionDataAsset* SkillDefinition = SkillSlot.GetSkillDefinition();
	if (!SkillDefinition)
	{
		return;
	}

	// 강화 대상 슬롯이면 PlayerState에 저장된 현재 레벨의 Definition으로 부여한다(리스폰/재접속 시 강화 상태 유지).
	const UMySkillDefinitionDataAsset* LeveledDefinition = ResolveLeveledDefinition(SkillSlot, SkillDefinition);
	if (LeveledDefinition && LeveledDefinition != SkillDefinition)
	{
		SkillSlot.SkillDefinition = const_cast<UMySkillDefinitionDataAsset*>(LeveledDefinition);
		SkillDefinition = LeveledDefinition;
	}

	if (!SkillDefinition->IsValidDefinition())
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl grant skipped - invalid SkillDefinition. Owner: %s, Slot: %s, Definition: %s"),
			*GetNameSafe(GetOwner()),
			SlotDebugName,
			*GetNameSafe(SkillDefinition));
		return;
	}

	FGameplayAbilitySpec AbilitySpec(
		SkillDefinition->GetAbilityClass(),
		SkillDefinition->GetAbilityLevel(),
		INDEX_NONE,
		const_cast<UMySkillDefinitionDataAsset*>(SkillDefinition)
	);

	const FGameplayTag InputTag = SkillDefinition->GetInputTag();
	if (InputTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);
	}

	const FGameplayTag AbilityTag = SkillDefinition->GetAbilityTag();
	if (AbilityTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityTag);
	}

	const FGameplayTag CooldownTag = SkillDefinition->GetCooldownTag();
	if (CooldownTag.IsValid())
	{
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(CooldownTag);
	}

	SkillSlot.GrantedAbilityHandle = CachedAbilitySystemComponent->GiveAbility(AbilitySpec);

	UE_LOG(LogTemp, Log, TEXT("SkillControl granted skill - Owner: %s, Slot: %s, Definition: %s, Ability: %s, InputTag: %s, HandleValid: %s"),
		*GetNameSafe(GetOwner()),
		SlotDebugName,
		*GetNameSafe(SkillDefinition),
		*GetNameSafe(SkillDefinition->GetAbilityClass()),
		*InputTag.ToString(),
		SkillSlot.GrantedAbilityHandle.IsValid() ? TEXT("true") : TEXT("false"));
}

////////////////////////////
//! \author HanUl
//! \brief 이 컴포넌트가 부여한 모든 Ability를 ASC에서 제거한다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::ClearGrantedSkillSlots()
{
	if (!CachedAbilitySystemComponent || !CachedAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	ClearGrantedSkillSlot(BasicAttackSlot);
	ClearGrantedSkillSlot(QSkillSlot);
	ClearGrantedSkillSlot(ESkillSlot);
	ClearGrantedSkillSlot(RSkillSlot);
	ClearGrantedSkillSlot(CSkillSlot);
	ClearGrantedSkillSlot(MoveSkillSlot);
}

////////////////////////////
//! \author HanUl
//! \brief 단일 슬롯의 GrantedAbilityHandle을 ASC에서 제거한다.
//! \param SkillSlot 회수할 스킬 슬롯
//! \return 없음
void UMySkillControlComponent::ClearGrantedSkillSlot(FMySkillSlotSpec& SkillSlot)
{
	if (!CachedAbilitySystemComponent || !SkillSlot.GrantedAbilityHandle.IsValid())
	{
		return;
	}

	CachedAbilitySystemComponent->ClearAbility(SkillSlot.GrantedAbilityHandle);
	SkillSlot.GrantedAbilityHandle = FGameplayAbilitySpecHandle();
}

////////////////////////////
//! \author HanUl
//! \brief 슬롯에 연결된 Ability에 입력 이벤트를 라우팅한다.
//! \param SkillSlot 입력을 전달할 스킬 슬롯
//! \param InputEvent 전달할 입력 이벤트
//! \return 입력 이벤트를 처리하거나 서버에 요청했으면 true
bool UMySkillControlComponent::RouteSkillInputSlot(const FMySkillSlotSpec& SkillSlot, EMySkillInputRouteEvent InputEvent)
{
	if (InputEvent == EMySkillInputRouteEvent::Pressed && IsSkillInputBlocked())
	{
		return true;
	}

	const FGameplayTag InputTag = SkillSlot.GetInputTag();
	if (!InputTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl input route failed - InputTag is invalid. Owner: %s, Definition: %s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(SkillSlot.GetSkillDefinition()));
		return false;
	}

	if (InputEvent == EMySkillInputRouteEvent::Pressed && ShouldBlockSkillPressedByOrbit(GetOwner(), InputTag))
	{
		return true;
	}

	if (InputEvent == EMySkillInputRouteEvent::Pressed && IsSkillSlotOnCooldown(SkillSlot))
	{
		UE_LOG(LogTemp, Log, TEXT("SkillControl input ignored by cooldown - Owner: %s, InputTag: %s, Definition: %s"),
			*GetNameSafe(GetOwner()),
			*InputTag.ToString(),
			*GetNameSafe(SkillSlot.GetSkillDefinition()));
		return true;
	}

	const FMySkillInputContext InputContext = BuildSkillInputContext(SkillSlot);
	return RouteSkillInputByInputTag(InputTag, InputEvent, InputContext);
}

////////////////////////////
//! \author HanUl
//! \brief 입력 태그에 해당하는 Ability에 입력 이벤트를 전달하고 서버 권한에 동기화한다.
//! \param InputTag 입력을 전달할 스킬 입력 태그
//! \param InputEvent 전달할 입력 이벤트
//! \param InputContext 입력 순간의 조준 컨텍스트
//! \return 입력 이벤트를 처리하거나 서버에 요청했으면 true
bool UMySkillControlComponent::RouteSkillInputByInputTag(FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, const FMySkillInputContext& InputContext)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	if (!CachedAbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl input route failed - ASC is null. Owner: %s, InputTag: %s"),
			*GetNameSafe(GetOwner()),
			*InputTag.ToString());
		return false;
	}

	if (InputEvent == EMySkillInputRouteEvent::Pressed && IsSkillInputBlocked())
	{
		return true;
	}

	if (InputEvent == EMySkillInputRouteEvent::Pressed && IsInputTagOnCooldown(InputTag))
	{
		UE_LOG(LogTemp, Log, TEXT("SkillControl input route ignored by cooldown - Owner: %s, InputTag: %s"),
			*GetNameSafe(GetOwner()),
			*InputTag.ToString());
		return true;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
		const UGameplayAbility* Ability = AbilitySpec ? AbilitySpec->Ability.Get() : nullptr;
		const EGameplayAbilityNetExecutionPolicy::Type NetExecutionPolicy = Ability
			? Ability->GetNetExecutionPolicy()
			: EGameplayAbilityNetExecutionPolicy::ServerInitiated;
		const bool bCanActivateLocally =
			NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted
			|| NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly;
		const bool bActivateLocallyOnPressed = InputEvent == EMySkillInputRouteEvent::Pressed && bCanActivateLocally;
		const bool bHandledLocally = AbilitySpec
			? ApplySkillInputEventToSpec(*AbilitySpec, InputTag, InputEvent, bActivateLocallyOnPressed, InputContext)
			: false;
		bool bServerRouteSent = false;
		if (!bHandledLocally || InputEvent != EMySkillInputRouteEvent::Pressed)
		{
			ServerRouteSkillInputByInputTag(InputTag, InputEvent, InputContext);
			bServerRouteSent = true;
		}

		if (!bServerRouteSent && InputEvent == EMySkillInputRouteEvent::Pressed && !bCanActivateLocally)
		{
			ServerRouteSkillInputByInputTag(InputTag, InputEvent, InputContext);
		}
		return true;
	}

	return ApplySkillInputEventByInputTag(InputTag, InputEvent, true, InputContext);
}

////////////////////////////
//! \author HanUl
//! \brief 입력 태그에 해당하는 AbilitySpec에 입력 이벤트를 적용한다.
//! \param InputTag 입력을 전달할 스킬 입력 태그
//! \param InputEvent 전달할 입력 이벤트
//! \param bActivateOnPressed Pressed 입력에서 비활성 Ability를 발동할지 여부
//! \param InputContext 입력 순간의 조준 컨텍스트
//! \return AbilitySpec에 입력 이벤트를 적용했으면 true
bool UMySkillControlComponent::ApplySkillInputEventByInputTag(FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, bool bActivateOnPressed, const FMySkillInputContext& InputContext)
{
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (!AbilitySpec)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl input route failed - ability spec not found. Owner: %s, InputTag: %s"),
			*GetNameSafe(GetOwner()),
			*InputTag.ToString());
		return false;
	}

	return ApplySkillInputEventToSpec(*AbilitySpec, InputTag, InputEvent, bActivateOnPressed, InputContext);
}

////////////////////////////
//! \author HanUl
//! \brief AbilitySpec에 Pressed/Released 입력 상태와 GAS generic input event를 반영한다.
//! \param AbilitySpec 입력을 적용할 AbilitySpec
//! \param InputTag 입력을 전달할 스킬 입력 태그
//! \param InputEvent 전달할 입력 이벤트
//! \param bActivateOnPressed Pressed 입력에서 비활성 Ability를 발동할지 여부
//! \param InputContext 입력 순간의 조준 컨텍스트
//! \return 입력 이벤트를 적용했으면 true
bool UMySkillControlComponent::ApplySkillInputEventToSpec(FGameplayAbilitySpec& AbilitySpec, FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, bool bActivateOnPressed, const FMySkillInputContext& InputContext)
{
	if (!CachedAbilitySystemComponent || !AbilitySpec.Handle.IsValid())
	{
		return false;
	}

	const bool bWasActive = AbilitySpec.IsActive();
	if (InputEvent == EMySkillInputRouteEvent::Pressed)
	{
		if (IsSkillInputBlocked())
		{
			UE_LOG(LogTemp, Log, TEXT("SkillControl input blocked by skill state - Owner: %s, InputTag: %s"),
				*GetNameSafe(GetOwner()),
				*InputTag.ToString());
			return true;
		}

		if (InputContext.bShouldFaceAimDirection)
		{
			RequestOwnerFacingFromInputContext(InputContext);
		}

		CachedAbilitySystemComponent->AbilitySpecInputPressed(AbilitySpec);

		if (bWasActive)
		{
			CachedAbilitySystemComponent->InvokeReplicatedEvent(
				ToGenericReplicatedEvent(InputEvent),
				AbilitySpec.Handle,
				GetActivationPredictionKeyForSpec(AbilitySpec)
			);
			return true;
		}

		return bActivateOnPressed
			? TryActivateAbilityWithInputContext(AbilitySpec, InputTag, InputContext)
			: true;
	}

	CachedAbilitySystemComponent->AbilitySpecInputReleased(AbilitySpec);
	if (bWasActive)
	{
		CachedAbilitySystemComponent->InvokeReplicatedEvent(
			ToGenericReplicatedEvent(InputEvent),
			AbilitySpec.Handle,
			GetActivationPredictionKeyForSpec(AbilitySpec)
		);
	}

	return true;
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 이동 방향이 필요한 LocalPredicted 스킬을 예측 클라에서도 EventData로 발동(서버 동기 전달)
//! \brief AbilitySpec을 입력 컨텍스트와 함께 발동한다.
//! \param AbilitySpec 발동할 AbilitySpec
//! \param InputTag 입력 GameplayTag
//! \param InputContext 입력 순간의 조준 컨텍스트
//! \return 발동 요청에 성공하면 true
bool UMySkillControlComponent::TryActivateAbilityWithInputContext(FGameplayAbilitySpec& AbilitySpec, FGameplayTag InputTag, const FMySkillInputContext& InputContext)
{
	if (!CachedAbilitySystemComponent || !AbilitySpec.Handle.IsValid())
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	const bool bHasActivationContext =
		InputContext.bHasAimYaw
		|| InputContext.bHasAimWorldLocation
		|| InputContext.bHasMoveDirection
		|| InputContext.bRequiresServerActivationContext;

	// 준혁 수정 : 대쉬처럼 이동 방향이 필요한 LocalPredicted 스킬은 예측 클라이언트에서도 방향을 활성화 EventData에 실어 발동한다.
	//            그래야 GAS가 ServerTryActivateAbilityWithEventData로 방향을 활성화와 동시에 서버에 전달하고,
	//            서버가 같은 방향으로 루트모션을 즉시 시작해 예측 정합이 유지된다(지연 TargetData 복제 경로 제거).
	// HanUl 수정 : 조준(마우스) 지점을 쓰는 스킬은 이동 입력이 없어도 조준 지점을 서버에 실어보내야 한다.
	//            (이동 방향 유무로 조준 데이터 전달이 갈리면, WASD 미입력 시 서버에 조준이 안 가 facing으로 폴백된다.)
	if (OwnerActor && !OwnerActor->HasAuthority() && (InputContext.bHasMoveDirection || InputContext.bHasAimWorldLocation))
	{
		FGameplayEventData EventData = BuildGameplayEventData(InputTag, InputContext);
		return CachedAbilitySystemComponent->InternalTryActivateAbility(
			AbilitySpec.Handle,
			FPredictionKey(),
			nullptr,
			nullptr,
			&EventData
		);
	}

	if (OwnerActor && !OwnerActor->HasAuthority() && InputContext.bRequiresServerActivationContext)
	{
		return false;
	}

	if (OwnerActor && OwnerActor->HasAuthority() && bHasActivationContext)
	{
		FGameplayEventData EventData = BuildGameplayEventData(InputTag, InputContext);
		return CachedAbilitySystemComponent->InternalTryActivateAbility(
			AbilitySpec.Handle,
			FPredictionKey(),
			nullptr,
			nullptr,
			&EventData
		);
	}

	return CachedAbilitySystemComponent->TryActivateAbility(AbilitySpec.Handle);
}

////////////////////////////
//! \author HanUl
//! \brief ASC에 다른 비활성 스킬 발동을 막는 상태 태그가 있는지 확인한다.
//! \param 없음
//! \return 스킬 입력 차단 상태이면 true
bool UMySkillControlComponent::IsSkillInputBlocked() const
{
	const APlayerCharacterBase* PlayerCharacter = Cast<APlayerCharacterBase>(GetOwner());
	if (PlayerCharacter && PlayerCharacter->IsDead())
	{
		return true;
	}

	return CachedAbilitySystemComponent
		&& (CachedAbilitySystemComponent->HasMatchingGameplayTag(MyGameplayTags::State_Player_Dead)
			|| CachedAbilitySystemComponent->HasMatchingGameplayTag(MyGameplayTags::State_Skill_BlockSkillInput));
}

////////////////////////////
//! \author HanUl
//! \brief 슬롯 SkillDefinition의 쿨다운 태그가 현재 ASC에 적용되어 있는지 확인한다.
//! \param SkillSlot 확인할 스킬 슬롯
//! \return 쿨타임 중이면 true
bool UMySkillControlComponent::IsSkillSlotOnCooldown(const FMySkillSlotSpec& SkillSlot) const
{
	if (!CachedAbilitySystemComponent)
	{
		return false;
	}

	if (&SkillSlot == &MoveSkillSlot)
	{
		return false;
	}

	const UMySkillDefinitionDataAsset* SkillDefinition = SkillSlot.GetSkillDefinition();
	if (!SkillDefinition || SkillDefinition->GetCooldownDuration() <= 0.0f)
	{
		return false;
	}

	const FGameplayTag CooldownTag = SkillDefinition->GetCooldownTag();
	return CooldownTag.IsValid() && CachedAbilitySystemComponent->HasMatchingGameplayTag(CooldownTag);
}

////////////////////////////
//! \author HanUl
//! \brief 입력 태그와 연결된 슬롯이 쿨타임 중인지 확인한다.
//! \param InputTag 확인할 입력 GameplayTag
//! \return 쿨타임 중이면 true
bool UMySkillControlComponent::IsInputTagOnCooldown(FGameplayTag InputTag) const
{
	const FMySkillSlotSpec* SkillSlot = FindSkillSlotByInputTag(InputTag);
	return SkillSlot && IsSkillSlotOnCooldown(*SkillSlot);
}

////////////////////////////
//! \author HanUl
//! \brief GA ActivateAbility로 넘길 GameplayEventData를 입력 컨텍스트에서 생성한다.
//! \param InputTag 입력 GameplayTag
//! \param InputContext 입력 순간의 조준 컨텍스트
//! \return GameplayEventData
FGameplayEventData UMySkillControlComponent::BuildGameplayEventData(FGameplayTag InputTag, const FMySkillInputContext& InputContext) const
{
	FGameplayEventData EventData;
	EventData.EventTag = InputTag;
	EventData.EventMagnitude = InputContext.bHasAimYaw ? InputContext.AimYaw : 0.0f;
	EventData.Instigator = OwnerCharacter.Get();
	EventData.Target = OwnerCharacter.Get();

	if (InputContext.bHasMoveDirection)
	{
		const FVector MoveDirection = InputContext.MoveDirection.GetSafeNormal2D();
		if (!MoveDirection.IsNearlyZero())
		{
			// 준혁 수정 : FVector_NetQuantize(정수 반올림) 양자화로 방향이 뭉개지지 않도록 크게 스케일하고
			//            성분을 정수로 반올림해 넣는다(클라 로컬 값과 서버 복제 값이 정확히 일치 → 예측 정합 유지).
			//            어빌리티는 이 벡터를 GetSafeNormal2D()로 다시 정규화해 사용한다.
			const FVector EncodedDirection(
				FMath::RoundToFloat(MoveDirection.X * MoveDirectionEncodeScale),
				FMath::RoundToFloat(MoveDirection.Y * MoveDirectionEncodeScale),
				0.0f);

			FHitResult MoveDirectionHitResult;
			MoveDirectionHitResult.bBlockingHit = false;
			MoveDirectionHitResult.Location = EncodedDirection;
			MoveDirectionHitResult.ImpactPoint = EncodedDirection;
			MoveDirectionHitResult.TraceStart = FVector::ZeroVector;
			MoveDirectionHitResult.TraceEnd = EncodedDirection;
			EventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(MoveDirectionHitResult));
		}
	}

	if (InputContext.bHasAimWorldLocation)
	{
		FHitResult AimHitResult;
		AimHitResult.bBlockingHit = true;
		AimHitResult.Location = InputContext.AimWorldLocation;
		AimHitResult.ImpactPoint = InputContext.AimWorldLocation;
		AimHitResult.TraceStart = InputContext.AimWorldLocation;
		AimHitResult.TraceEnd = InputContext.AimWorldLocation;
		EventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(AimHitResult));
	}

	return EventData;
}

////////////////////////////
//! \author HanUl
//! \editor 준혁 - 대쉬(이동기)를 표준 예측 발동으로 전환하기 위해 서버 전용 활성화 컨텍스트 강제를 제거
//! \brief 슬롯 입력 시점에 사용할 스킬 입력 컨텍스트를 만든다.
//! \param SkillSlot 입력을 받은 스킬 슬롯
//! \return 입력 컨텍스트
FMySkillInputContext UMySkillControlComponent::BuildSkillInputContext(const FMySkillSlotSpec& SkillSlot) const
{
	FMySkillInputContext InputContext;
	const UMySkillDefinitionDataAsset* SkillDefinition = SkillSlot.GetSkillDefinition();
	if (!SkillDefinition)
	{
		return InputContext;
	}

	const FMySkillInputSpec& InputSpec = SkillDefinition->GetInput();

	// 준혁 수정 : 대쉬(이동기)는 입력 시점에 최종 대쉬 방향을 확정해 활성화 EventData(TargetData)로 서버에 동기 전달한다.
	//            이렇게 해야 서버가 활성화와 동시에 같은 방향으로 루트모션을 시작해 예측 정합이 유지되고,
	//            데디 서버 접속 시 대쉬가 드드드 떨리던 문제(지연 TargetData 복제로 서버 루트모션이 늦게 붙던 것)가 사라진다.
	//            방향 정책은 SkillDefinition의 DashDirectionPolicy로 에디터에서 선택한다:
	//              - RequireMoveInput               : 이동 입력이 없으면 방향을 비워 두어 발동이 실패하게 한다.
	//              - UseFacingWhenMoveInputMissing  : 이동 입력이 없으면 바라보는 방향으로 폴백한다.
	const FMySkillMovementSpec& MovementSpec = SkillDefinition->GetMovement();
	if (MovementSpec.DashStrength > 0.0f)
	{
		if (!ResolveMoveDirectionContext(InputContext)
			&& MovementSpec.DashDirectionPolicy == EMyDashDirectionPolicy::UseFacingWhenMoveInputMissing)
		{
			const ACharacter* DashCharacter = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
			if (DashCharacter)
			{
				const FVector FacingDirection = DashCharacter->GetActorForwardVector().GetSafeNormal2D();
				if (!FacingDirection.IsNearlyZero())
				{
					InputContext.bHasMoveDirection = true;
					InputContext.MoveDirection = FacingDirection;
				}
			}
		}
	}

	switch (InputSpec.AimSource)
	{
	case EMySkillAimSource::MouseCursor:
		InputContext.bShouldFaceAimDirection = ResolveMouseAimContext(InputContext);
		break;
	case EMySkillAimSource::ControllerForward:
		ResolveControllerForwardAimContext(InputContext);
		break;
	case EMySkillAimSource::CurrentFacing:
		ResolveCurrentFacingAimContext(InputContext);
		break;
	case EMySkillAimSource::None:
	default:
		break;
	}

	if (!InputContext.bHasAimYaw)
	{
		InputContext.bShouldFaceAimDirection = false;
	}

	return InputContext;
}

////////////////////////////
//! \author HanUl
//! \brief 대쉬 입력 순간의 최근 이동 방향을 입력 컨텍스트에 기록한다.
//! \param OutInputContext 계산된 입력 컨텍스트
//! \return 최근 이동 방향을 기록했으면 true
bool UMySkillControlComponent::ResolveMoveDirectionContext(FMySkillInputContext& OutInputContext) const
{
	const UPlayerMovementComponent* MovementComponent = PlayerMovementComponent.Get();
	if (!MovementComponent)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			MovementComponent = OwnerActor->FindComponentByClass<UPlayerMovementComponent>();
		}
	}

	FVector MoveDirection = FVector::ZeroVector;
	if (!MovementComponent || !MovementComponent->GetLastMoveInputDirection(MoveInputContextMaxAgeSeconds, MoveDirection))
	{
		return false;
	}

	MoveDirection = MoveDirection.GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		return false;
	}

	OutInputContext.bHasMoveDirection = true;
	OutInputContext.MoveDirection = MoveDirection;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 마우스 커서 기준 월드 위치와 Yaw를 입력 컨텍스트로 변환한다.
//! \param OutInputContext 계산된 입력 컨텍스트
//! \return 마우스 조준 방향을 계산했으면 true
bool UMySkillControlComponent::ResolveMouseAimContext(FMySkillInputContext& OutInputContext) const
{
	const ACharacter* Character = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (!PlayerController)
	{
		return false;
	}

	FVector AimPoint = FVector::ZeroVector;
	if (ResolveMouseAimPoint(PlayerController, AimPoint))
	{
		FVector AimDirection = AimPoint - Character->GetActorLocation();
		AimDirection.Z = 0.0f;
		if (!AimDirection.IsNearlyZero())
		{
			OutInputContext.bHasAimYaw = true;
			OutInputContext.AimYaw = FRotator::NormalizeAxis(AimDirection.Rotation().Yaw);
			OutInputContext.bHasAimWorldLocation = true;
			OutInputContext.AimWorldLocation = AimPoint;
			return true;
		}
	}

	const FRotator ControlYawRotation(0.0f, PlayerController->GetControlRotation().Yaw, 0.0f);
	OutInputContext.bHasAimYaw = true;
	OutInputContext.AimYaw = FRotator::NormalizeAxis(ControlYawRotation.Yaw);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 컨트롤러가 바라보는 방향을 입력 컨텍스트로 변환한다.
//! \param OutInputContext 계산된 입력 컨텍스트
//! \return 컨트롤러 방향을 계산했으면 true
bool UMySkillControlComponent::ResolveControllerForwardAimContext(FMySkillInputContext& OutInputContext) const
{
	const ACharacter* Character = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	const AController* Controller = Character->GetController();
	if (!Controller)
	{
		return false;
	}

	OutInputContext.bHasAimYaw = true;
	OutInputContext.AimYaw = FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 현재 캐릭터가 바라보는 방향을 입력 컨텍스트로 변환한다.
//! \param OutInputContext 계산된 입력 컨텍스트
//! \return 현재 캐릭터 방향을 계산했으면 true
bool UMySkillControlComponent::ResolveCurrentFacingAimContext(FMySkillInputContext& OutInputContext) const
{
	const ACharacter* Character = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	OutInputContext.bHasAimYaw = true;
	OutInputContext.AimYaw = FRotator::NormalizeAxis(Character->GetActorRotation().Yaw);
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 커서 HitResult 또는 마우스 디프로젝션으로 월드 조준 지점을 계산한다.
//! \param PlayerController 로컬 PlayerController
//! \param OutAimPoint 계산된 월드 조준 지점
//! \return 조준 지점을 계산했으면 true
bool UMySkillControlComponent::ResolveMouseAimPoint(APlayerController* PlayerController, FVector& OutAimPoint) const
{
	const ACharacter* Character = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
	if (!PlayerController || !Character)
	{
		return false;
	}

	FVector WorldLocation = FVector::ZeroVector;
	FVector WorldDirection = FVector::ForwardVector;
	if (!PlayerController->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return false;
	}

	if (UWorld* World = PlayerController->GetWorld())
	{
		const FVector TraceEnd = WorldLocation + WorldDirection * 100000.0f;
		FHitResult CursorHitResult;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SkillMouseAimTrace), false, Character);
		QueryParams.AddIgnoredActor(Character);
		QueryParams.AddIgnoredActor(GetOwner());

		if (World->LineTraceSingleByChannel(
			CursorHitResult,
			WorldLocation,
			TraceEnd,
			MouseAimTraceChannel.GetValue(),
			QueryParams
		))
		{
			OutAimPoint = CursorHitResult.ImpactPoint.IsNearlyZero()
				? CursorHitResult.Location
				: CursorHitResult.ImpactPoint;
			return true;
		}
	}

	if (FMath::IsNearlyZero(WorldDirection.Z))
	{
		return false;
	}

	const float PlaneZ = Character->GetActorLocation().Z;
	const float DistanceAlongRay = (PlaneZ - WorldLocation.Z) / WorldDirection.Z;
	if (DistanceAlongRay <= 0.0f)
	{
		return false;
	}

	OutAimPoint = WorldLocation + WorldDirection * DistanceAlongRay;
	return true;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 컨텍스트의 Yaw 방향 회전을 MovementComponent에 요청한다.
//! \param InputContext 입력 순간의 조준 컨텍스트
//! \return 없음
void UMySkillControlComponent::RequestOwnerFacingFromInputContext(const FMySkillInputContext& InputContext)
{
	if (!InputContext.bHasAimYaw)
	{
		return;
	}

	if (!PlayerMovementComponent)
	{
		CacheOwnerCharacter();
	}

	if (PlayerMovementComponent)
	{
		PlayerMovementComponent->RequestSkillFacingYaw(
			InputContext.AimYaw,
			MouseAimFacingInterpSpeed,
			MouseAimFacingToleranceDegrees
		);
		return;
	}
}

////////////////////////////
//! \author HanUl
//! \brief ASC에 부여된 Ability 중 입력 태그와 일치하는 AbilitySpec을 찾는다.
//! \param InputTag 검색할 입력 GameplayTag
//! \return 일치하는 AbilitySpec 포인터, 없으면 nullptr
FGameplayAbilitySpec* UMySkillControlComponent::FindAbilitySpecByInputTag(FGameplayTag InputTag) const
{
	if (!CachedAbilitySystemComponent || !InputTag.IsValid())
	{
		return nullptr;
	}

	TArray<FGameplayAbilitySpec>& ActivatableAbilities = CachedAbilitySystemComponent->GetActivatableAbilities();
	for (FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities)
	{
		if (AbilitySpec.Handle.IsValid() && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return &AbilitySpec;
		}
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 태그와 일치하는 스킬 슬롯을 찾는다.
//! \param InputTag 검색할 입력 GameplayTag
//! \return 일치하는 슬롯 포인터, 없으면 nullptr
const FMySkillSlotSpec* UMySkillControlComponent::FindSkillSlotByInputTag(FGameplayTag InputTag) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	const FMySkillSlotSpec* SkillSlots[] = { &BasicAttackSlot, &QSkillSlot, &ESkillSlot, &RSkillSlot, &CSkillSlot, &MoveSkillSlot };
	for (const FMySkillSlotSpec* SkillSlot : SkillSlots)
	{
		if (SkillSlot && SkillSlot->GetInputTag() == InputTag)
		{
			return SkillSlot;
		}
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief Owner Character 참조를 캐시한다.
//! \param 없음
//! \return Owner가 Character이면 true
bool UMySkillControlComponent::CacheOwnerCharacter()
{
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ACharacter>(GetOwner());
	}

	if (!PlayerMovementComponent)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			PlayerMovementComponent = OwnerActor->FindComponentByClass<UPlayerMovementComponent>();
		}
	}

	return OwnerCharacter != nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 시작 시 해당 슬롯에 Pressed 이벤트를 라우팅한다.
//! \param Value Enhanced Input 값
//! \param SkillSlot 입력을 전달할 스킬 슬롯
//! \return 없음
void UMySkillControlComponent::HandleSkillPressed(const FInputActionValue& Value, FMySkillSlotSpec* SkillSlot)
{
	(void)Value;
	if (!SkillSlot)
	{
		return;
	}

	// 임시 입력(추후 UI 대체): Ctrl + 스킬키로 강화 대상(Q·E·R·C)을 강화 요청하고, 스킬 발동은 건너뛴다.
	if (IsUpgradeableSlot(SkillSlot) && IsUpgradeModifierHeld())
	{
		RequestSkillUpgrade(SkillSlot->GetInputTag());
		return;
	}

	RouteSkillInputSlot(*SkillSlot, EMySkillInputRouteEvent::Pressed);
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 종료 시 해당 슬롯에 Released 이벤트를 라우팅한다.
//! \param Value Enhanced Input 값
//! \param SkillSlot 입력을 전달할 스킬 슬롯
//! \return 없음
void UMySkillControlComponent::HandleSkillReleased(const FInputActionValue& Value, FMySkillSlotSpec* SkillSlot)
{
	(void)Value;
	if (SkillSlot)
	{
		RouteSkillInputSlot(*SkillSlot, EMySkillInputRouteEvent::Released);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 입력 취소 시 해당 슬롯에 Canceled 이벤트를 라우팅한다.
//! \param Value Enhanced Input 값
//! \param SkillSlot 입력을 전달할 스킬 슬롯
//! \return 없음
void UMySkillControlComponent::HandleSkillCanceled(const FInputActionValue& Value, FMySkillSlotSpec* SkillSlot)
{
	(void)Value;
	if (SkillSlot)
	{
		RouteSkillInputSlot(*SkillSlot, EMySkillInputRouteEvent::Canceled);
	}
}

////////////////////////////
//! \author HanUl
//! \brief 소유 클라이언트의 스킬 입력 이벤트를 서버 ASC에 전달한다.
//! \param InputTag 입력을 전달할 스킬 입력 태그
//! \param InputEvent 전달할 입력 이벤트
//! \return 없음
void UMySkillControlComponent::ServerRouteSkillInputByInputTag_Implementation(FGameplayTag InputTag, EMySkillInputRouteEvent InputEvent, FMySkillInputContext InputContext)
{
	if (!FindSkillSlotByInputTag(InputTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl server input route rejected - slot not found. Owner: %s, InputTag: %s"),
			*GetNameSafe(GetOwner()),
			*InputTag.ToString());
		return;
	}

	RouteSkillInputByInputTag(InputTag, InputEvent, InputContext);
}

////////////////////////////
//! \author HanUl
//! \brief 서버가 판정한 스킬 디버그 도형을 소유 클라이언트 화면에 그린다.
//! \param Shape 표시할 디버그 도형
//! \return 없음
void UMySkillControlComponent::ClientDrawSkillDebugShape_Implementation(FMySkillDebugShape Shape)
{
	MySkillDebugDraw::DrawShape(GetWorld(), Shape);
}

////////////////////////////
//! \author HanUl
//! \brief 강화 대상 슬롯(Q·E·R·C)인지 확인한다. 기본공격/이동기는 강화 대상이 아니다.
//! \param SkillSlot 확인할 슬롯 포인터
//! \return Q·E·R·C 슬롯이면 true
bool UMySkillControlComponent::IsUpgradeableSlot(const FMySkillSlotSpec* SkillSlot) const
{
	return SkillSlot == &QSkillSlot
		|| SkillSlot == &ESkillSlot
		|| SkillSlot == &RSkillSlot
		|| SkillSlot == &CSkillSlot;
}

////////////////////////////
//! \author HanUl
//! \brief 강화 입력 수정자(Ctrl)가 눌려 있는지 확인한다. 추후 UI로 대체될 임시 입력이다.
//! \param 없음
//! \return 좌/우 Ctrl 중 하나라도 눌려 있으면 true
bool UMySkillControlComponent::IsUpgradeModifierHeld() const
{
	const ACharacter* Character = OwnerCharacter ? OwnerCharacter.Get() : Cast<ACharacter>(GetOwner());
	const APlayerController* PlayerController = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PlayerController)
	{
		return false;
	}

	return PlayerController->IsInputKeyDown(EKeys::LeftControl)
		|| PlayerController->IsInputKeyDown(EKeys::RightControl);
}

////////////////////////////
//! \author HanUl
//! \brief 소유 클라이언트에서 스킬 강화를 요청한다. 권한이면 즉시 실행, 아니면 서버 RPC로 전달한다.
//! \param InputTag 강화할 슬롯의 입력 태그
//! \return 없음
void UMySkillControlComponent::RequestSkillUpgrade(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		PerformSkillUpgrade(InputTag);
		return;
	}

	ServerRequestSkillUpgrade(InputTag);
}

////////////////////////////
//! \author HanUl
//! \brief 소유 클라이언트의 스킬 강화 요청을 서버에서 실행한다.
//! \param InputTag 강화할 슬롯의 입력 태그
//! \return 없음
void UMySkillControlComponent::ServerRequestSkillUpgrade_Implementation(FGameplayTag InputTag)
{
	PerformSkillUpgrade(InputTag);
}

////////////////////////////
//! \author HanUl
//! \brief 서버에서 스킬포인트/최대레벨을 검증하고 강화를 실행한다. 검증 통과 시 포인트 1 소비 → 레벨 +1 → Definition 교체.
//! \param InputTag 강화할 슬롯의 입력 태그
//! \return 없음
void UMySkillControlComponent::PerformSkillUpgrade(FGameplayTag InputTag)
{
	if (!CachedAbilitySystemComponent || !CachedAbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	FMySkillSlotSpec* SkillSlot = FindMutableSkillSlotByInputTag(InputTag);
	if (!SkillSlot || !IsUpgradeableSlot(SkillSlot))
	{
		return;
	}

	if (!UpgradeLadder)
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl upgrade skipped - UpgradeLadder not set. Owner: %s, InputTag: %s"),
			*GetNameSafe(GetOwner()),
			*InputTag.ToString());
		return;
	}

	const UMySkillDefinitionDataAsset* CurrentDefinition = SkillSlot->GetSkillDefinition();
	if (!CurrentDefinition)
	{
		return;
	}

	AMyPlayerState* PlayerState = GetOwnerPlayerState();
	if (!PlayerState)
	{
		return;
	}

	const FName SkillId = CurrentDefinition->GetSkillId();
	const int32 CurrentLevel = PlayerState->GetSkillLevel(SkillId);

	if (!UpgradeLadder->CanUpgrade(SkillId, CurrentLevel))
	{
		UE_LOG(LogTemp, Log, TEXT("SkillControl upgrade ignored - already max level. Owner: %s, SkillId: %s, Level: %d"),
			*GetNameSafe(GetOwner()),
			*SkillId.ToString(),
			CurrentLevel);
		return;
	}

	if (PlayerState->GetSkillPoints() <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("SkillControl upgrade ignored - no skill points. Owner: %s, SkillId: %s"),
			*GetNameSafe(GetOwner()),
			*SkillId.ToString());
		return;
	}

	const UMySkillDefinitionDataAsset* NextDefinition = UpgradeLadder->GetDefinitionForLevel(SkillId, CurrentLevel + 1);
	if (!NextDefinition)
	{
		return;
	}

	if (!PlayerState->ConsumeSkillPoints(1))
	{
		return;
	}

	PlayerState->SetSkillLevel(SkillId, CurrentLevel + 1);
	ApplySkillLevelDefinition(*SkillSlot, NextDefinition, TEXT("Upgrade"));

	UE_LOG(LogTemp, Log, TEXT("SkillControl upgraded - Owner: %s, SkillId: %s, Level: %d -> %d, Definition: %s"),
		*GetNameSafe(GetOwner()),
		*SkillId.ToString(),
		CurrentLevel,
		CurrentLevel + 1,
		*GetNameSafe(NextDefinition));
}

////////////////////////////
//! \author HanUl
//! \brief 슬롯의 부여된 Ability를 새 레벨 Definition으로 교체한다. 같은 GA면 SourceObject/Level만 교체(재부여 없이 복제), 다르면 회수 후 재부여한다.
//! \param SkillSlot 교체 대상 슬롯
//! \param NewDefinition 새 레벨 Definition
//! \param SlotDebugName 로그용 슬롯 이름
//! \return 없음
void UMySkillControlComponent::ApplySkillLevelDefinition(FMySkillSlotSpec& SkillSlot, const UMySkillDefinitionDataAsset* NewDefinition, const TCHAR* SlotDebugName)
{
	if (!CachedAbilitySystemComponent || !CachedAbilitySystemComponent->IsOwnerActorAuthoritative() || !NewDefinition)
	{
		return;
	}

	if (!NewDefinition->IsValidDefinition())
	{
		UE_LOG(LogTemp, Warning, TEXT("SkillControl upgrade skipped - invalid new Definition. Owner: %s, Slot: %s, Definition: %s"),
			*GetNameSafe(GetOwner()),
			SlotDebugName,
			*GetNameSafe(NewDefinition));
		return;
	}

	FGameplayAbilitySpec* AbilitySpec = SkillSlot.GrantedAbilityHandle.IsValid()
		? CachedAbilitySystemComponent->FindAbilitySpecFromHandle(SkillSlot.GrantedAbilityHandle)
		: nullptr;

	const bool bSameAbilityClass = AbilitySpec
		&& AbilitySpec->Ability
		&& AbilitySpec->Ability->GetClass() == NewDefinition->GetAbilityClass();

	if (bSameAbilityClass)
	{
		// 같은 GA: 부여된 스펙의 SourceObject/Level만 교체하고 MarkAbilitySpecDirty로 클라에 복제한다(재부여 없이 강화).
		AbilitySpec->SourceObject = const_cast<UMySkillDefinitionDataAsset*>(NewDefinition);
		AbilitySpec->Level = NewDefinition->GetAbilityLevel();
		SkillSlot.SkillDefinition = const_cast<UMySkillDefinitionDataAsset*>(NewDefinition);
		CachedAbilitySystemComponent->MarkAbilitySpecDirty(*AbilitySpec);
	}
	else
	{
		// 다른 GA(또는 스펙 없음): 회수 후 새 Definition으로 재부여한다. GrantSkillSlot이 PlayerState 레벨로 재해석한다.
		ClearGrantedSkillSlot(SkillSlot);
		SkillSlot.SkillDefinition = const_cast<UMySkillDefinitionDataAsset*>(NewDefinition);
		GrantSkillSlot(SkillSlot, SlotDebugName);
	}

	UE_LOG(LogTemp, Log, TEXT("SkillControl definition swapped - Owner: %s, Slot: %s, SameGA: %s, Definition: %s"),
		*GetNameSafe(GetOwner()),
		SlotDebugName,
		bSameAbilityClass ? TEXT("true") : TEXT("false"),
		*GetNameSafe(NewDefinition));
}

////////////////////////////
//! \author HanUl
//! \brief PlayerState에 저장된 현재 레벨에 해당하는 Definition을 사다리에서 해석한다.
//! \param SkillSlot 대상 슬롯
//! \param BaseDefinition 슬롯의 기준 Definition
//! \return 현재 레벨 Definition, 강화 대상이 아니거나 사다리에 없으면 BaseDefinition
const UMySkillDefinitionDataAsset* UMySkillControlComponent::ResolveLeveledDefinition(const FMySkillSlotSpec& SkillSlot, const UMySkillDefinitionDataAsset* BaseDefinition) const
{
	if (!UpgradeLadder || !BaseDefinition || !IsUpgradeableSlot(&SkillSlot))
	{
		return BaseDefinition;
	}

	const AMyPlayerState* PlayerState = GetOwnerPlayerState();
	if (!PlayerState)
	{
		return BaseDefinition;
	}

	const FName SkillId = BaseDefinition->GetSkillId();
	const int32 Level = PlayerState->GetSkillLevel(SkillId);
	const UMySkillDefinitionDataAsset* LeveledDefinition = UpgradeLadder->GetDefinitionForLevel(SkillId, Level);
	return LeveledDefinition ? LeveledDefinition : BaseDefinition;
}

////////////////////////////
//! \author HanUl
//! \brief 강화 대상 슬롯(Q·E·R·C)의 로컬 Definition을 현재 진행도(레벨)에 맞춰 갱신한다. 클라의 입력 컨텍스트/쿨다운/UI 일관성용이다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::RefreshUpgradeableSlotDefinitionsFromProgress()
{
	if (!UpgradeLadder)
	{
		return;
	}

	const AMyPlayerState* PlayerState = GetOwnerPlayerState();
	if (!PlayerState)
	{
		return;
	}

	FMySkillSlotSpec* UpgradeableSlots[] = { &QSkillSlot, &ESkillSlot, &RSkillSlot, &CSkillSlot };
	for (FMySkillSlotSpec* SkillSlot : UpgradeableSlots)
	{
		if (!SkillSlot || !SkillSlot->SkillDefinition)
		{
			continue;
		}

		const FName SkillId = SkillSlot->SkillDefinition->GetSkillId();
		const int32 Level = PlayerState->GetSkillLevel(SkillId);
		const UMySkillDefinitionDataAsset* LeveledDefinition = UpgradeLadder->GetDefinitionForLevel(SkillId, Level);
		if (LeveledDefinition && LeveledDefinition != SkillSlot->SkillDefinition)
		{
			SkillSlot->SkillDefinition = const_cast<UMySkillDefinitionDataAsset*>(LeveledDefinition);
		}
	}
}

////////////////////////////
//! \author HanUl
//! \brief 소유 Pawn의 PlayerState(AMyPlayerState)를 반환한다.
//! \param 없음
//! \return PlayerState, 없으면 nullptr
AMyPlayerState* UMySkillControlComponent::GetOwnerPlayerState() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		return OwnerPawn->GetPlayerState<AMyPlayerState>();
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 입력 태그와 일치하는 수정 가능한 스킬 슬롯 포인터를 찾는다.
//! \param InputTag 검색할 입력 태그
//! \return 일치하는 슬롯 포인터, 없으면 nullptr
FMySkillSlotSpec* UMySkillControlComponent::FindMutableSkillSlotByInputTag(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	FMySkillSlotSpec* SkillSlots[] = { &BasicAttackSlot, &QSkillSlot, &ESkillSlot, &RSkillSlot, &CSkillSlot, &MoveSkillSlot };
	for (FMySkillSlotSpec* SkillSlot : SkillSlots)
	{
		if (SkillSlot && SkillSlot->GetInputTag() == InputTag)
		{
			return SkillSlot;
		}
	}

	return nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 소유 PlayerState의 스킬 진행도 변경 델리게이트에 바인딩하고 현재 상태를 즉시 반영한다. 중복 바인딩을 방지한다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::BindToSkillProgress()
{
	AMyPlayerState* PlayerState = GetOwnerPlayerState();
	if (!PlayerState)
	{
		return;
	}

	if (BoundSkillProgressPlayerState.Get() == PlayerState && SkillProgressChangedHandle.IsValid())
	{
		return;
	}

	UnbindFromSkillProgress();
	SkillProgressChangedHandle = PlayerState->OnSkillProgressChanged.AddUObject(this, &UMySkillControlComponent::HandleSkillProgressChanged);
	BoundSkillProgressPlayerState = PlayerState;

	// 접속/리스폰 직후 이미 강화된 상태를 로컬 슬롯 Definition에 즉시 반영한다.
	RefreshUpgradeableSlotDefinitionsFromProgress();
}

////////////////////////////
//! \author HanUl
//! \brief 스킬 진행도 변경 델리게이트 바인딩을 해제한다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::UnbindFromSkillProgress()
{
	if (AMyPlayerState* PlayerState = BoundSkillProgressPlayerState.Get())
	{
		if (SkillProgressChangedHandle.IsValid())
		{
			PlayerState->OnSkillProgressChanged.Remove(SkillProgressChangedHandle);
		}
	}

	SkillProgressChangedHandle.Reset();
	BoundSkillProgressPlayerState = nullptr;
}

////////////////////////////
//! \author HanUl
//! \brief 스킬포인트/강화 레벨 변경 시(서버 갱신 또는 클라 복제 수신) 로컬 슬롯 Definition을 갱신한다.
//! \param 없음
//! \return 없음
void UMySkillControlComponent::HandleSkillProgressChanged()
{
	RefreshUpgradeableSlotDefinitionsFromProgress();
}

////////////////////////////
//! \author 장효제
//! \brief 스킬 입력을 조작 사실로 보고한다.
//! \details 눌림과 서버 RPC는 기본 조작 컴포넌트가 이미 갖고 있다. 같은 Pawn의
//!          형제 컴포넌트를 찾아 그 경로를 재사용한다. 스킬이 쿨다운이라 실제로
//!          발동하지 않아도 누른 것은 조작이다.
//! \return 없음
void UMySkillControlComponent::ReportSkillInputAsPlayerInput()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (UMyBasicControlComponent* BasicControl =
			Owner->FindComponentByClass<UMyBasicControlComponent>())
	{
		BasicControl->ReportPlayerInput();
	}
}
