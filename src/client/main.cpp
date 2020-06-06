#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>

#include "app.h"
#include "atlas.h"
#include "display.h"
#include "file.h"
#include "picture.h"
#include "span.h"
#include "sprite.h"
#include "stats.h"
#include "steamgui.h"
#include "steamgui_impl.h"

std::vector<Sprite> g_Sprites;

extern std::vector<Vec2i> packRects(const std::vector<Vec2i>& sizes, Vec2i containerSize);
extern std::vector<Picture> decodeAni(Span<const uint8_t> in);
extern Picture decodePcx(Span<const uint8_t> data);
extern std::vector<uint8_t> loadFile(const char* path);

extern std::vector<TriangleBag> g_Bags;

namespace
{
static const std::vector<const char*> ANIMATIONS =
{
  "data/ani/kface.ani",
  "data/ani/powers.ani",
  "data/ani/triganim.ani",
  "data/ani/mflame.ani",
  "data/ani/walk.ani",
  "data/ani/bombs.ani",
  "data/ani/stand.ani",
  "data/ani/tiles0.ani",
  "data/ani/tiles1.ani",
  "data/ani/tiles2.ani",
  "data/ani/tiles3.ani",
  "data/ani/tiles4.ani",
  "data/ani/tiles5.ani",
  "data/ani/tiles6.ani",
  "data/ani/tiles7.ani",
  "data/ani/tiles8.ani",
  "data/ani/tiles9.ani",
  "data/ani/tiles10.ani",
  "data/ani/xbrick0.ani",
  "data/ani/xbrick1.ani",
  "data/ani/xbrick2.ani",
  "data/ani/xbrick3.ani",
  "data/ani/xbrick4.ani",
  "data/ani/xbrick5.ani",
  "data/ani/xbrick6.ani",
  "data/ani/xbrick7.ani",
  "data/ani/xbrick8.ani",
  "data/ani/xbrick9.ani",
  "data/ani/xbrick10.ani",
  "data/ani/xplode1.ani",
  "data/ani/xplode2.ani",
  "data/ani/xplode3.ani",
  "data/ani/xplode4.ani",
  "data/ani/xplode5.ani",
  "data/ani/xplode6.ani",
  "data/ani/xplode7.ani",
  "data/ani/xplode8.ani",
  "data/ani/xplode9.ani",
  "data/ani/xplode10.ani",
  "data/ani/xplode11.ani",
  "data/ani/xplode12.ani",
  "data/ani/xplode13.ani",
  "data/ani/xplode14.ani",
  "data/ani/xplode15.ani",
  "data/ani/xplode16.ani",
  "data/ani/xplode17.ani",
  "data/ani/corner0.ani",
  "data/ani/corner1.ani",
  "data/ani/corner2.ani",
  "data/ani/corner3.ani",
  "data/ani/corner4.ani",
  "data/ani/corner5.ani",
  "data/ani/corner6.ani",
  "data/ani/corner7.ani",
  "data/ani/kfont.ani",
  "data/ani/hurry.ani",
  "data/ani/kick.ani",
  "data/ani/duds.ani",
  "data/ani/bwalk1.ani",
  "data/ani/bwalk2.ani",
  "data/ani/bwalk3.ani",
  "data/ani/bwalk4.ani",
  "data/ani/pup1.ani",
  "data/ani/pup2.ani",
  "data/ani/pup3.ani",
  "data/ani/pup4.ani",
  "data/ani/punbomb1.ani",
  "data/ani/punbomb2.ani",
  "data/ani/punbomb3.ani",
  "data/ani/punbomb4.ani",
  "data/ani/extras.ani",
  "data/ani/conveyor.ani",
  "data/ani/shadow.ani",
  "data/ani/misc.ani",
  "data/ani/edit.ani",
  "data/ani/aliens1.ani",
  "data/ani/flame.ani",
};

std::vector<Stamp> g_Stamps;

Picture CreateAtlas(const std::vector<Picture>& allPics)
{
  AutoProfile ap("Atlas creation (ms)");

  static auto blitPicture = [] (Picture& dst, const Picture& src, Vec2i pos)
    {
      int srcPos = 0;
      int dstPos = pos.x + dst.size.x * pos.y;

      for(int y = 0; y < src.size.y; ++y)
      {
        memcpy(&dst.pixels[dstPos], &src.pixels[srcPos], src.size.x * sizeof(Pixel));

        dstPos += dst.size.x;
        srcPos += src.size.x;
      }
    };

  const auto atlasSize = Vec2i{ 2048, 4096 };

  std::vector<Vec2i> sizes;

  for(auto& pic : allPics)
    sizes.push_back(pic.size);

  const auto positions = packRects(sizes, atlasSize);

  Picture atlas;
  atlas.size = atlasSize;
  atlas.pixels.resize(atlas.size.x * atlas.size.y);

  g_Stamps.resize(allPics.size());

  for(auto& pic : allPics)
  {
    int idx = int(&pic - allPics.data());
    blitPicture(atlas, pic, positions[idx]);

    g_Stamps[idx].size.x = pic.size.x;
    g_Stamps[idx].size.y = pic.size.y;
    g_Stamps[idx].uv0.x = float(positions[idx].x) / atlasSize.x;
    g_Stamps[idx].uv0.y = float(positions[idx].y) / atlasSize.y;
    g_Stamps[idx].uv1.x = float((positions[idx] + pic.size).x) / atlasSize.x;
    g_Stamps[idx].uv1.y = float((positions[idx] + pic.size).y) / atlasSize.y;
    g_Stamps[idx].origin.x = pic.origin.x;
    g_Stamps[idx].origin.y = pic.origin.y;
  }

  if(0)
  {
    FILE* fp = fopen("atlas.data", "wb");
    fwrite(atlas.pixels.data(), 1, atlas.pixels.size() * sizeof(atlas.pixels[0]), fp);
    fclose(fp);
  }

  return atlas;
}

Picture loadAtlasFromAniFiles(std::vector<const char*> pathes)
{
  AutoProfile ap("Atlas load+creation (ms)");
  std::vector<Picture> allPics;

  for(auto path : pathes)
    for(auto& pic : decodeAni(loadFile(path)))
      allPics.push_back(std::move(pic));

  return CreateAtlas(allPics);
}

Picture loadRawTexture(const unsigned char* data, int width, int height)
{
  Picture pic;
  pic.size.x = width;
  pic.size.y = height;
  pic.pixels.resize(height * width);

  memcpy(pic.pixels.data(), data, pic.pixels.size() * 4);

  return pic;
}

void tesselateSprites(int atlasTexture)
{
  if(g_Sprites.empty())
    return;

  const float scale = 1.0;

  auto byIncreasingZOrder = [] (Sprite& a, Sprite& b)
    {
      if(a.colormode != b.colormode)
        return a.colormode < b.colormode;

      return a.zorder < b.zorder;
    };

  std::sort(g_Sprites.begin(), g_Sprites.end(), byIncreasingZOrder);

  TriangleBag* bag = nullptr;

  int currColormode = -1;
  int bagCount = 0;

  for(auto& s : g_Sprites)
  {
    if(s.colormode != currColormode)
    {
      g_Bags.push_back({});
      bag = &g_Bags.back();
      bag->texture = atlasTexture;
      bag->zorder = ceil(s.zorder);
      bag->colormode = s.colormode;

      currColormode = s.colormode;
      ++bagCount;
    }

    auto& stamp = g_Stamps[s.tilenum];
    bag->addQuad(s.pos - stamp.origin * scale, stamp.size * scale, s.color, stamp.uv0, stamp.uv1);
  }

  Stat("Sprites", g_Sprites.size());
  Stat("Bags for Sprites", bagCount);
}

Picture whitePicture()
{
  Picture r {};
  r.size = { 1, 1 };
  r.pixels.resize(1, { 0xff, 0xff, 0xff, 0xff });
  return r;
}
}

extern unsigned char font_256x256[];

void safeMain(Span<const String> args)
{
  display_init();

  SteamGuiImpl gui;

  gui.whiteTexture = display_createTexture(whitePicture());
  gui.fontTexture = display_createTexture(loadRawTexture(font_256x256, 256, 256));
  const auto atlasTexture = display_createTexture(loadAtlasFromAniFiles(ANIMATIONS));
  const auto bgTexture = display_createTexture(decodePcx(loadFile("data/res/field1.pcx")));

  AppInit(args);

  uint8_t keys[256] {};

  bool quit = false;

  while(!quit)
  {
    {
      SDL_Event event;

      while(SDL_PollEvent(&event))
      {
        if(event.type == SDL_QUIT)
          quit = true;

        if(event.type == SDL_KEYDOWN)
        {
          if(event.key.keysym.scancode == SDL_SCANCODE_F4 && !event.key.repeat)
          {
            static bool fs = false;
            fs = !fs;

            display_setFullscreen(fs);
          }
        }
      }

      const Uint8* state = SDL_GetKeyboardState(NULL);
      keys[Key::Left] = state[SDL_SCANCODE_LEFT];
      keys[Key::Right] = state[SDL_SCANCODE_RIGHT];
      keys[Key::Up] = state[SDL_SCANCODE_UP];
      keys[Key::Down] = state[SDL_SCANCODE_DOWN];
      keys[Key::Enter] = state[SDL_SCANCODE_RETURN];
      keys[Key::Escape] = state[SDL_SCANCODE_ESCAPE];
      keys[Key::Space] = state[SDL_SCANCODE_SPACE];
      keys[Key::LeftShift] = state[SDL_SCANCODE_LSHIFT];
      keys[Key::LeftAlt] = state[SDL_SCANCODE_LALT];
      keys[Key::RightShift] = state[SDL_SCANCODE_RSHIFT];
      keys[Key::RightAlt] = state[SDL_SCANCODE_RALT];
      keys[Key::F2] = state[SDL_SCANCODE_F2];
    }

    g_Sprites.clear();

    gui.beginFrame();

    {
      int mouseX, mouseY;
      int state = SDL_GetMouseState(&mouseX, &mouseY);
      gui.mousePos.x = mouseX;
      gui.mousePos.y = mouseY;
      gui.mouseButton = state & SDL_BUTTON(SDL_BUTTON_LEFT);
    }

    {
      AutoProfile aprof("App Tick Time (ms)");

      if(!AppTick(&gui, keys))
        quit = true;
    }

    {
      AutoProfile aprof("Tesselation (ms)");

      tesselateSprites(atlasTexture);

      {
        g_Bags.push_back({});
        auto& bag = g_Bags.back();
        bag.texture = bgTexture;
        bag.addQuad(Vec2f(0, 0), Vec2f(640, 480), Vec4f(1, 1, 1, 1));
        bag.zorder = -1;
      }

      for(auto& bag : gui.bags)
        g_Bags.push_back(std::move(bag));

      auto byZorder = [] (TriangleBag const& a, TriangleBag const& b)
        {
          return a.zorder < b.zorder;
        };

      std::sort(g_Bags.begin(), g_Bags.end(), byZorder);
    }

    display_refresh();
  }

  AppExit();

  display_cleanup();
}

