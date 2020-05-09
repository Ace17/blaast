#pragma once

#include "vec.h"
#include <cmath>
#include <vector>

struct Vertex
{
  Vec2f pos;
  Vec2f uv;
  uint32_t col;
};

struct TriangleBag
{
  int texture = 0;
  int zorder = 0; // greater means above
  bool colormode = false; // replace green with vertex color?
  std::vector<Vertex> triangles;

  void addQuad(Vec2f pos, Vec2f size, Vec4f rgba, Vec2f uv0 = Vec2f(0, 0), Vec2f uv1 = Vec2f(1, 1))
  {
    const auto color = mkColor(rgba);
    triangles.push_back({ pos + Vec2f(0, 0), Vec2f(uv0.x, uv0.y), color });
    triangles.push_back({ pos + Vec2f(size.x, size.y), Vec2f(uv1.x, uv1.y), color });
    triangles.push_back({ pos + Vec2f(size.x, 0), Vec2f(uv1.x, uv0.y), color });

    triangles.push_back({ pos + Vec2f(0, 0), Vec2f(uv0.x, uv0.y), color });
    triangles.push_back({ pos + Vec2f(0, size.y), Vec2f(uv0.x, uv1.y), color });
    triangles.push_back({ pos + Vec2f(size.x, size.y), Vec2f(uv1.x, uv1.y), color });
  }

  void addCircle(Vec2f center, float radius, Vec4f rgba)
  {
    const auto color = mkColor(rgba);
    const auto uv = Vec2f(0, 0);
    const auto uv1 = Vec2f(1, 1);

    const auto TAU = 6.28318530718;
    const int N = 12;

    for(int i = 0; i <= N; ++i)
    {
      const auto a = (i + 0) * TAU / N;
      const auto b = (i + 1) * TAU / N;
      const auto A = Vec2f(cos(a), sin(a)) * radius;
      const auto B = Vec2f(cos(b), sin(b)) * radius;

      triangles.push_back({ center, uv, color });
      triangles.push_back({ center + B, uv1, color });
      triangles.push_back({ center + A, uv1, color });
    }
  }

private:
  static uint32_t mkColor(Vec4f rgba)
  {
    uint32_t color = 0;
    color <<= 8;
    color |= uint8_t(rgba.a * 255);
    color <<= 8;
    color |= uint8_t(rgba.b * 255);
    color <<= 8;
    color |= uint8_t(rgba.g * 255);
    color <<= 8;
    color |= uint8_t(rgba.r * 255);
    return color;
  }
};

