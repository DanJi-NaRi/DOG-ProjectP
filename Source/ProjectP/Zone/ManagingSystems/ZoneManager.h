// 

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Zone/Types/ZoneDataTypes.h"
#include "GameFramework/Actor.h"
#include "ZoneManager.generated.h"

class AZoneBase;
class APlayerState;
class AMyPlayerState;
enum class EPlayerLifeState : uint8;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnZoneProgressChangedSignature, int32 /*OldIndex*/, int32 /*NewIndex*/);
DECLARE_MULTICAST_DELEGATE(FOnAllZonesClearedSignature);

UCLASS()
class PROJECTP_API AZoneManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AZoneManager();

	//=== 조회 ===
	UFUNCTION(BlueprintPure, Category = "Zone|Flow")
	int32 GetCurrentZoneIndex() const { return CurrentZoneIndex; }

	UFUNCTION(BlueprintPure, Category = "Zone|Flow")
	AZoneBase* GetZoneAt(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Zone|Flow")
	int32 GetZoneCount() const { return OrderedZones.Num(); }

	//=== 외부 통지 (GameMode / GS / UI 구독용, 서버 전용) ===
	FOnZoneProgressChangedSignature OnZoneProgressChanged;
	FOnAllZonesClearedSignature OnAllZonesCleared;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Zone Order Array, Level에서 순서대로 등록 가능
	UPROPERTY(EditInstanceOnly, Category = "Zone|Order")
	TArray<TObjectPtr<AZoneBase>> OrderedZones;

	//=== 윈도우 규칙: (ZoneIndex - CurrentZoneIndex) 상대거리 → 상태 ===
	UPROPERTY(EditAnywhere, Category = "Zone|Flow")
	TMap<int32, EZoneState> WindowRule;

	//=== 진행 상태 (INDEX_NONE = 아직 Clear된 Zone 없음) ===
	UPROPERTY(VisibleInstanceOnly, Category = "Zone|Flow")
	int32 CurrentZoneIndex = INDEX_NONE;

private:
	void BindZoneDelegates();
	void BindPlayerLifeStateDelegates();
	void ApplyZoneWindow();
	EZoneState ResolveWindowState(int32 Delta) const;
	bool AreAllAlivePlayersEntered() const;
	void TryActivateEnteringZone();

	//=== Zone → Manager 보고 수신 ===
	void HandleZoneCleared(AZoneBase* ClearedZone);
	void HandleZonePlayerEntered(AZoneBase* Zone, APlayerState* EnteredPlayer);
	void HandlePlayerLifeStateChanged(EPlayerLifeState OldLifeState, EPlayerLifeState NewLifeState);
	void BroadcastZoneStreamingEvent(FGameplayTag EventTag, int32 ZoneIndex, int32 InstigatorUserIndex = INDEX_NONE) const;

	// 현재 Entering Zone에 입장 완료한 플레이어 집계
	TSet<TWeakObjectPtr<AMyPlayerState>> EnteredPlayers;

	// 생존자 조건 변경 시 즉시 재판정하기 위해 생명 상태 델리게이트를 구독한 플레이어
	TSet<TWeakObjectPtr<AMyPlayerState>> LifeStateBoundPlayers;
};
