const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const projectRoot = path.resolve(__dirname, "..");
const playerCharacterHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Player", "PlayerCharacterBase.h");
const playerCharacterSourcePath = path.join(projectRoot, "Source", "ProjectP", "Player", "PlayerCharacterBase.cpp");
const lobbyGameStateHeaderPath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyGSB.h");
const lobbyGameStateSourcePath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyGSB.cpp");
const lobbyGameModeSourcePath = path.join(projectRoot, "Source", "ProjectP", "Lobby", "CPP_LobbyGMB.cpp");
const lobbyPartyPanelSourcePath = path.join(projectRoot, "Source", "ProjectP", "Widget", "CPP_LobbyPartyPanel.cpp");

test("player character replicates selected character id and maps it to temporary lobby materials", () => {
  const header = fs.readFileSync(playerCharacterHeaderPath, "utf8");
  const source = fs.readFileSync(playerCharacterSourcePath, "utf8");

  assert.match(header, /void\s+SetSelectedCharacterId\(int32\s+NewSelectedCharacterId\);/);
  assert.match(header, /void\s+PreviewSelectedCharacterMaterial\(int32\s+PreviewSelectedCharacterId\);/);
  assert.match(header, /int32\s+GetSelectedCharacterId\(\)\s+const;/);
  assert.match(header, /UPROPERTY\(ReplicatedUsing\s*=\s*OnRep_SelectedCharacterId[\s\S]*?int32\s+SelectedCharacterId\s*=\s*-1;/);
  assert.match(source, /DOREPLIFETIME\(APlayerCharacterBase,\s*SelectedCharacterId\);/);
  assert.match(source, /MI_White_Halo_V10_R\.MI_White_Halo_V10_R/);
  assert.match(source, /MI_White_Halo_V10_G\.MI_White_Halo_V10_G/);
  assert.match(source, /MI_White_Halo_V10\.MI_White_Halo_V10/);
  assert.match(source, /CharacterMaterialSlotName\s*=\s*TEXT\("Halo"\)/);
  assert.match(source, /void\s+APlayerCharacterBase::PreviewSelectedCharacterMaterial\(int32\s+PreviewSelectedCharacterId\)[\s\S]*?GetMesh\(\)->SetMaterial\(MaterialIndex,\s*SelectedMaterial\);/);
});

test("lobby character selection previews and applies selected character id to the possessed character", () => {
  const gsbHeader = fs.readFileSync(lobbyGameStateHeaderPath, "utf8");
  const gsbSource = fs.readFileSync(lobbyGameStateSourcePath, "utf8");
  const gmbSource = fs.readFileSync(lobbyGameModeSourcePath, "utf8");
  const panelSource = fs.readFileSync(lobbyPartyPanelSourcePath, "utf8");

  assert.match(gsbHeader, /bool\s+GetPartyMemberSelectedCharacter\(APlayerState\*\s+MemberPlayerState,\s*int32&\s+OutSelectedCharacterId\)\s+const;/);
  assert.match(gsbSource, /bool\s+ACPP_LobbyGSB::GetPartyMemberSelectedCharacter\(APlayerState\*\s+MemberPlayerState,\s*int32&\s+OutSelectedCharacterId\)\s+const/);
  assert.match(gmbSource, /#include\s+"\.\.\/Player\/PlayerCharacterBase\.h"/);
  assert.match(panelSource, /#include\s+"\.\.\/Player\/PlayerCharacterBase\.h"/);
  assert.match(panelSource, /PlayerCharacter->PreviewSelectedCharacterMaterial\(SelectedCharacterId\);/);
  assert.match(gmbSource, /bool\s+ACPP_LobbyGMB::SelectPartyCharacter\(APlayerController\*\s+RequestingController,\s*int32\s+SelectedCharacterId\)[\s\S]*?PlayerCharacter->SetSelectedCharacterId\(SelectedCharacterId\);/);
  assert.match(gmbSource, /PlayerCharacter->SetSelectedCharacterId\(SelectedCharacterId\);/);
});
