#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "../Item/MyItemTypes.h"
#include "DungeonReconnectTypes.generated.h"

UENUM(BlueprintType)
enum class EDungeonReconnectStep : uint8
{
    Stage1,
    Rest1,
    Stage2,
    Rest2,
    Boss
};

USTRUCT(BlueprintType)
struct PROJECTP_API FDungeonAttributeSnapshot
{
    GENERATED_BODY()

    //! 캐릭터 레벨. 재접속 시 레벨 스탯 재적용의 기준이 된다.
    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    int32 CharacterLevel = 1;

    //! 현재 레벨의 누적 경험치.
    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    int32 CharacterExp = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float Health = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float MaxHealth = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float AttackPower = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float Defense = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float MoveSpeed = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float Shield = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float CritChance = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float CritDamage = 1.5f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float AttackSpeed = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float CooldownReduction = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float MaxMoveCharge = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float MoveCharge = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float CurseGauge = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTP_API FDungeonSkillCooldownSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    FGameplayTag CooldownTag;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float RemainingSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTP_API FDungeonItemStatEffectSnapshot
{
    GENERATED_BODY()

    //! 공용 스탯 GameplayEffect에 주입되었던 Data.Stat.* SetByCaller 값.
    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    TMap<FGameplayTag, float> StatMagnitudes;

    //! 같은 계열 버프 덮어쓰기에 사용한 동적 부여 태그.
    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    FGameplayTagContainer DynamicGrantedTags;

    //! 시간제 버프를 저장한 시점의 남은 시간. 영구 효과에서는 사용하지 않는다.
    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    float RemainingSeconds = 0.0f;

    //! ItemStatPermanentEffectClass로 적용된 무기한 효과인지 여부.
    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    bool bPermanent = false;
};

USTRUCT(BlueprintType)
struct PROJECTP_API FDungeonReconnectSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    int32 UserIndex = -1;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    FString Username;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    int32 SelectedCharacterId = -1;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    int32 Meso = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    TArray<FMyInventoryEntry> InventoryEntries;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    TArray<FName> QuickSlotItemIds;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    TArray<FDungeonSkillCooldownSnapshot> SkillCooldowns;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    TArray<FDungeonItemStatEffectSnapshot> ItemStatEffects;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    double SavedServerTimeSeconds = 0.0;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    EDungeonReconnectStep SavedDungeonStep = EDungeonReconnectStep::Stage1;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    FTransform SavedTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    FDungeonAttributeSnapshot AttributeSnapshot;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    TSoftClassPtr<APawn> CharacterClass;

    UPROPERTY(BlueprintReadWrite, Category = "Dungeon|Reconnect")
    bool bOutGame = false;
};
