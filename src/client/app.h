#pragma once

#include "span.h"
#include <cstdint>

struct SteamGui;

void AppInit(Span<const String> args);
void AppExit();
bool AppTick(SteamGui*, Span<const uint8_t> keys);

enum Key
{
  Left,
  Right,
  Up,
  Down,
  Space,
  LeftShift,
  LeftAlt,
  RightShift,
  RightAlt,
  Escape,
  Enter,
  F2,
};

