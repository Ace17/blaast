#pragma once

#include "common/vec.h"

struct Sprite
{
  int tilenum;
  float zorder = 0;
  bool colormode = false;
  Vec2f pos;
  Vec4f color = { 1, 1, 1, 1 };
};

