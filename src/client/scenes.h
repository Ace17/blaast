#pragma once

struct SteamGui;

struct SceneFuncStruct
{
  SceneFuncStruct(* stateFunc)(SteamGui*);
};

SceneFuncStruct sceneIngame(SteamGui* ui);
SceneFuncStruct scenePaused(SteamGui* ui);

