const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");

function readProjectFile(...segments) {
  return fs.readFileSync(path.join(projectRoot, ...segments), "utf8");
}

function getFunctionBody(source, signature) {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `${signature} should exist`);

  const bodyStart = source.indexOf("{", start);
  assert.notEqual(bodyStart, -1, `${signature} should have a body`);

  let depth = 0;
  for (let index = bodyStart; index < source.length; ++index) {
    if (source[index] === "{") {
      ++depth;
    } else if (source[index] === "}") {
      --depth;
      if (depth === 0) {
        return source.slice(bodyStart + 1, index);
      }
    }
  }

  assert.fail(`${signature} body should be closed`);
}

test("global Notice widget exposes timed and countdown presentation contracts", () => {
  const noticeHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Widget", "MyNoticeWidget.h");
  const noticeSourcePath = path.join(projectRoot, "Source", "ProjectP", "Widget", "MyNoticeWidget.cpp");

  assert.equal(fs.existsSync(noticeHeaderPath), true, "MyNoticeWidget.h should exist");
  assert.equal(fs.existsSync(noticeSourcePath), true, "MyNoticeWidget.cpp should exist");

  const noticeHeader = fs.readFileSync(noticeHeaderPath, "utf8");
  const noticeSource = fs.readFileSync(noticeSourcePath, "utf8");

  assert.match(noticeHeader, /UMyNoticeWidget\s*:\s*public\s+UCommonUserWidget/);
  assert.match(noticeHeader, /public:[\s\S]*void\s+ShowNotice\(const FText& Message, float DurationSeconds\)/);
  assert.match(noticeHeader, /public:[\s\S]*void\s+ShowCountdownNotice\(const FText& MessageFormat, float EndServerTime\)/);
  assert.match(noticeHeader, /public:[\s\S]*void\s+ClearNotice\(\)/);
  assert.match(noticeHeader, /protected:[\s\S]*BindWidgetOptional[\s\S]*TXT_Notice/);
  assert.match(noticeHeader, /private:[\s\S]*PendingNotices/);
  assert.match(noticeHeader, /private:[\s\S]*CountdownEndServerTime/);
  assert.match(noticeSource, /GetServerWorldTimeSeconds\(\)/);
  assert.match(noticeSource, /FText::Format\(ActiveCountdownMessageFormat, RemainingSeconds\)/);

  const countdownBody = getFunctionBody(
    noticeSource,
    "void UMyNoticeWidget::ShowCountdownNotice(const FText& MessageFormat, float EndServerTime)",
  );
  const durationElapsedBody = getFunctionBody(
    noticeSource,
    "void UMyNoticeWidget::HandleNoticeDurationElapsed()",
  );

  assert.match(countdownBody, /EndServerTime\s*<=\s*GetSyncedServerWorldTimeSeconds\(\)/);
  assert.doesNotMatch(durationElapsedBody, /bNoticeVisible\s*=\s*false/);
});

test("UI manager owns one persistent Notice widget and forwards display requests", () => {
  const managerHeader = readProjectFile("Source", "ProjectP", "Widget", "MyUIManagerSubsystem.h");
  const managerSource = readProjectFile("Source", "ProjectP", "Widget", "MyUIManagerSubsystem.cpp");

  assert.match(managerHeader, /class\s+UMyNoticeWidget;/);
  assert.match(
    managerHeader,
    /public:[\s\S]*UMyNoticeWidget\*\s+EnsureNoticeWidget\(\s*APlayerController\* OwningPlayer,\s*TSubclassOf<UMyNoticeWidget> WidgetClass\s*\)/,
  );
  assert.match(managerHeader, /public:[\s\S]*void\s+ShowNotice\(const FText& Message, float DurationSeconds\)/);
  assert.match(
    managerHeader,
    /public:[\s\S]*void\s+ShowCountdownNotice\(const FText& MessageFormat, float EndServerTime\)/,
  );
  assert.match(managerHeader, /public:[\s\S]*void\s+ClearNotice\(\)/);
  assert.match(managerHeader, /private:[\s\S]*TObjectPtr<UMyNoticeWidget>\s+NoticeWidget/);

  const ensureNoticeBody = getFunctionBody(
    managerSource,
    "UMyNoticeWidget* UMyUIManagerSubsystem::EnsureNoticeWidget(",
  );
  const showNoticeBody = getFunctionBody(
    managerSource,
    "void UMyUIManagerSubsystem::ShowNotice(const FText& Message, float DurationSeconds)",
  );
  const showCountdownBody = getFunctionBody(
    managerSource,
    "void UMyUIManagerSubsystem::ShowCountdownNotice(const FText& MessageFormat, float EndServerTime)",
  );
  const clearNoticeBody = getFunctionBody(
    managerSource,
    "void UMyUIManagerSubsystem::ClearNotice()",
  );
  const deinitializeBody = getFunctionBody(
    managerSource,
    "void UMyUIManagerSubsystem::Deinitialize()",
  );
  const ensureLayoutBody = getFunctionBody(
    managerSource,
    "UMyPrimaryGameLayout* UMyUIManagerSubsystem::EnsurePrimaryLayoutUsingClass(",
  );

  assert.match(ensureNoticeBody, /bCanReuseNoticeWidget[\s\S]*return NoticeWidget/);
  assert.match(ensureNoticeBody, /NoticeWidget->RemoveFromParent\(\)/);
  assert.match(ensureNoticeBody, /AddPersistentWidget\(WidgetClass\)/);
  assert.match(showNoticeBody, /NoticeWidget->ShowNotice\(Message, DurationSeconds\)/);
  assert.match(showCountdownBody, /NoticeWidget->ShowCountdownNotice\(MessageFormat, EndServerTime\)/);
  assert.match(clearNoticeBody, /NoticeWidget->ClearNotice\(\)/);
  assert.match(deinitializeBody, /NoticeWidget\s*=\s*nullptr/);
  assert.match(ensureLayoutBody, /NoticeWidget\s*=\s*nullptr/);
});

test("dungeon player controller creates Notice and provides server-to-owner delivery", () => {
  const dungeonPCHeader = readProjectFile("Source", "ProjectP", "Dungeon", "DungeonPC.h");
  const dungeonPCSource = readProjectFile("Source", "ProjectP", "Dungeon", "DungeonPC.cpp");

  assert.match(dungeonPCHeader, /class\s+UMyNoticeWidget;/);
  assert.match(
    dungeonPCHeader,
    /public:[\s\S]*void\s+SendNoticeToClient\(const FText& Message, float DurationSeconds\)/,
  );
  assert.match(
    dungeonPCHeader,
    /public:[\s\S]*void\s+SendCountdownNoticeToClient\(const FText& MessageFormat, float EndServerTime\)/,
  );
  assert.match(
    dungeonPCHeader,
    /protected:[\s\S]*EditDefaultsOnly[\s\S]*TSubclassOf<UMyNoticeWidget>\s+NoticeWidgetClass/,
  );
  assert.match(
    dungeonPCHeader,
    /private:[\s\S]*UFUNCTION\(Client, Reliable\)\s*void\s+ClientReceiveNotice\(const FText& Message, float DurationSeconds\)/,
  );
  assert.match(
    dungeonPCHeader,
    /private:[\s\S]*UFUNCTION\(Client, Reliable\)\s*void\s+ClientReceiveCountdownNotice\(const FText& MessageFormat, float EndServerTime\)/,
  );

  const beginPlayBody = getFunctionBody(dungeonPCSource, "void ADungeonPC::BeginPlay()");
  const sendNoticeBody = getFunctionBody(
    dungeonPCSource,
    "void ADungeonPC::SendNoticeToClient(const FText& Message, float DurationSeconds)",
  );
  const sendCountdownBody = getFunctionBody(
    dungeonPCSource,
    "void ADungeonPC::SendCountdownNoticeToClient(const FText& MessageFormat, float EndServerTime)",
  );
  const receiveNoticeBody = getFunctionBody(
    dungeonPCSource,
    "void ADungeonPC::ClientReceiveNotice_Implementation(const FText& Message, float DurationSeconds)",
  );
  const receiveCountdownBody = getFunctionBody(
    dungeonPCSource,
    "void ADungeonPC::ClientReceiveCountdownNotice_Implementation(const FText& MessageFormat, float EndServerTime)",
  );

  const surrenderWidgetIndex = beginPlayBody.indexOf("AddPersistentWidget(SurrenderVotePanelClass)");
  const noticeWidgetIndex = beginPlayBody.indexOf("EnsureNoticeWidget(this, NoticeWidgetClass)");
  assert.notEqual(surrenderWidgetIndex, -1);
  assert.notEqual(noticeWidgetIndex, -1);
  assert.ok(noticeWidgetIndex > surrenderWidgetIndex, "Notice should be inserted after surrender UI so it draws above it");

  assert.match(sendNoticeBody, /HasAuthority\(\)/);
  assert.match(sendNoticeBody, /ClientReceiveNotice\(Message, DurationSeconds\)/);
  assert.match(sendCountdownBody, /HasAuthority\(\)/);
  assert.match(sendCountdownBody, /ClientReceiveCountdownNotice\(MessageFormat, EndServerTime\)/);
  assert.match(receiveNoticeBody, /UIManager->ShowNotice\(Message, DurationSeconds\)/);
  assert.match(receiveCountdownBody, /UIManager->ShowCountdownNotice\(MessageFormat, EndServerTime\)/);
});

test("surrender vote panel is rendered above CommonUI layer stacks without occupying a stack", () => {
  const dungeonPCSource = readProjectFile("Source", "ProjectP", "Dungeon", "DungeonPC.cpp");
  const votePanelHeader = readProjectFile("Source", "ProjectP", "Dungeon", "MySurrenderVotePanelWidget.h");
  const primaryLayoutSource = readProjectFile("Source", "ProjectP", "Widget", "MyPrimaryGameLayout.cpp");
  const persistentOverlayBody = getFunctionBody(
    primaryLayoutSource,
    "UOverlay* UMyPrimaryGameLayout::GetOrCreatePersistentOverlay()",
  );

  assert.match(votePanelHeader, /UMySurrenderVotePanelWidget\s*:\s*public\s+UCommonUserWidget/);
  assert.match(dungeonPCSource, /AddPersistentWidget\(SurrenderVotePanelClass\)/);
  assert.doesNotMatch(dungeonPCSource, /PushToast\(SurrenderVotePanelClass\)/);
  assert.match(persistentOverlayBody, /CanvasSlot->SetZOrder\([1-9][0-9]{2,}\);/);
});

test("dialogue deactivates the HUD layer before hiding it and reactivates it after restoring visibility", () => {
  const managerHeader = readProjectFile("Source", "ProjectP", "Widget", "MyUIManagerSubsystem.h");
  const managerSource = readProjectFile("Source", "ProjectP", "Widget", "MyUIManagerSubsystem.cpp");
  const dungeonPCSource = readProjectFile("Source", "ProjectP", "Dungeon", "DungeonPC.cpp");
  const dialogueSource = readProjectFile("Source", "ProjectP", "Dungeon", "Dialogue", "MyDialogueWidget.cpp");

  assert.match(
    managerHeader,
    /public:[\s\S]*void\s+SetLayerActive\(FGameplayTag LayerTag, bool bActive\)/,
  );

  const setLayerActiveBody = getFunctionBody(
    managerSource,
    "void UMyUIManagerSubsystem::SetLayerActive(FGameplayTag LayerTag, bool bActive)",
  );
  const startDialogueBody = getFunctionBody(
    dungeonPCSource,
    "void ADungeonPC::ClientStartDialogue_Implementation(const TSoftObjectPtr<UMyDialogueDataAsset>& DialogueAsset)",
  );
  const restoreHUDBody = getFunctionBody(
    dialogueSource,
    "void UMyDialogueWidget::RestoreHUDLayer()",
  );

  assert.match(setLayerActiveBody, /Layer->GetActiveWidget\(\)/);
  assert.match(setLayerActiveBody, /ActiveWidget->DeactivateWidget\(\)/);
  assert.match(setLayerActiveBody, /ActiveWidget->ActivateWidget\(\)/);

  const deactivateIndex = startDialogueBody.indexOf("SetLayerActive(MyGameplayTags::UI_Layer_HUD, false)");
  const hideIndex = startDialogueBody.indexOf("SetLayerVisible(MyGameplayTags::UI_Layer_HUD, false)");
  const pushDialogueIndex = startDialogueBody.indexOf("PushDialogue(DialogueWidgetClass)");
  assert.notEqual(deactivateIndex, -1);
  assert.ok(deactivateIndex < hideIndex, "HUD should be deactivated before its layer is collapsed");
  assert.ok(hideIndex < pushDialogueIndex, "Dialogue should be pushed after the HUD layer is hidden");
  assert.match(
    startDialogueBody,
    /if \(!DialogueWidget\)[\s\S]*SetLayerVisible\(MyGameplayTags::UI_Layer_HUD, true\)[\s\S]*SetLayerActive\(MyGameplayTags::UI_Layer_HUD, true\)[\s\S]*return/,
  );

  const showIndex = restoreHUDBody.indexOf("SetLayerVisible(MyGameplayTags::UI_Layer_HUD, true)");
  const reactivateIndex = restoreHUDBody.indexOf("SetLayerActive(MyGameplayTags::UI_Layer_HUD, true)");
  assert.notEqual(reactivateIndex, -1);
  assert.ok(showIndex < reactivateIndex, "HUD should be visible before it is reactivated");
});
