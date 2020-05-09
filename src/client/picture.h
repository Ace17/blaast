#pragma once

#include "vec.h"
#include <stdint.h>
#include <vector>

struct Pixel
{
  uint8_t r, g, b, a;
};

struct Picture
{
  Vec2i size;
  Vec2i origin;
  std::vector<Pixel> pixels;
};

