////////////////////////////
//! \page MyInventoryComponent.h
//! \brief 메소와 아이템 보유량을 서버 권위로 관리하고 클라이언트로 복제하는 인벤토리 컴포넌트 선언 파일이다.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Item/MyItemTypes.h"
#include "MyInventoryComponent.generated.h"

class AMyShopActor;
class UDataTable;
class UGameplayEffect;
class UAbilitySystemComponent;
struct FDungeonItemStatEffectSnapshot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryUpdatedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMesoChangedSignature, int32, NewMeso);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FQuickSlotChangedSignature, int32, SlotIndex, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShopPurchaseResultSignature, EMyShopPurchaseResult, Result, FName, ItemId, int32, Count);

////////////////////////////
//! \class UMyInventoryComponent
//! \brief AMyPlayerState에 부착되어 메소/아이템을 관리하는 컴포넌트이다.
//! \note 3인 멀티 기준: 모든 변경은 서버에서만 수행하고(Add/Remove/Use), 클라는 OnRep으로 UI를 갱신한다.
//!       퀵슬롯 키등록은 로컬 클라이언트의 개인 설정이므로 복제하지 않는다.
//! \note 위 퀵슬롯 설명은 기존 설계 기록이며, 현재는 로컬 UI에 즉시 반영한 뒤 서버에 동기화하고 소유 클라이언트로 복제한다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTP_API UMyInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMyInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ 서버 전용 변경 API (클라에서 호출하면 무시된다)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Meso")
	void AddMeso(int32 Amount, FGameplayTag SourceTag);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Meso")
    void SetMeso(int32 NewMeso);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Meso")
	bool TryConsumeMeso(int32 Amount, FGameplayTag SourceTag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Item")
	bool AddItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Item")
	bool RemoveItem(FName ItemId, int32 Count = 1);
	//~end of 서버 전용 변경 API

	//! 로컬 UI에서 호출하는 아이템 사용 진입점. 내부에서 Server RPC로 라우팅한다.
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void UseItem(FName ItemId);

	//! 로컬 상점 UI에서 호출하는 구매 진입점. 내부에서 Server RPC로 라우팅하고, 결과는 OnPurchaseResult로 통지된다.
	UFUNCTION(BlueprintCallable, Category = "Inventory|Shop")
	void RequestPurchase(AMyShopActor* ShopActor, FName ItemId, int32 Count = 1);

	//~ 조회 API
	UFUNCTION(BlueprintPure, Category = "Inventory|Meso")
	int32 GetMeso() const { return Meso; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	const TArray<FMyInventoryEntry>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int32 GetItemCount(FName ItemId) const;

	//! 인벤토리 최대 슬롯(칸) 수를 반환한다. 한 칸 = 아이템 1종(같은 종류는 스택).
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int32 GetMaxSlots() const { return FMath::Max(1, MaxSlots); }

	//! 현재 사용 중인 슬롯 수(보유 아이템 종류 수)를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int32 GetUsedSlots() const { return Entries.Num(); }

	//! 인벤토리가 가득 찼는지(빈 칸이 없는지) 반환한다.
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsInventoryFull() const { return Entries.Num() >= GetMaxSlots(); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool FindItemData(FName ItemId, FMyItemData& OutItemData) const;

	//! 데이터테이블에 정의된 모든 아이템 ID(Row Name)를 반환한다. (치트 자동완성 등에서 사용)
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	void GetAllItemIds(TArray<FName>& OutItemIds) const;

	//! 아이템 사용 쿨타임의 남은 시간을 반환한다. 쿨타임 태그의 활성 GE 잔여 시간 기준이라 클라에서도 동작한다.
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	float GetItemCooldownRemaining(FName ItemId) const;
	//~end of 조회 API

	//~ 퀵슬롯(키셋팅) API - 로컬 전용, 복제하지 않음
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	bool AssignQuickSlot(int32 SlotIndex, FName ItemId);

    //! 출발 퀵슬롯의 아이템을 대상 퀵슬롯으로 이동한다. 대상에 아이템이 있으면 두 위치를 교환한다.
    UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
    bool MoveOrSwapQuickSlot(int32 SourceSlotIndex, int32 TargetSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void ClearQuickSlot(int32 SlotIndex);

	//! 비어 있는 첫 퀵슬롯에 등록한다. 모두 차 있으면 0번을 덮어쓴다.
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	int32 AssignQuickSlotAuto(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	FName GetQuickSlotItem(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|QuickSlot")
	int32 GetQuickSlotCount() const { return QuickSlotItemIds.Num(); }

    //! 서버 재접속 스냅샷에 저장할 전체 퀵슬롯 아이템 ID를 반환한다.
    const TArray<FName>& GetQuickSlotItemIds() const { return QuickSlotItemIds; }

	//! 퀵슬롯에 등록된 아이템을 사용한다. (키 입력 → 이 함수 → Server RPC)
	UFUNCTION(BlueprintCallable, Category = "Inventory|QuickSlot")
	void UseQuickSlot(int32 SlotIndex);

    //! 서버 재접속 스냅샷에서 인벤토리, 퀵슬롯, 메소를 복원한다.
    bool RestoreReconnectInventory(
        const TArray<FMyInventoryEntry>& NewEntries,
        const TArray<FName>& NewQuickSlotItemIds,
        int32 NewMeso);

    //! 서버 재접속 스냅샷에 저장할 활성 아이템 스탯 효과를 생성한다.
    TArray<FDungeonItemStatEffectSnapshot> MakeReconnectItemStatEffectSnapshots(
        const UAbilitySystemComponent* AbilitySystemComponent) const;

    //! 서버 재접속 스냅샷의 아이템 스탯 효과를 경과 시간을 반영해 복원한다.
    bool RestoreReconnectItemStatEffects(
        UAbilitySystemComponent* AbilitySystemComponent,
        const TArray<FDungeonItemStatEffectSnapshot>& EffectSnapshots,
        float ElapsedSeconds) const;
	//~end of 퀵슬롯 API

	//! 인벤토리 항목이 바뀔 때(획득/소모) 알림. UI 갱신용.
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryUpdatedSignature OnInventoryUpdated;

	//! 메소가 바뀔 때 알림. UI 갱신용.
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FMesoChangedSignature OnMesoChanged;

	//! 퀵슬롯 등록이 바뀔 때 알림. HUD 아이템 슬롯 갱신용.
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FQuickSlotChangedSignature OnQuickSlotChanged;

	//! 구매 요청의 서버 처리 결과 알림. 상점 UI 피드백용. (요청한 플레이어의 클라이언트에서만 발화)
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FShopPurchaseResultSignature OnPurchaseResult;

protected:
	virtual void BeginPlay() override;

	//! 아이템 정적 데이터 테이블 (Row = FMyItemData, Row Name = ItemId)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ItemDataTable;

	//! 아이템 사용 쿨타임용 공용 GE. Duration = SetByCaller(Data.Cooldown)로 만들고,
	//! 부여 태그는 아이템별 CooldownTag를 코드가 DynamicGrantedTags로 주입한다. (BP_MyPlayerState 디폴트에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UGameplayEffect> ItemCooldownEffectClass;

	//! 스탯강화 아이템(지속 버프)용 공용 GE. HasDuration + Duration = SetByCaller(Data.Duration),
	//! 스탯별 Add 모디파이어(SetByCaller Data.Stat.*)로 만들면 모든 스탯 아이템이 이 GE 하나를 공유한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UGameplayEffect> ItemStatBuffEffectClass;

	//! 스탯강화 아이템(영구)용 공용 GE. 모디파이어 구성은 버프용과 동일하되 Duration Policy = Infinite.
	//! Instant(Base 직접 변경)가 아닌 Infinite인 이유: 레벨업이 SetNumericAttributeBase로 Base를 덮어쓰므로
	//! Base에 더한 값은 레벨업 시 사라진다. Infinite 모디파이어는 Base 덮어쓰기와 공존한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UGameplayEffect> ItemStatPermanentEffectClass;

	//! 인벤토리 최대 슬롯(칸) 수. 한 칸 = 아이템 1종이며, 같은 종류는 MaxStackCount까지 한 칸에 스택된다.
	//! 칸이 가득 차면 새 종류의 아이템 지급(AddItem)은 실패한다. (UI 그리드는 5열 x 6행 = 30칸 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 MaxSlots = 30;

	//! 퀵슬롯 개수 (기본 4칸)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|QuickSlot", meta = (ClampMin = "1"))
	int32 NumQuickSlots = 4;

private:
	void BroadcastMesoFact(
		int32 AppliedDelta,
		int32 CurrentMeso,
		FGameplayTag SourceTag);

	UFUNCTION(Server, Reliable)
	void Server_UseItem(FName ItemId);

	UFUNCTION(Server, Reliable)
	void Server_PurchaseItem(AMyShopActor* ShopActor, FName ItemId, int32 Count);

    UFUNCTION(Server, Reliable)
    void Server_AssignQuickSlot(int32 SlotIndex, FName ItemId);

    UFUNCTION(Server, Reliable)
    void Server_MoveOrSwapQuickSlot(int32 SourceSlotIndex, int32 TargetSlotIndex);

    UFUNCTION(Server, Reliable)
    void Server_ClearQuickSlot(int32 SlotIndex);

	UFUNCTION(Client, Reliable)
	void Client_PurchaseResult(EMyShopPurchaseResult Result, FName ItemId, int32 Count);

	//! [서버] 구매 요청을 검증한다. 상점 상호작용 상태/판매 여부/스택·슬롯 여유/메소까지 확인한다.
	EMyShopPurchaseResult ValidatePurchase(const AMyShopActor* ShopActor, FName ItemId, int32 Count) const;

	//! [서버] 스탯강화 아이템의 StatModifiers를 공용 스탯 GE에 SetByCaller로 주입해 적용한다.
	void ApplyStatModifiers(class UAbilitySystemComponent* ASC, const FMyItemData& ItemData) const;

	UFUNCTION()
	void OnRep_Entries();

	UFUNCTION()
	void OnRep_Meso();

    UFUNCTION()
    void OnRep_QuickSlotItemIds();

	FMyInventoryEntry* FindEntry(FName ItemId);

	//! 보유 아이템 목록. 서버에서만 변경한다.
	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FMyInventoryEntry> Entries;

	//! 보유 메소. 서버에서만 변경한다.
	UPROPERTY(ReplicatedUsing = OnRep_Meso)
	int32 Meso = 0;

    //! 퀵슬롯별 등록 아이템 ID. 로컬 키셋팅이므로 복제하지 않는다.
    //! 위 설명은 기존 설계 기록이며, 현재는 클라이언트 변경을 서버에 동기화하고 소유 클라이언트에 복제한다.
    UPROPERTY(Transient, ReplicatedUsing = OnRep_QuickSlotItemIds)
    TArray<FName> QuickSlotItemIds;
};
