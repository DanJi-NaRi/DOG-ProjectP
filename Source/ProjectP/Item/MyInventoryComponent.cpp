////////////////////////////
//! \page MyInventoryComponent.cpp
//! \brief 메소와 아이템 보유량을 서버 권위로 관리하는 인벤토리 컴포넌트 구현 파일이다.
#include "Item/MyInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Dungeon/DungeonReconnectTypes.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GAS/MyAttributeSet.h"
#include "GAS/MyPlayerState.h"
#include "Streaming/MyStreamingPayloads.h"
#include "MyGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Shop/MyShopActor.h"
#include "Streaming/MyStreamingPayloads.h"
#include "UObject/ConstructorHelpers.h"

////////////////////////////
//! \author 준혁
//! \brief 인벤토리 컴포넌트를 복제 가능하게 설정한다.
UMyInventoryComponent::UMyInventoryComponent()
{
    static ConstructorHelpers::FObjectFinder<UDataTable> DefaultItemDataTable(
        TEXT("/Game/LeDuat/Dungeon/Item/FMyItemData.FMyItemData"));
    if (DefaultItemDataTable.Succeeded())
    {
        ItemDataTable = DefaultItemDataTable.Object;
    }

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

////////////////////////////
//! \author 준혁
//! \brief 퀵슬롯 배열을 설정된 개수만큼 초기화한다. 데이터테이블 미지정 시 경고를 남긴다.
void UMyInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	QuickSlotItemIds.Init(NAME_None, FMath::Max(1, NumQuickSlots));

	// 데이터테이블은 BP_MyPlayerState의 InventoryComponent 디폴트에서 지정한다
	if (!ItemDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] ItemDataTable is not set. Assign it on the InventoryComponent of the PlayerState BP."));
	}
}

////////////////////////////
//! \author 준혁
//! \brief 복제할 프로퍼티(보유 아이템 목록, 메소)를 등록한다.
//! \note 재접속 복구를 위해 퀵슬롯도 소유 클라이언트 전용 복제 프로퍼티로 등록한다.
//! \param OutLifetimeProps 언리얼 네트워크 복제 시스템에 등록될 프로퍼티 목록
void UMyInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UMyInventoryComponent, Entries);
    DOREPLIFETIME(UMyInventoryComponent, Meso);
    DOREPLIFETIME_CONDITION(UMyInventoryComponent, QuickSlotItemIds, COND_OwnerOnly);
}

////////////////////////////
//! \author 준혁
//! \editor 장효제 - SourceTag를 함께 받아 서버 확정 Meso Fact를 발행하도록 확장
//! \brief [서버 전용] 출처가 명시된 Meso를 증가시키고 실제 변화 사실을 발행한다.
//! \param Amount 증가시킬 양 (0 이하는 무시)
//! \param SourceTag Meso 변화가 발생한 출처 태그다.
void UMyInventoryComponent::AddMeso(
	const int32 Amount,
	const FGameplayTag SourceTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0)
	{
		return;
	}

	Meso += Amount;
	OnRep_Meso();
	BroadcastMesoFact(Amount, Meso, SourceTag);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 서버에서 보유 메소를 지정한 값으로 설정하는 함수
// NewMeso : 새로 설정할 메소 값, 0 미만이면 0으로 보정
void UMyInventoryComponent::SetMeso(int32 NewMeso)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    const int32 ClampedMeso = FMath::Max(NewMeso, 0);
    if (Meso == ClampedMeso)
    {
        return;
    }

    Meso = ClampedMeso;
    OnRep_Meso();
}

////////////////////////////
//! \author 준혁
//! \editor 장효제 - SourceTag를 함께 받아 서버 확정 Meso Fact를 발행하도록 확장
//! \brief [서버 전용] 출처가 명시된 Meso가 충분하면 차감하고 실제 변화 사실을 발행한다.
//! \param Amount 차감할 양
//! \param SourceTag Meso 변화가 발생한 출처 태그다.
//! \return 차감 성공 여부
bool UMyInventoryComponent::TryConsumeMeso(
	const int32 Amount,
	const FGameplayTag SourceTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount < 0 || Meso < Amount)
	{
		return false;
	}

	Meso -= Amount;
	OnRep_Meso();
	BroadcastMesoFact(-Amount, Meso, SourceTag);
	return true;
}

////////////////////////////
//! \author 준혁
//! \editor 준혁 - 새 종류 지급 시 최대 슬롯(MaxSlots) 초과하면 실패하도록 칸 상한 검사 추가
//! \brief [서버 전용] 아이템을 지급한다. 데이터테이블에 없는 아이템이거나 인벤토리가 가득 차면 실패한다.
//! \param ItemId 지급할 아이템 ID (데이터테이블 Row Name)
//! \param Count 지급 개수
//! \return 지급 성공 여부
bool UMyInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	FMyItemData ItemData;
	if (!FindItemData(ItemId, ItemData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] AddItem failed - unknown ItemId '%s'."), *ItemId.ToString());
		return false;
	}

	if (FMyInventoryEntry* Entry = FindEntry(ItemId))
	{
		// 이미 보유 중인 종류는 기존 칸에 스택되므로 슬롯 수를 소모하지 않는다.
		Entry->Count = FMath::Min(Entry->Count + Count, ItemData.MaxStackCount);
	}
	else
	{
		// 새 종류는 새 칸이 필요하다. 빈 칸이 없으면 지급 실패.
		if (IsInventoryFull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Inventory] AddItem failed - inventory full (%d/%d slots). ItemId '%s'."), GetUsedSlots(), GetMaxSlots(), *ItemId.ToString());
			return false;
		}

		FMyInventoryEntry NewEntry;
		NewEntry.ItemId = ItemId;
		NewEntry.Count = FMath::Min(Count, ItemData.MaxStackCount);
		Entries.Add(NewEntry);
	}

	OnRep_Entries();
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief [서버 전용] 아이템을 차감한다. 보유량이 부족하면 실패한다. 0개가 되면 목록에서 제거한다.
//! \param ItemId 차감할 아이템 ID
//! \param Count 차감 개수
//! \return 차감 성공 여부
bool UMyInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	FMyInventoryEntry* Entry = FindEntry(ItemId);
	if (!Entry || Entry->Count < Count)
	{
		return false;
	}

	Entry->Count -= Count;
	if (Entry->Count <= 0)
	{
		Entries.RemoveAll([ItemId](const FMyInventoryEntry& InEntry)
		{
			return InEntry.ItemId == ItemId;
		});
	}

	OnRep_Entries();
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 사용 진입점. 서버면 즉시 처리하고, 클라면 Server RPC로 요청한다.
//!        쿨타임 태그는 클라에도 복제되므로 쿨타임 중에는 RPC를 보내지 않는다. (최종 검증은 서버)
//! \param ItemId 사용할 아이템 ID
void UMyInventoryComponent::UseItem(FName ItemId)
{
	const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(GetOwner());
	if (MyPlayerState && MyPlayerState->IsDead())
	{
		return;
	}

	if (GetItemCooldownRemaining(ItemId) > 0.0f)
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Server_UseItem_Implementation(ItemId);
		return;
	}

	Server_UseItem(ItemId);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 아이템 사용을 검증(보유량/쿨타임)하고 GameplayEffect 적용, 쿨타임 시작 후 1개를 차감한다.
//! \editor 준혁 (스탯강화 아이템의 StatModifiers 적용 추가, 보호막 아이템의 ShieldRatio 전달 추가)
//! \param ItemId 사용할 아이템 ID
void UMyInventoryComponent::Server_UseItem_Implementation(FName ItemId)
{
	const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(GetOwner());
	if (MyPlayerState && MyPlayerState->IsDead())
	{
		return;
	}

	FMyItemData ItemData;
	if (!FindItemData(ItemId, ItemData) || !ItemData.bUsable)
	{
		return;
	}

	if (GetItemCount(ItemId) <= 0)
	{
		return;
	}

	// 효과 적용/쿨타임 대상: 소유자(PlayerState)의 ASC
	const IAbilitySystemInterface* ASCOwner = Cast<IAbilitySystemInterface>(GetOwner());
	UAbilitySystemComponent* ASC = ASCOwner ? ASCOwner->GetAbilitySystemComponent() : nullptr;

	// 쿨타임 검증: 같은 CooldownTag를 쓰는 아이템(포션 3종)끼리 공유된다
	if (ItemData.CooldownTag.IsValid() && ASC && ASC->HasMatchingGameplayTag(ItemData.CooldownTag))
	{
		return;
	}

	if (ItemData.UseEffectClass)
	{
		if (!ASC)
		{
			return;
		}

		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ItemData.UseEffectClass, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			// 비율 회복 아이템(포션): 최대 체력 * HealRatio를 Data.Heal SetByCaller로 전달한다.
			// GE의 IncomingHeal 모디파이어가 이 값을 받아 AttributeSet에서 클램프 회복 처리된다.
			if (ItemData.HealRatio > 0.0f)
			{
				const float MaxHealth = ASC->GetNumericAttribute(UMyAttributeSet::GetMaxHealthAttribute());
				SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Heal, MaxHealth * ItemData.HealRatio);
			}

			// 비율 보호막 아이템(보호막 물약): 최대 체력 * ShieldRatio를 Data.Shield SetByCaller로 전달한다.
			// AttributeSet의 IncomingShield 처리에서 기존 보호막보다 큰 값일 때만 교체된다 (ReplaceIfGreater).
			if (ItemData.ShieldRatio > 0.0f)
			{
				const float MaxHealth = ASC->GetNumericAttribute(UMyAttributeSet::GetMaxHealthAttribute());
				SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Shield, MaxHealth * ItemData.ShieldRatio);
			}

			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}

	// 스탯강화 아이템: 데이터테이블의 StatModifiers를 공용 스탯 GE에 주입해 적용한다 (버프/영구 분기)
	if (ItemData.StatModifiers.Num() > 0)
	{
		if (!ASC)
		{
			return;
		}

		ApplyStatModifiers(ASC, ItemData);
	}

	// 쿨타임 시작: 공용 쿨타임 GE에 지속시간(SetByCaller)과 아이템별 쿨타임 태그(DynamicGrantedTags)를 주입한다.
	// 태그가 복제되면서 클라 HUD의 쿨다운 표시도 이 GE 하나로 구동된다.
	if (ASC && ItemData.CooldownTag.IsValid() && ItemData.CooldownSeconds > 0.0f)
	{
		if (ItemCooldownEffectClass)
		{
			FGameplayEffectContextHandle CooldownContext = ASC->MakeEffectContext();
			CooldownContext.AddSourceObject(this);

			const FGameplayEffectSpecHandle CooldownSpecHandle = ASC->MakeOutgoingSpec(ItemCooldownEffectClass, 1.0f, CooldownContext);
			if (CooldownSpecHandle.IsValid())
			{
				CooldownSpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Cooldown, ItemData.CooldownSeconds);
				CooldownSpecHandle.Data->DynamicGrantedTags.AddTag(ItemData.CooldownTag);
				ASC->ApplyGameplayEffectSpecToSelf(*CooldownSpecHandle.Data.Get());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Inventory] ItemCooldownEffectClass is not set - item '%s' cooldown skipped. Assign it on the InventoryComponent of the PlayerState BP."), *ItemId.ToString());
		}
	}

	RemoveItem(ItemId, 1);

	// 스트리밍 조건이 셀 수 있도록 사용 사실을 남긴다. 소비가 끝난 뒤에만 발행한다.
	MyStreamingCountEvent::BroadcastItemEvent(
		this,
		MyGameplayTags::Streaming_Event_Item_Used,
		ItemId,
		MyPlayerState ? MyPlayerState->GetUserIndex() : INDEX_NONE,
		1);
}

////////////////////////////
//! \author 준혁
//! \brief 스탯 종류를 대응하는 어트리뷰트와 SetByCaller 태그로 변환한다.
//! \param StatType 스탯 종류
//! \param OutAttribute 대응하는 UMyAttributeSet 어트리뷰트
//! \param OutTag 공용 스탯 GE의 모디파이어에 연결된 SetByCaller 태그 (Data.Stat.*)
//! \return 대응이 정의되어 있으면 true
static bool GetStatBinding(EMyItemStatType StatType, FGameplayAttribute& OutAttribute, FGameplayTag& OutTag)
{
	switch (StatType)
	{
	case EMyItemStatType::AttackPower:
		OutAttribute = UMyAttributeSet::GetAttackPowerAttribute();
		OutTag = MyGameplayTags::Data_Stat_AttackPower;
		return true;
	case EMyItemStatType::Defense:
		OutAttribute = UMyAttributeSet::GetDefenseAttribute();
		OutTag = MyGameplayTags::Data_Stat_Defense;
		return true;
	case EMyItemStatType::MaxHealth:
		OutAttribute = UMyAttributeSet::GetMaxHealthAttribute();
		OutTag = MyGameplayTags::Data_Stat_MaxHealth;
		return true;
	case EMyItemStatType::MoveSpeed:
		OutAttribute = UMyAttributeSet::GetMoveSpeedAttribute();
		OutTag = MyGameplayTags::Data_Stat_MoveSpeed;
		return true;
	case EMyItemStatType::AttackSpeed:
		OutAttribute = UMyAttributeSet::GetAttackSpeedAttribute();
		OutTag = MyGameplayTags::Data_Stat_AttackSpeed;
		return true;
	case EMyItemStatType::CritChance:
		OutAttribute = UMyAttributeSet::GetCritChanceAttribute();
		OutTag = MyGameplayTags::Data_Stat_CritChance;
		return true;
	case EMyItemStatType::CooldownReduction:
		OutAttribute = UMyAttributeSet::GetCooldownReductionAttribute();
		OutTag = MyGameplayTags::Data_Stat_CooldownReduction;
		return true;
	default:
		return false;
	}
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 스탯강화 아이템의 StatModifiers를 공용 스탯 GE에 SetByCaller로 주입해 적용한다.
//!        StatDurationSeconds > 0이면 지속 버프 GE(HasDuration), 0이면 영구 GE(Infinite)를 사용한다.
//!        BuffGroupTag가 있으면 같은 계열 태그의 기존 버프를 제거하고 새로 적용한다 (계열 덮어쓰기, 중첩 방지).
//! \param ASC 효과를 적용할 소유자(PlayerState)의 ASC
//! \param ItemData 사용한 아이템의 데이터테이블 Row
void UMyInventoryComponent::ApplyStatModifiers(UAbilitySystemComponent* ASC, const FMyItemData& ItemData) const
{
	const bool bPermanent = ItemData.StatDurationSeconds <= 0.0f;
	const TSubclassOf<UGameplayEffect> EffectClass = bPermanent ? ItemStatPermanentEffectClass : ItemStatBuffEffectClass;
	if (!EffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] %s is not set - stat item effect skipped. Assign it on the InventoryComponent of the PlayerState BP."),
			bPermanent ? TEXT("ItemStatPermanentEffectClass") : TEXT("ItemStatBuffEffectClass"));
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// GE에 모디파이어로 들어 있는 SetByCaller는 값이 지정되지 않으면 적용 시 에러가 나므로,
	// 아이템이 쓰지 않는 스탯까지 전부 0(무변화)으로 선초기화한다.
	static const EMyItemStatType AllStatTypes[] =
	{
		EMyItemStatType::AttackPower, EMyItemStatType::Defense, EMyItemStatType::MaxHealth,
		EMyItemStatType::MoveSpeed, EMyItemStatType::AttackSpeed, EMyItemStatType::CritChance,
		EMyItemStatType::CooldownReduction
	};
	for (const EMyItemStatType StatType : AllStatTypes)
	{
		FGameplayAttribute Attribute;
		FGameplayTag Tag;
		if (GetStatBinding(StatType, Attribute, Tag))
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Tag, 0.0f);
		}
	}

	for (const FMyItemStatModifier& Mod : ItemData.StatModifiers)
	{
		FGameplayAttribute Attribute;
		FGameplayTag Tag;
		if (!GetStatBinding(Mod.StatType, Attribute, Tag))
		{
			continue;
		}

		float Magnitude = Mod.Value;
		if (Mod.Op == EMyItemStatModOp::PercentAdd)
		{
			// 적용 시점의 베이스 값 기준 %를 고정치로 환산한다 (버프 중첩 시 복리 증가 방지)
			Magnitude = ASC->GetNumericAttributeBase(Attribute) * (Mod.Value / 100.0f);
		}

		// 같은 스탯이 여러 줄이면 합산
		const float Existing = SpecHandle.Data->GetSetByCallerMagnitude(Tag, false, 0.0f);
		SpecHandle.Data->SetSetByCallerMagnitude(Tag, Existing + Magnitude);
	}

	if (!bPermanent)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Duration, ItemData.StatDurationSeconds);
	}

	// 계열 덮어쓰기: 같은 계열 태그(BuffGroupTag)가 부여된 기존 스탯 버프를 제거한 뒤 새로 적용한다.
	// 새 스펙에도 같은 태그를 DynamicGrantedTags로 부여해 다음 사용 때 제거 대상으로 잡히게 한다.
	if (ItemData.BuffGroupTag.IsValid())
	{
		ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ItemData.BuffGroupTag)));
		SpecHandle.Data->DynamicGrantedTags.AddTag(ItemData.BuffGroupTag);
	}

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 활성 아이템 스탯 GameplayEffect의 적용 수치와 남은 시간을 재접속 스냅샷으로 생성하는 함수
// AbilitySystemComponent : 아이템 스탯 효과를 조회할 AbilitySystemComponent
// 반환값 : 재접속 복구용 아이템 스탯 효과 스냅샷 배열
TArray<FDungeonItemStatEffectSnapshot> UMyInventoryComponent::MakeReconnectItemStatEffectSnapshots(
    const UAbilitySystemComponent* AbilitySystemComponent) const
{
    TArray<FDungeonItemStatEffectSnapshot> EffectSnapshots;
    const UWorld* World = AbilitySystemComponent ? AbilitySystemComponent->GetWorld() : nullptr;
    if (!AbilitySystemComponent || !World)
    {
        return EffectSnapshots;
    }

    const FGameplayTag StatMagnitudeTags[] =
    {
        MyGameplayTags::Data_Stat_AttackPower,
        MyGameplayTags::Data_Stat_Defense,
        MyGameplayTags::Data_Stat_MaxHealth,
        MyGameplayTags::Data_Stat_MoveSpeed,
        MyGameplayTags::Data_Stat_AttackSpeed,
        MyGameplayTags::Data_Stat_CritChance,
        MyGameplayTags::Data_Stat_CooldownReduction
    };

    const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles =
        AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery());
    for (const FActiveGameplayEffectHandle& ActiveEffectHandle : ActiveEffectHandles)
    {
        const FActiveGameplayEffect* ActiveEffect =
            AbilitySystemComponent->GetActiveGameplayEffect(ActiveEffectHandle);
        if (!ActiveEffect || !ActiveEffect->Spec.Def)
        {
            continue;
        }

        const bool bPermanent =
            ItemStatPermanentEffectClass
            && ActiveEffect->Spec.Def->IsA(ItemStatPermanentEffectClass);
        const bool bTimed =
            ItemStatBuffEffectClass
            && ActiveEffect->Spec.Def->IsA(ItemStatBuffEffectClass);
        if (!bPermanent && !bTimed)
        {
            continue;
        }

        const float RemainingSeconds = bPermanent
            ? 0.0f
            : ActiveEffect->GetTimeRemaining(World->GetTimeSeconds());
        if (!bPermanent && RemainingSeconds <= 0.0f)
        {
            continue;
        }

        FDungeonItemStatEffectSnapshot EffectSnapshot;
        EffectSnapshot.bPermanent = bPermanent;
        EffectSnapshot.RemainingSeconds = RemainingSeconds;
        EffectSnapshot.DynamicGrantedTags = ActiveEffect->Spec.DynamicGrantedTags;
        for (const FGameplayTag& StatMagnitudeTag : StatMagnitudeTags)
        {
            EffectSnapshot.StatMagnitudes.Add(
                StatMagnitudeTag,
                ActiveEffect->Spec.GetSetByCallerMagnitude(StatMagnitudeTag, false, 0.0f));
        }
        EffectSnapshots.Add(MoveTemp(EffectSnapshot));
    }

    return EffectSnapshots;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 저장된 아이템 스탯 효과에서 접속 종료 후 경과 시간을 차감해 새 ASC에 적용하는 함수
// AbilitySystemComponent : 아이템 스탯 효과를 적용할 새 PlayerState의 AbilitySystemComponent
// EffectSnapshots : 저장된 아이템 스탯 효과 목록
// ElapsedSeconds : 접속 종료 후 재접속까지 서버 기준으로 흐른 시간
// 반환값 : 남아 있는 모든 아이템 스탯 효과를 적용했거나 적용할 효과가 없으면 true
bool UMyInventoryComponent::RestoreReconnectItemStatEffects(
    UAbilitySystemComponent* AbilitySystemComponent,
    const TArray<FDungeonItemStatEffectSnapshot>& EffectSnapshots,
    float ElapsedSeconds) const
{
    if (EffectSnapshots.IsEmpty())
    {
        return true;
    }

    const AActor* OwnerActor = GetOwner();
    if (!AbilitySystemComponent || !OwnerActor || !OwnerActor->HasAuthority())
    {
        return false;
    }

    const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles =
        AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery());
    for (const FActiveGameplayEffectHandle& ActiveEffectHandle : ActiveEffectHandles)
    {
        const FActiveGameplayEffect* ActiveEffect =
            AbilitySystemComponent->GetActiveGameplayEffect(ActiveEffectHandle);
        if (!ActiveEffect || !ActiveEffect->Spec.Def)
        {
            continue;
        }

        const bool bItemStatEffect =
            (ItemStatBuffEffectClass && ActiveEffect->Spec.Def->IsA(ItemStatBuffEffectClass))
            || (ItemStatPermanentEffectClass && ActiveEffect->Spec.Def->IsA(ItemStatPermanentEffectClass));
        if (bItemStatEffect)
        {
            AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveEffectHandle);
        }
    }

    bool bAppliedAllEffects = true;
    for (const FDungeonItemStatEffectSnapshot& EffectSnapshot : EffectSnapshots)
    {
        const float RemainingSeconds = EffectSnapshot.bPermanent
            ? 0.0f
            : EffectSnapshot.RemainingSeconds - FMath::Max(0.0f, ElapsedSeconds);
        if (!EffectSnapshot.bPermanent && RemainingSeconds <= 0.0f)
        {
            continue;
        }

        const TSubclassOf<UGameplayEffect> EffectClass = EffectSnapshot.bPermanent
            ? ItemStatPermanentEffectClass
            : ItemStatBuffEffectClass;
        if (!EffectClass)
        {
            bAppliedAllEffects = false;
            continue;
        }

        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        const FGameplayEffectSpecHandle SpecHandle =
            AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
        if (!SpecHandle.IsValid())
        {
            bAppliedAllEffects = false;
            continue;
        }

        for (const TPair<FGameplayTag, float>& StatMagnitude : EffectSnapshot.StatMagnitudes)
        {
            if (StatMagnitude.Key.IsValid())
            {
                SpecHandle.Data->SetSetByCallerMagnitude(StatMagnitude.Key, StatMagnitude.Value);
            }
        }

        if (!EffectSnapshot.bPermanent)
        {
            SpecHandle.Data->SetSetByCallerMagnitude(MyGameplayTags::Data_Duration, RemainingSeconds);
        }
        SpecHandle.Data->DynamicGrantedTags.AppendTags(EffectSnapshot.DynamicGrantedTags);

        if (!AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).IsValid())
        {
            bAppliedAllEffects = false;
        }
    }

    return bAppliedAllEffects;
}

////////////////////////////
//! \author 준혁
//! \brief 상점 구매 진입점. 서버면 즉시 처리하고, 클라면 Server RPC로 요청한다.
//! \param ShopActor 구매를 요청하는 상점 액터
//! \param ItemId 구매할 아이템 ID
//! \param Count 구매 개수
void UMyInventoryComponent::RequestPurchase(AMyShopActor* ShopActor, FName ItemId, int32 Count)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Server_PurchaseItem_Implementation(ShopActor, ItemId, Count);
		return;
	}

	Server_PurchaseItem(ShopActor, ItemId, Count);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 구매 요청을 검증하고 메소 차감 + 아이템 지급을 처리한 뒤, 결과를 요청 클라이언트에 통지한다.
//! \param ShopActor 구매를 요청한 상점 액터
//! \param ItemId 구매할 아이템 ID
//! \param Count 구매 개수
void UMyInventoryComponent::Server_PurchaseItem_Implementation(AMyShopActor* ShopActor, FName ItemId, int32 Count)
{
	const EMyShopPurchaseResult ValidationResult = ValidatePurchase(ShopActor, ItemId, Count);
	if (ValidationResult != EMyShopPurchaseResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Shop] Purchase rejected (%d) - Buyer: %s, ItemId: %s, Count: %d"),
			static_cast<int32>(ValidationResult), *GetNameSafe(GetOwner()), *ItemId.ToString(), Count);
		Client_PurchaseResult(ValidationResult, ItemId, Count);
		return;
	}

	FMyItemData ItemData;
	FindItemData(ItemId, ItemData); // 검증을 통과했으므로 항상 성공한다

	const int32 TotalPrice = ItemData.BuyPrice * Count;
	if (!TryConsumeMeso(TotalPrice, MyGameplayTags::Meso_Source_ShopPurchase))
	{
		Client_PurchaseResult(EMyShopPurchaseResult::NotEnoughMeso, ItemId, Count);
		return;
	}

	if (!AddItem(ItemId, Count))
	{
		// 검증 통과 후 지급 실패는 정상 흐름에선 없지만, 메소만 사라지는 일이 없도록 환불한다.
		// 구매가 성립하지 않은 트랜잭션 복원이므로 획득 Fact는 발행하지 않는다.
		SetMeso(GetMeso() + TotalPrice);
		Client_PurchaseResult(EMyShopPurchaseResult::InventoryFull, ItemId, Count);
		return;
	}

	Client_PurchaseResult(EMyShopPurchaseResult::Success, ItemId, Count);

	// 스트리밍 조건이 셀 수 있도록 구매 사실을 남긴다. 지급까지 성공한 뒤에만 발행한다.
	const AMyPlayerState* BuyerState = Cast<AMyPlayerState>(GetOwner());
	MyStreamingCountEvent::BroadcastItemEvent(
		this,
		MyGameplayTags::Streaming_Event_Item_Purchased,
		ItemId,
		BuyerState ? BuyerState->GetUserIndex() : INDEX_NONE,
		Count);
}

////////////////////////////
//! \author 준혁
//! \brief [서버] 구매 요청을 검증한다.
//!        스택/슬롯 여유를 메소 차감 전에 확인해 "지불했는데 일부만 받는" 상황을 차단한다.
//! \param ShopActor 구매를 요청한 상점 액터
//! \param ItemId 구매할 아이템 ID
//! \param Count 구매 개수
//! \return 검증 결과 (통과 시 Success)
EMyShopPurchaseResult UMyInventoryComponent::ValidatePurchase(const AMyShopActor* ShopActor, FName ItemId, int32 Count) const
{
	if (!IsValid(ShopActor) || ItemId.IsNone() || Count <= 0)
	{
		return EMyShopPurchaseResult::InvalidRequest;
	}

	// 구매자가 실제로 이 상점과 상호작용 중인지 확인한다. (거리 검증은 상호작용 시작 시 서버가 이미 수행)
	const APlayerState* OwnerPlayerState = Cast<APlayerState>(GetOwner());
	const APawn* OwnerPawn = OwnerPlayerState ? OwnerPlayerState->GetPawn() : nullptr;
	if (!OwnerPawn || !ShopActor->IsInteractorActive(OwnerPawn))
	{
		return EMyShopPurchaseResult::InvalidRequest;
	}

	FMyItemData ItemData;
	if (!FindItemData(ItemId, ItemData) || ItemData.BuyPrice <= 0)
	{
		return EMyShopPurchaseResult::NotSold;
	}

	// 스택/슬롯 여유 확인: 보유 중이면 스택 상한까지, 새 종류면 빈 칸이 있어야 한다.
	const int32 OwnedCount = GetItemCount(ItemId);
	if (OwnedCount > 0)
	{
		if (OwnedCount + Count > ItemData.MaxStackCount)
		{
			return EMyShopPurchaseResult::InventoryFull;
		}
	}
	else
	{
		if (IsInventoryFull() || Count > ItemData.MaxStackCount)
		{
			return EMyShopPurchaseResult::InventoryFull;
		}
	}

	// int32 곱 오버플로 방지를 위해 합계는 64비트로 비교한다
	if (static_cast<int64>(ItemData.BuyPrice) * Count > Meso)
	{
		return EMyShopPurchaseResult::NotEnoughMeso;
	}

	return EMyShopPurchaseResult::Success;
}

////////////////////////////
//! \author 준혁
//! \brief 구매 결과를 요청한 플레이어의 클라이언트에 통지한다. (리슨 서버 호스트는 로컬 실행된다)
//! \param Result 서버 처리 결과
//! \param ItemId 구매 요청했던 아이템 ID
//! \param Count 구매 요청했던 개수
void UMyInventoryComponent::Client_PurchaseResult_Implementation(EMyShopPurchaseResult Result, FName ItemId, int32 Count)
{
	OnPurchaseResult.Broadcast(Result, ItemId, Count);
}

////////////////////////////
//! \author 준혁
//! \brief 특정 아이템의 보유 개수를 반환한다.
//! \param ItemId 조회할 아이템 ID
//! \return 보유 개수 (없으면 0)
int32 UMyInventoryComponent::GetItemCount(FName ItemId) const
{
	for (const FMyInventoryEntry& Entry : Entries)
	{
		if (Entry.ItemId == ItemId)
		{
			return Entry.Count;
		}
	}

	return 0;
}

////////////////////////////
//! \author 준혁
//! \brief 데이터테이블에서 아이템 정적 데이터를 찾는다.
//! \param ItemId 조회할 아이템 ID (Row Name)
//! \param OutItemData 찾은 아이템 데이터
//! \return 찾기 성공 여부
bool UMyInventoryComponent::FindItemData(FName ItemId, FMyItemData& OutItemData) const
{
	if (!ItemDataTable || ItemId.IsNone())
	{
		return false;
	}

	const FMyItemData* FoundRow = ItemDataTable->FindRow<FMyItemData>(ItemId, TEXT("MyInventoryComponent::FindItemData"));
	if (!FoundRow)
	{
		return false;
	}

	OutItemData = *FoundRow;
	return true;
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 사용 쿨타임의 남은 시간을 반환한다.
//!        쿨타임 태그를 가진 활성 GE의 잔여 시간을 조회하므로 GE가 복제되는 클라에서도 동작한다.
//! \param ItemId 조회할 아이템 ID
//! \return 남은 쿨타임(초). 쿨타임이 없거나 끝났으면 0
float UMyInventoryComponent::GetItemCooldownRemaining(FName ItemId) const
{
	FMyItemData ItemData;
	if (!FindItemData(ItemId, ItemData) || !ItemData.CooldownTag.IsValid())
	{
		return 0.0f;
	}

	const IAbilitySystemInterface* ASCOwner = Cast<IAbilitySystemInterface>(GetOwner());
	const UAbilitySystemComponent* ASC = ASCOwner ? ASCOwner->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return 0.0f;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ItemData.CooldownTag));
	const TArray<float> RemainingTimes = ASC->GetActiveEffectsTimeRemaining(Query);

	float MaxRemaining = 0.0f;
	for (const float RemainingTime : RemainingTimes)
	{
		MaxRemaining = FMath::Max(MaxRemaining, RemainingTime);
	}

	return MaxRemaining;
}

////////////////////////////
//! \author 준혁
//! \brief 데이터테이블에 정의된 모든 아이템 ID(Row Name)를 반환한다.
//! \param OutItemIds 아이템 ID 목록(출력)
void UMyInventoryComponent::GetAllItemIds(TArray<FName>& OutItemIds) const
{
	OutItemIds.Reset();

	if (ItemDataTable)
	{
		OutItemIds = ItemDataTable->GetRowNames();
	}
}

////////////////////////////
//! \author 준혁
//! \brief 지정한 퀵슬롯에 아이템을 등록한다. (로컬 키셋팅)
//! \note 재접속 복구를 위해 클라이언트 변경이면 서버에도 같은 슬롯 정보를 동기화한다.
//! \param SlotIndex 등록할 퀵슬롯 인덱스
//! \param ItemId 등록할 아이템 ID
//! \return 등록 성공 여부
bool UMyInventoryComponent::AssignQuickSlot(int32 SlotIndex, FName ItemId)
{
    if (!QuickSlotItemIds.IsValidIndex(SlotIndex) || ItemId.IsNone() || GetItemCount(ItemId) <= 0)
    {
        return false;
    }

    // 같은 아이템이 다른 슬롯에 이미 등록되어 있으면 기존 슬롯을 비운다 (중복 등록 방지)
    for (int32 Index = 0; Index < QuickSlotItemIds.Num(); ++Index)
    {
        if (Index != SlotIndex && QuickSlotItemIds[Index] == ItemId)
        {
            QuickSlotItemIds[Index] = NAME_None;
            OnQuickSlotChanged.Broadcast(Index, NAME_None);
        }
    }

    QuickSlotItemIds[SlotIndex] = ItemId;
    OnQuickSlotChanged.Broadcast(SlotIndex, ItemId);

    if (AActor* OwnerActor = GetOwner())
    {
        if (OwnerActor->HasAuthority())
        {
            OwnerActor->ForceNetUpdate();
        }
        else
        {
            Server_AssignQuickSlot(SlotIndex, ItemId);
        }
    }

    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 출발 퀵슬롯의 아이템을 대상 퀵슬롯으로 이동하고, 대상에 아이템이 있으면 두 위치를 교환하는 함수
// SourceSlotIndex : 드래그를 시작한 퀵슬롯 인덱스
// TargetSlotIndex : 아이템을 드롭한 퀵슬롯 인덱스
// 반환값 : 이동 또는 교환에 성공하면 true, 인덱스가 잘못됐거나 출발 슬롯이 비어 있으면 false
bool UMyInventoryComponent::MoveOrSwapQuickSlot(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
    if (!QuickSlotItemIds.IsValidIndex(SourceSlotIndex)
        || !QuickSlotItemIds.IsValidIndex(TargetSlotIndex))
    {
        return false;
    }

    const FName SourceItemId = QuickSlotItemIds[SourceSlotIndex];
    if (SourceItemId.IsNone())
    {
        return false;
    }

    if (SourceSlotIndex == TargetSlotIndex)
    {
        return true;
    }

    const FName TargetItemId = QuickSlotItemIds[TargetSlotIndex];

    // 두 슬롯의 최종 데이터를 먼저 확정한 뒤 알림을 보내 UI가 중간 상태를 읽지 않도록 한다.
    QuickSlotItemIds[SourceSlotIndex] = TargetItemId;
    QuickSlotItemIds[TargetSlotIndex] = SourceItemId;

    OnQuickSlotChanged.Broadcast(SourceSlotIndex, TargetItemId);
    OnQuickSlotChanged.Broadcast(TargetSlotIndex, SourceItemId);

    if (AActor* OwnerActor = GetOwner())
    {
        if (OwnerActor->HasAuthority())
        {
            OwnerActor->ForceNetUpdate();
        }
        else
        {
            Server_MoveOrSwapQuickSlot(SourceSlotIndex, TargetSlotIndex);
        }
    }

    return true;
}

////////////////////////////
//! \author 준혁
//! \brief 지정한 퀵슬롯 등록을 해제한다.
//! \param SlotIndex 해제할 퀵슬롯 인덱스
void UMyInventoryComponent::ClearQuickSlot(int32 SlotIndex)
{
    if (!QuickSlotItemIds.IsValidIndex(SlotIndex))
    {
        return;
    }

    QuickSlotItemIds[SlotIndex] = NAME_None;
    OnQuickSlotChanged.Broadcast(SlotIndex, NAME_None);

    if (AActor* OwnerActor = GetOwner())
    {
        if (OwnerActor->HasAuthority())
        {
            OwnerActor->ForceNetUpdate();
        }
        else
        {
            Server_ClearQuickSlot(SlotIndex);
        }
    }
}

////////////////////////////
//! \author 준혁
//! \brief 비어 있는 첫 퀵슬롯에 아이템을 등록한다. 모두 차 있으면 0번 슬롯을 덮어쓴다.
//! \param ItemId 등록할 아이템 ID
//! \return 등록된 슬롯 인덱스 (실패 시 INDEX_NONE)
int32 UMyInventoryComponent::AssignQuickSlotAuto(FName ItemId)
{
	if (ItemId.IsNone())
	{
		return INDEX_NONE;
	}

	// 이미 등록된 아이템이면 해당 슬롯을 그대로 반환한다
	const int32 ExistingIndex = QuickSlotItemIds.IndexOfByKey(ItemId);
	if (ExistingIndex != INDEX_NONE)
	{
		return ExistingIndex;
	}

	int32 TargetIndex = QuickSlotItemIds.IndexOfByKey(NAME_None);
	if (TargetIndex == INDEX_NONE)
	{
		TargetIndex = 0;
	}

	return AssignQuickSlot(TargetIndex, ItemId) ? TargetIndex : INDEX_NONE;
}

////////////////////////////
//! \author 준혁
//! \brief 지정한 퀵슬롯의 등록 아이템 ID를 반환한다.
//! \param SlotIndex 조회할 퀵슬롯 인덱스
//! \return 등록된 아이템 ID (비어 있으면 NAME_None)
FName UMyInventoryComponent::GetQuickSlotItem(int32 SlotIndex) const
{
	return QuickSlotItemIds.IsValidIndex(SlotIndex) ? QuickSlotItemIds[SlotIndex] : NAME_None;
}

////////////////////////////
//! \author 준혁
//! \brief 퀵슬롯에 등록된 아이템을 사용한다. 키 입력 처리에서 호출한다.
//! \param SlotIndex 사용할 퀵슬롯 인덱스
void UMyInventoryComponent::UseQuickSlot(int32 SlotIndex)
{
	const FName ItemId = GetQuickSlotItem(SlotIndex);
	if (!ItemId.IsNone())
	{
		UseItem(ItemId);
	}
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 재접속 스냅샷의 인벤토리, 퀵슬롯, 메소를 서버 권위로 복원하는 함수
// NewEntries : 복원할 아이템 목록
// NewQuickSlotItemIds : 복원할 퀵슬롯별 아이템 ID
// NewMeso : 복원할 메소
// 반환값 : 서버에서 복원 처리를 완료하면 true
bool UMyInventoryComponent::RestoreReconnectInventory(
    const TArray<FMyInventoryEntry>& NewEntries,
    const TArray<FName>& NewQuickSlotItemIds,
    int32 NewMeso)
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority())
    {
        return false;
    }

    Entries.Reset();
    for (const FMyInventoryEntry& NewEntry : NewEntries)
    {
        FMyItemData ItemData;
        if (NewEntry.ItemId.IsNone()
            || NewEntry.Count <= 0
            || !FindItemData(NewEntry.ItemId, ItemData))
        {
            continue;
        }

        if (FMyInventoryEntry* ExistingEntry = FindEntry(NewEntry.ItemId))
        {
            ExistingEntry->Count = FMath::Clamp(
                ExistingEntry->Count + NewEntry.Count,
                1,
                FMath::Max(1, ItemData.MaxStackCount));
            continue;
        }

        if (Entries.Num() >= GetMaxSlots())
        {
            break;
        }

        FMyInventoryEntry RestoredEntry;
        RestoredEntry.ItemId = NewEntry.ItemId;
        RestoredEntry.Count = FMath::Clamp(NewEntry.Count, 1, FMath::Max(1, ItemData.MaxStackCount));
        Entries.Add(RestoredEntry);
    }

    Meso = FMath::Max(NewMeso, 0);
    QuickSlotItemIds.Init(NAME_None, FMath::Max(1, NumQuickSlots));

    TSet<FName> RestoredQuickSlotItemIds;
    const int32 QuickSlotCount = FMath::Min(QuickSlotItemIds.Num(), NewQuickSlotItemIds.Num());
    for (int32 SlotIndex = 0; SlotIndex < QuickSlotCount; ++SlotIndex)
    {
        const FName ItemId = NewQuickSlotItemIds[SlotIndex];
        if (!ItemId.IsNone()
            && GetItemCount(ItemId) > 0
            && !RestoredQuickSlotItemIds.Contains(ItemId))
        {
            QuickSlotItemIds[SlotIndex] = ItemId;
            RestoredQuickSlotItemIds.Add(ItemId);
        }
    }

    OnRep_Entries();
    OnRep_Meso();
    OnRep_QuickSlotItemIds();
    OwnerActor->ForceNetUpdate();
    return true;
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 클라이언트에서 변경한 퀵슬롯 등록을 서버 인벤토리에 동기화하는 함수 (Reliable)
// SlotIndex : 등록할 퀵슬롯 인덱스
// ItemId : 등록할 아이템 ID
void UMyInventoryComponent::Server_AssignQuickSlot_Implementation(int32 SlotIndex, FName ItemId)
{
    AssignQuickSlot(SlotIndex, ItemId);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 클라이언트에서 변경한 퀵슬롯 이동 또는 교환을 서버 인벤토리에 동기화하는 함수 (Reliable)
// SourceSlotIndex : 이동을 시작한 퀵슬롯 인덱스
// TargetSlotIndex : 이동할 대상 퀵슬롯 인덱스
void UMyInventoryComponent::Server_MoveOrSwapQuickSlot_Implementation(int32 SourceSlotIndex, int32 TargetSlotIndex)
{
    MoveOrSwapQuickSlot(SourceSlotIndex, TargetSlotIndex);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 클라이언트에서 해제한 퀵슬롯을 서버 인벤토리에 동기화하는 함수 (Reliable)
// SlotIndex : 해제할 퀵슬롯 인덱스
void UMyInventoryComponent::Server_ClearQuickSlot_Implementation(int32 SlotIndex)
{
    ClearQuickSlot(SlotIndex);
}

////////////////////////////
//! \author 준혁
//! \brief 아이템 목록 복제 알림. 서버에서도 직접 호출해 리슨서버/데디 양쪽에서 UI 델리게이트를 보장한다.
void UMyInventoryComponent::OnRep_Entries()
{
	OnInventoryUpdated.Broadcast();
}

////////////////////////////
//! \author 준혁
//! \brief 메소 복제 알림. 서버에서도 직접 호출해 UI 델리게이트를 보장한다.
void UMyInventoryComponent::OnRep_Meso()
{
	OnMesoChanged.Broadcast(Meso);
}

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 복제되거나 복원된 전체 퀵슬롯 정보를 UI 델리게이트로 전달하는 함수
void UMyInventoryComponent::OnRep_QuickSlotItemIds()
{
    for (int32 SlotIndex = 0; SlotIndex < QuickSlotItemIds.Num(); ++SlotIndex)
    {
        OnQuickSlotChanged.Broadcast(SlotIndex, QuickSlotItemIds[SlotIndex]);
    }
}

////////////////////////////
//! \author 준혁
//! \brief 내부용: 보유 목록에서 항목을 찾는다.
//! \param ItemId 찾을 아이템 ID
//! \return 항목 포인터 (없으면 nullptr)
FMyInventoryEntry* UMyInventoryComponent::FindEntry(FName ItemId)
{
	return Entries.FindByPredicate([ItemId](const FMyInventoryEntry& InEntry)
	{
		return InEntry.ItemId == ItemId;
	});
}

////////////////////////////
//! \author 장효제
//! \brief 서버에서 확정된 실제 Meso 변화를 GameplayMessageSubsystem으로 발행한다.
//! \param AppliedDelta 실제 적용된 signed Meso 변화량이다.
//! \param CurrentMeso 변화 적용 직후의 최종 Meso 보유량이다.
//! \param SourceTag Meso 변화가 발생한 출처 태그다.
void UMyInventoryComponent::BroadcastMesoFact(
	const int32 AppliedDelta,
	const int32 CurrentMeso,
	const FGameplayTag SourceTag)
{
	const AMyPlayerState* MyPlayerState = Cast<AMyPlayerState>(GetOwner());
	if (!MyPlayerState
		|| !MyPlayerState->HasAuthority()
		|| AppliedDelta == 0
		|| !SourceTag.IsValid()
		|| !UGameplayMessageSubsystem::HasInstance(this))
	{
		return;
	}

	FMyStreamingMesoPayload Payload;
	Payload.EventTag = AppliedDelta > 0
		? MyGameplayTags::Streaming_Event_Meso_Earned
		: MyGameplayTags::Streaming_Event_Meso_Spent;
	Payload.SourceTag = SourceTag;
	Payload.UserIndex = MyPlayerState->GetUserIndex();
	Payload.AppliedDelta = AppliedDelta;
	Payload.CurrentMeso = CurrentMeso;

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		MyGameplayTags::Streaming_Channel_Meso,
		Payload);
}
