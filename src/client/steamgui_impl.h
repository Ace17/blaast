#include "steamgui.h"
#include "trianglebag.h"
#include "vec.h"
#include <vector>

inline
bool isPointInRect(Vec2f point, Vec2f rectPos, Vec2f rectSize)
{
  if(point.x < rectPos.x || point.x >= rectPos.x + rectSize.x)
    return false;

  if(point.y < rectPos.y || point.y >= rectPos.y + rectSize.y)
    return false;

  return true;
}

static const Vec4f COLOR_FG = Vec4f(0.2, 0.2, 0.8);
static const Vec4f COLOR_FG_HOVER = Vec4f(0.4, 0.4, 0.8);

struct SteamGuiImpl : SteamGui
{
  // ---------------------------------------------------------------------------
  // client-side interface

  void text(String text) override
  {
    addText(cursorPos, text);
    cursorPos.y += 16;
  }

  bool button(String text) override
  {
    auto buttonSize = Vec2f(200.0f, 40.0f);

    auto buttonPos = cursorPos;
    auto buttonEnd = cursorPos + buttonSize - Vec2f(2, 2);
    auto mouseInside = isPointInRect(mousePos, buttonPos, buttonSize);
    auto clickInside = mouseInside && mouseButton;

    {
      TriangleBag& bag = getBag(whiteTexture);
      Vec4f color = Vec4f(0, 0, 0);
      bag.addQuad(cursorPos, buttonSize, color);
    }

    if(clickInside)
    {
      buttonPos += Vec2f(2, 2);
      buttonEnd += Vec2f(2, 2);
    }

    {
      TriangleBag& bag = getBag(whiteTexture);

      const auto color = mouseInside ? COLOR_FG_HOVER : COLOR_FG;
      bag.addQuad(buttonPos, buttonEnd - buttonPos, color);
    }

    addText(buttonPos + Vec2f(8, 8), text);

    cursorPos.y += buttonSize.y * 1.1;
    return clickInside && mouseButtonJustClicked;
  }

  void checkbox(String text, bool& value) override
  {
    auto buttonPos = cursorPos;
    auto buttonSize = Vec2f(24.0f, 24.0f);
    auto mouseInside = isPointInRect(mousePos, buttonPos, buttonSize);
    auto clickInside = mouseInside && mouseButtonJustClicked;

    if(clickInside)
      value = !value;

    const auto color = mouseInside ? COLOR_FG_HOVER : COLOR_FG;

    {
      TriangleBag& bag = getBag(whiteTexture);

      Vec2f s = Vec2f(1, 1);
      bag.addQuad(buttonPos, buttonSize, Vec4f(1, 0, 0, 0));
      bag.addQuad(cursorPos + s, buttonSize - s * 2, color);
    }

    if(value)
    {
      addText(cursorPos + Vec2f(2, 2), "\xEB");
    }

    addText(cursorPos + Vec2f(buttonSize.x + 4, 4), text);

    cursorPos.y += 32;
  }

  void slider(String text, float& value) override
  {
    auto buttonSize = Vec2f(200.0f, 32.0f);
    auto margin = 8.0f;

    auto buttonPos = cursorPos;
    auto buttonEnd = cursorPos + buttonSize;
    auto mouseInside = isPointInRect(mousePos, buttonPos, buttonSize);
    auto clickInside = mouseInside && mouseButton;

    if(clickInside)
    {
      value = (mousePos.x - (buttonPos.x + margin)) / (buttonSize.x - 2 * margin);

      if(value <= 0)
        value = 0;

      if(value >= 1)
        value = 1;
    }

    {
      TriangleBag& bag = getBag(whiteTexture);

      const auto color = mouseInside ? COLOR_FG_HOVER : COLOR_FG;
      bag.addQuad(buttonPos, buttonEnd - buttonPos, color);
    }

    {
      TriangleBag& bag = getBag(whiteTexture);
      Vec4f color = Vec4f(0.2, 0.2, 0.2);
      bag.addQuad(cursorPos + Vec2f(value * (buttonSize.x - margin * 2) + margin - 8, 2), Vec2f(16, buttonSize.y - 2 * 2), color);
    }

    addText(buttonPos + Vec2f(buttonSize.x, 8), text);

    {
      char buffer[32];
      sprintf(buffer, "%.3f", value);
      const int n = strlen(buffer);
      addText(buttonPos + Vec2f(buttonSize.x / 2 - 16 * n / 2, 8), { buffer, n });
    }

    cursorPos.y += buttonSize.y * 1.1;
  }

  void icon() override
  {
    auto& bag = getBag(whiteTexture);

    bag.addQuad(cursorPos, Vec2f(50, 50), Vec4f(1, 1, 1, 1));

    cursorPos.y += 50 * 1.1;
  }

  // ---------------------------------------------------------------------------
  // backend interface

  void beginFrame()
  {
    cursorPos = Vec2f(650, 10);
    bags.clear();
    getBag(whiteTexture);
    getBag(fontTexture);

    mouseButtonJustClicked = mouseButton && !mouseButtonPrev;
    mouseButtonPrev = mouseButton;
  }

  std::vector<TriangleBag> bags;

  TriangleBag & getBag(int texture)
  {
    for(auto& bag : bags)
      if(bag.texture == texture)
        return bag;

    bags.push_back({});
    bags.back().texture = texture;
    return bags.back();
  };

  int whiteTexture {};
  int fontTexture {};
  Vec2f mousePos {};
  int mouseButtonJustClicked {};
  int mouseButton {};
  int mouseButtonPrev {};

private:
  // ---------------------------------------------------------------------------
  Vec2f cursorPos {};

  void addText(Vec2f pos, String text)
  {
    const auto FontSize = Vec2f(16, 16);

    TriangleBag& bag = getBag(fontTexture);

    for(auto c : text)
    {
      const int row = c / 16;
      const int col = c % 16;
      const Vec2f uv0 = Vec2f(1.0 / 16.0 * col, 1.0 / 16.0 * row);
      const Vec2f uv1 = uv0 + Vec2f(1.0 / 16.0, 1.0 / 16.0);

      bag.addQuad(pos, FontSize, Vec4f(1, 1, 1, 1), uv0, uv1);

      pos.x += FontSize.x;
    }
  }
};

