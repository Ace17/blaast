#pragma once

struct Vec2i
{
  int x, y;
  Vec2i operator + (Vec2i other) const { return { x + other.x, y + other.y }; }
  Vec2i operator * (int n) const { return { x* n, y* n }; }
  bool operator == (Vec2i other) const { return x == other.x && y == other.y; }
};

struct Vec2f
{
  float x, y;

  Vec2f() = default;
  Vec2f(float x_, float y_) : x(x_), y(y_) {}

  void operator += (Vec2f other) { *this = *this + other; }
  void operator *= (float f) { *this = *this * f; }
  void operator -= (Vec2f other) { *this = *this - other; }
  float operator * (Vec2f other) const { return x * other.x + y * other.y; }

  Vec2f operator + (Vec2f other) const { return Vec2f { x + other.x, y + other.y }; }
  Vec2f operator - (Vec2f other) const { return Vec2f { x - other.x, y - other.y }; }
  Vec2f operator * (float f) const { return Vec2f { x* f, y* f }; }

  bool operator == (Vec2f other) const { return x == other.x && y == other.y; };

  static Vec2f zero() { return Vec2f(0, 0); }
};

struct Vec4f
{
  Vec4f() = default;
  Vec4f(float x_, float y_, float z_, float w_ = 1) : x(x_), y(y_), z(z_), w(w_) {}
  union
  {
    struct { float x {}, y {}, z{}, w{};
    };
    struct { float r, g, b, a;
    };
  };
};

