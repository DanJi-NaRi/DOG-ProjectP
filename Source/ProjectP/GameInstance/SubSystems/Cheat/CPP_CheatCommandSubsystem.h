#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CPP_CheatCommandSubsystem.generated.h"

class AMyPlayerController;

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 치트 명령이 사용 가능한 월드 범위
UENUM(BlueprintType)
enum class ECheatCommandScope : uint8
{
    All     UMETA(DisplayName = "전체"),
    Lobby   UMETA(DisplayName = "로비"),
    Dungeon UMETA(DisplayName = "던전")
};

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 치트 명령 1개의 표시/검색용 정보
USTRUCT(BlueprintType)
struct FCheatCommandInfo
{
    GENERATED_BODY()

public:
    // 명령 이름 (접두어 '/' 제외, 예: "heal")
    UPROPERTY(BlueprintReadOnly, Category = "Cheat")
    FString Name;

    // 사용법 표기 (예: "/heal [양]")
    UPROPERTY(BlueprintReadOnly, Category = "Cheat")
    FString Usage;

    // 자동완성 목록에 함께 표시할 설명
    UPROPERTY(BlueprintReadOnly, Category = "Cheat")
    FString Description;

    // 사용 가능한 월드 범위 (로비/던전/전체)
    UPROPERTY(BlueprintReadOnly, Category = "Cheat")
    ECheatCommandScope Scope = ECheatCommandScope::All;

    // true이면 서버로 보내지 않고 요청한 클라이언트에서 즉시 실행한다. (FPS 제한 등 로컬 설정용)
    UPROPERTY(BlueprintReadOnly, Category = "Cheat")
    bool bRunOnClient = false;
};

// 치트 명령 핸들러. 요청자와 인자 목록을 받아 채팅창에 표시할 결과 문자열을 반환한다.
DECLARE_DELEGATE_RetVal_TwoParams(FString, FCheatCommandHandler, AMyPlayerController* /*Requester*/, const TArray<FString>& /*Args*/);

// 치트 인자 자동완성 제공자. 입력 중인 인자 인덱스와 접두어를 받아 후보 문자열 목록을 채운다. (클라 자동완성용)
DECLARE_DELEGATE_ThreeParams(FCheatArgumentSuggestionProvider, int32 /*ArgIndex*/, const FString& /*Prefix*/, TArray<FString>& /*OutSuggestions*/);

//////////////////////////////////////////////////////////////////////
// - 준혁 -
// 채팅창 기반 테스트 치트 명령 레지스트리 월드 서브시스템
// 서버는 ExecuteCommand로 명령을 실행하고, 클라는 GetMatchingCommands로 자동완성 목록을 얻는다.
// 새 치트는 FCheatCommandInfo + 핸들러를 RegisterCommand로 등록하면 /help와 자동완성에 자동 반영된다.
UCLASS()
class PROJECTP_API UCPP_CheatCommandSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // 치트 명령을 등록한다. 같은 이름이 이미 있으면 덮어쓴다.
    void RegisterCommand(const FCheatCommandInfo& CommandInfo, FCheatCommandHandler CommandHandler);

    // 명령의 인자 자동완성 제공자를 등록한다. 같은 명령에 다시 등록하면 덮어쓴다.
    void RegisterArgumentSuggestionProvider(const FString& CommandName, FCheatArgumentSuggestionProvider Provider);

    // 명령의 인자 후보 목록을 반환한다. 제공자가 없거나 현재 월드에서 사용 불가한 명령이면 빈 목록.
    void GetArgumentSuggestions(const FString& CommandName, int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions) const;

    // 현재 월드에서 사용 가능한 명령 목록을 반환한다. (자동완성/도움말용, 클라에서도 사용)
    UFUNCTION(BlueprintCallable, Category = "Cheat")
    void GetAvailableCommands(TArray<FCheatCommandInfo>& OutCommands) const;

    // 입력 접두어로 시작하는 사용 가능 명령 목록을 반환한다. 접두어가 비어 있으면 전체를 반환한다.
    UFUNCTION(BlueprintCallable, Category = "Cheat")
    void GetMatchingCommands(const FString& NamePrefix, TArray<FCheatCommandInfo>& OutCommands) const;

    // 명령 이름으로 등록 정보를 찾는다. (클라 즉시 실행 여부 판단용)
    UFUNCTION(BlueprintCallable, Category = "Cheat")
    bool FindCommandInfo(const FString& CommandName, FCheatCommandInfo& OutCommandInfo) const;

    // 이 월드에서 채팅 치트 실행이 허용되는지 확인한다. (Standalone은 항상 허용, 그 외는 서버 설정)
    UFUNCTION(BlueprintPure, Category = "Cheat")
    bool AreChatCheatsAllowed() const;

    // 명령 문자열을 파싱해 실행하고 결과 문자열을 반환한다. 명령 설정에 따라 서버 또는 로컬 클라이언트에서 호출된다.
    FString ExecuteCommand(AMyPlayerController* Requester, const FString& CommandLine);

private:
    struct FRegisteredCheatCommand
    {
        FCheatCommandInfo Info;
        FCheatCommandHandler Handler;
    };

    // 등록 순서 유지를 위해 배열로 보관한다. (자동완성/도움말 표시 순서)
    TArray<FRegisteredCheatCommand> Commands;

    // 명령 이름(소문자) -> 인자 자동완성 제공자
    TMap<FString, FCheatArgumentSuggestionProvider> ArgumentSuggestionProviders;

    ECheatCommandScope GetCurrentWorldScope() const;
    bool IsCommandAvailableInCurrentWorld(const FCheatCommandInfo& CommandInfo) const;
    void RegisterBuiltInCommands();

    // --- 기본 제공 명령 핸들러 ---
    FString HandleHelpCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleSelectCharacterCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleHealCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleKillCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleReviveCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleFpsCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleAddItemCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleAddMesoCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleSetLevelCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleAddExpCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleSetSpeedCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleStatsCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleDebugLineCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleSpawnMonsterCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
#if !UE_BUILD_SHIPPING
    FString HandleClearZoneCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
    FString HandleMakeBubblesCommand(AMyPlayerController* Requester, const TArray<FString>& Args);
#endif

    // /stats로 토글되는 스탯 디버그 위젯 (로컬 클라 전용)
    UPROPERTY(Transient)
    TObjectPtr<class UMyStatDebugWidget> StatDebugWidget;

    // --- 인자 자동완성 제공자 ---
    void HandleAddItemArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions);
    void HandleReviveArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions);
    void HandleDebugLineArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions);
    void HandleSpawnMonsterArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions);
#if !UE_BUILD_SHIPPING
    void HandleClearZoneArgumentSuggestions(int32 ArgIndex, const FString& Prefix, TArray<FString>& OutSuggestions);
#endif
};
