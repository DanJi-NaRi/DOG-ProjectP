////////////////////////////
//! \page MyItemTypes.h
//! \brief 아이템 타입, 데이터테이블 Row, 인벤토리 항목 등 아이템 시스템 공용 타입 정의 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "MyItemTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

//! \enum EMyShopPurchaseResult 상점 구매 요청의 서버 처리 결과. 상점 UI의 피드백 문구 분기에 사용한다.
UENUM(BlueprintType)
enum class EMyShopPurchaseResult : uint8
{
	Success           UMETA(DisplayName = "성공"),
	InvalidRequest    UMETA(DisplayName = "잘못된 요청"),
	NotSold           UMETA(DisplayName = "비매품"),
	NotEnoughMeso UMETA(DisplayName = "메소 부족"),
	InventoryFull     UMETA(DisplayName = "인벤토리 가득 참")
};

//! \enum EMyItemType 아이템 대분류. 인벤토리 타입별 정렬 기준으로도 사용한다.
UENUM(BlueprintType)
enum class EMyItemType : uint8
{
	Consumable UMETA(DisplayName = "소비"),
	Material   UMETA(DisplayName = "재료"),
	Quest      UMETA(DisplayName = "퀘스트"),
	Etc        UMETA(DisplayName = "기타")
};

//! \enum EMyItemStatType 스탯강화 아이템이 올릴 수 있는 스탯 종류. UMyAttributeSet의 어트리뷰트와 1:1 대응한다.
UENUM(BlueprintType)
enum class EMyItemStatType : uint8
{
	AttackPower       UMETA(DisplayName = "공격력"),
	Defense           UMETA(DisplayName = "방어력"),
	MaxHealth         UMETA(DisplayName = "최대 체력"),
	MoveSpeed         UMETA(DisplayName = "이동 속도"),
	AttackSpeed       UMETA(DisplayName = "공격 속도"),
	CritChance        UMETA(DisplayName = "치명타 확률"),
	CooldownReduction UMETA(DisplayName = "쿨타임 감소")
};

//! \enum EMyItemStatModOp 스탯 상승치 계산 방식.
UENUM(BlueprintType)
enum class EMyItemStatModOp : uint8
{
	//! Value를 그대로 더한다 (공격력 +10). 확률형 스탯(치명타/쿨감)은 0~1 스케일이므로 0.05 = +5%p
	Add        UMETA(DisplayName = "고정치 증가"),
	//! 적용 시점의 베이스 값 대비 Value%만큼 더한다 (Value=15 → 공격력 +15%)
	PercentAdd UMETA(DisplayName = "베이스 % 증가")
};

////////////////////////////
//! \struct FMyItemStatModifier
//! \brief 스탯강화 아이템의 스탯 상승 한 줄(어떤 스탯을, 어떤 방식으로, 얼마나). 아이템 하나가 여러 개 가질 수 있다.
USTRUCT(BlueprintType)
struct FMyItemStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EMyItemStatType StatType = EMyItemStatType::AttackPower;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EMyItemStatModOp Op = EMyItemStatModOp::Add;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float Value = 0.0f;
};

////////////////////////////
//! \struct FMyItemData
//! \brief 아이템 정적 데이터. DataTable(Row Name = ItemId)로 관리한다.
//! \note 상점(구매/판매 가격) 확장을 대비해 가격 필드를 미리 포함한다.
USTRUCT(BlueprintType)
struct FMyItemData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EMyItemType ItemType = EMyItemType::Etc;

	//! 한 슬롯에 겹쳐 쌓을 수 있는 최대 개수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackCount = 99;

	//! 사용(소비) 가능한 아이템인지 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use")
	bool bUsable = false;

	//! 사용 시 소유자 ASC에 적용할 GameplayEffect (서버에서만 적용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable"))
	TSubclassOf<UGameplayEffect> UseEffectClass;

	//! 최대 체력 대비 회복 비율 (0.3 = 30%). 0보다 크면 사용 시 MaxHealth * HealRatio를
	//! Data.Heal SetByCaller로 UseEffectClass에 전달한다. (포션 소/중/대의 회복량 차등에 사용)
	//! \note UseEffectClass에는 Data.Heal SetByCaller를 IncomingHeal에 더하는 GE를 지정해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable", ClampMin = "0.0", ClampMax = "1.0"))
	float HealRatio = 0.0f;

	//! 최대 체력 대비 보호막 비율 (0.2 = 20%). 0보다 크면 사용 시 MaxHealth * ShieldRatio를
	//! Data.Shield SetByCaller로 UseEffectClass에 전달한다. 보호막은 소진 시까지 유지되며
	//! AttributeSet의 IncomingShield 처리 정책상 더 큰 값으로만 갱신된다.
	//! \note UseEffectClass에는 Data.Shield SetByCaller를 IncomingShield에 더하는 Instant GE를 지정해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable", ClampMin = "0.0", ClampMax = "1.0"))
	float ShieldRatio = 0.0f;

	//! 사용 쿨타임 태그. 같은 태그를 쓰는 아이템끼리 쿨타임을 공유한다 (포션 3종 = Cooldown.Item.Potion)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable", Categories = "Cooldown"))
	FGameplayTag CooldownTag;

	//! 사용 쿨타임(초). 0이면 쿨타임 없음. 서버가 검증하고 클라 HUD는 태그 복제로 표시한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable", ClampMin = "0.0"))
	float CooldownSeconds = 0.0f;

	//! 사용 시 올릴 스탯 목록 (스탯강화 아이템). 비어 있으면 스탯 효과 없음.
	//! 컴포넌트의 공용 스탯 GE(ItemStatBuffEffectClass/ItemStatPermanentEffectClass)에 SetByCaller로 주입되므로
	//! 아이템별 GE를 따로 만들 필요가 없다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable"))
	TArray<FMyItemStatModifier> StatModifiers;

	//! 스탯 상승 지속시간(초). 0이면 영구 적용(Infinite GE), 0보다 크면 해당 시간 동안만 버프(HasDuration GE).
	//! \note BuffGroupTag가 비어 있으면 같은 버프를 중복 사용 시 효과가 겹쳐 쌓인다 — 중첩을 막으려면
	//!       BuffGroupTag를 지정(계열 덮어쓰기)하거나 CooldownSeconds를 지속시간 이상으로 설정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable", ClampMin = "0.0"))
	float StatDurationSeconds = 0.0f;

	//! 버프 계열 태그 (계열 덮어쓰기). 사용 시 같은 태그가 부여된 기존 스탯 버프 GE를 제거하고 새로 적용한다.
	//! 비어 있으면 덮어쓰기 없이 중첩 허용. (기획 스펙: 강화제는 계열 덮어쓰기·쿨타임 없음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Use", meta = (EditCondition = "bUsable", Categories = "Item.BuffGroup"))
	FGameplayTag BuffGroupTag;

	//! 상점 구매 가격 (0이면 비매품)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Shop", meta = (ClampMin = "0"))
	int32 BuyPrice = 0;

	//! 상점 판매 가격 (0이면 판매 불가)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Shop", meta = (ClampMin = "0"))
	int32 SellPrice = 0;
};

////////////////////////////
//! \struct FMyInventoryEntry
//! \brief 인벤토리에 보유 중인 아이템 한 항목(아이템 ID + 개수). 서버에서 클라로 복제된다.
USTRUCT(BlueprintType)
struct FMyInventoryEntry
{
	GENERATED_BODY()

	//! ItemDataTable의 Row Name과 일치하는 아이템 식별자
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Count = 0;
};
