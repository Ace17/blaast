#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "glad/glad.h"
#include <SDL.h>

#include "app.h"
#include "atlas.h"
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

namespace
{
std::vector<TriangleBag> g_Bags;

static const std::vector<const char*> ANIMATIONS =
{
  "iso/data/ani/kface.ani",
  "iso/data/ani/powers.ani",
  "iso/data/ani/triganim.ani",
  "iso/data/ani/mflame.ani",
  "iso/data/ani/walk.ani",
  "iso/data/ani/bombs.ani",
  "iso/data/ani/stand.ani",
  "iso/data/ani/tiles0.ani",
  "iso/data/ani/tiles1.ani",
  "iso/data/ani/tiles2.ani",
  "iso/data/ani/tiles3.ani",
  "iso/data/ani/tiles4.ani",
  "iso/data/ani/tiles5.ani",
  "iso/data/ani/tiles6.ani",
  "iso/data/ani/tiles7.ani",
  "iso/data/ani/tiles8.ani",
  "iso/data/ani/tiles9.ani",
  "iso/data/ani/tiles10.ani",
  "iso/data/ani/xbrick0.ani",
  "iso/data/ani/xbrick1.ani",
  "iso/data/ani/xbrick2.ani",
  "iso/data/ani/xbrick3.ani",
  "iso/data/ani/xbrick4.ani",
  "iso/data/ani/xbrick5.ani",
  "iso/data/ani/xbrick6.ani",
  "iso/data/ani/xbrick7.ani",
  "iso/data/ani/xbrick8.ani",
  "iso/data/ani/xbrick9.ani",
  "iso/data/ani/xbrick10.ani",
  "iso/data/ani/xplode1.ani",
  "iso/data/ani/xplode2.ani",
  "iso/data/ani/xplode3.ani",
  "iso/data/ani/xplode4.ani",
  "iso/data/ani/xplode5.ani",
  "iso/data/ani/xplode6.ani",
  "iso/data/ani/xplode7.ani",
  "iso/data/ani/xplode8.ani",
  "iso/data/ani/xplode9.ani",
  "iso/data/ani/xplode10.ani",
  "iso/data/ani/xplode11.ani",
  "iso/data/ani/xplode12.ani",
  "iso/data/ani/xplode13.ani",
  "iso/data/ani/xplode14.ani",
  "iso/data/ani/xplode15.ani",
  "iso/data/ani/xplode16.ani",
  "iso/data/ani/xplode17.ani",
  "iso/data/ani/corner0.ani",
  "iso/data/ani/corner1.ani",
  "iso/data/ani/corner2.ani",
  "iso/data/ani/corner3.ani",
  "iso/data/ani/corner4.ani",
  "iso/data/ani/corner5.ani",
  "iso/data/ani/corner6.ani",
  "iso/data/ani/corner7.ani",
  "iso/data/ani/kfont.ani",
  "iso/data/ani/hurry.ani",
  "iso/data/ani/kick.ani",
  "iso/data/ani/duds.ani",
  "iso/data/ani/bwalk1.ani",
  "iso/data/ani/bwalk2.ani",
  "iso/data/ani/bwalk3.ani",
  "iso/data/ani/bwalk4.ani",
  "iso/data/ani/pup1.ani",
  "iso/data/ani/pup2.ani",
  "iso/data/ani/pup3.ani",
  "iso/data/ani/pup4.ani",
  "iso/data/ani/punbomb1.ani",
  "iso/data/ani/punbomb2.ani",
  "iso/data/ani/punbomb3.ani",
  "iso/data/ani/punbomb4.ani",
  "iso/data/ani/extras.ani",
  "iso/data/ani/conveyor.ani",
  "iso/data/ani/shadow.ani",
  "iso/data/ani/misc.ani",
  "iso/data/ani/edit.ani",
  "iso/data/ani/aliens1.ani",
  "iso/data/ani/flame.ani",
};

struct AutoProfile
{
  AutoProfile(String statName) : m_name(statName), m_timeStart(SDL_GetPerformanceCounter())
  {
  }

  ~AutoProfile()
  {
    const uint64_t timeStop = SDL_GetPerformanceCounter();
    auto delta = (timeStop - m_timeStart) * 1000.0 / SDL_GetPerformanceFrequency();
    Stat(m_name, delta);
  }

  const String m_name;
  const uint64_t m_timeStart;
};

#ifdef NDEBUG
#define SAFE_GL(a) a
#else
#define SAFE_GL(a) \
  do { a; ensureGl(# a, __LINE__); } while (0)
#endif

void ensureGl(char const* expr, int line)
{
  auto const errorCode = glGetError();

  if(errorCode == GL_NO_ERROR)
    return;

  std::string ss;
  ss += "OpenGL error\n";
  ss += "Expr: " + std::string(expr) + "\n";
  ss += "Line: " + std::to_string(line) + "\n";
  ss += "Code: " + std::to_string(errorCode) + "\n";
  throw std::runtime_error(ss);
}

enum { attrib_position, attrib_uv, attrib_color };

int compileShader(int type, const char* code)
{
  auto vs = glCreateShader(type);

  glShaderSource(vs, 1, &code, nullptr);
  glCompileShader(vs);

  GLint status;
  glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
  assert(status);
  return vs;
}

int createShader()
{
  static const char* vertex_shader = R"(#version 130
uniform mat4 transfo;
in vec2 pos;
in vec2 uv;
in vec4 col;
out vec4 v_color;
out vec2 v_uv;
void main()
{
  v_color = col;
  v_uv = uv;
  gl_Position = transfo * vec4(pos, 0, 1);
}
)";

  static const char* fragment_shader = R"(#version 130
uniform sampler2D tex;
uniform float colormode;
in vec4 v_color;
in vec2 v_uv;
out vec4 o_color;
void main()
{
  if(colormode > 0.5)
  {
    vec4 c = texture(tex, v_uv);
    if(c.g > c.r && c.g > c.b)
    {
      float val = c.g;
      c.rgb = val * v_color.rgb;
    }
    o_color = c;
  }
  else
    o_color = v_color * texture(tex, v_uv);
}
)";

  auto vs = compileShader(GL_VERTEX_SHADER, vertex_shader);
  auto fs = compileShader(GL_FRAGMENT_SHADER, fragment_shader);

  auto program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);

  SAFE_GL(glBindAttribLocation(program, attrib_position, "pos"));
  SAFE_GL(glBindAttribLocation(program, attrib_uv, "uv"));
  SAFE_GL(glBindAttribLocation(program, attrib_color, "col"));
  glLinkProgram(program);

  return program;
}

intptr_t createTextureFromPixels(const Picture& pic)
{
  // Create a OpenGL texture identifier
  GLuint r;
  glGenTextures(1, &r);
  glBindTexture(GL_TEXTURE_2D, r);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Upload pixels into texture
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic.size.x, pic.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic.pixels.data());

  return r;
}

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

intptr_t loadTextureFromFile(const char* filename, int width, int height)
{
  Picture pic;
  pic.size.x = width;
  pic.size.y = height;
  pic.pixels.resize(height * width);

  InputFile fp(filename);
  fp.read(pic.pixels.data(), pic.pixels.size() * 4);

  return createTextureFromPixels(pic);
}

void drawScreen(SDL_Window* window, int vbo, int transfoLoc, int colormodeLoc, std::vector<TriangleBag> const& bags)
{
  AutoProfile aprof("Draw Time (ms)");
  glClearColor(0.10, 0.10, 0.20, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  int width, height;
  SDL_GetWindowSize(window, &width, &height);

  glViewport(0, 0, width, height);

  const float orthoMat[4][4] =
  {
    { 2.0f / width, 0.0f, 0.0f, 0.0f },
    { 0.0f, -2.0f / height, 0.0f, 0.0f },
    { 0.0f, 0.0f, -1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f, 1.0f },
  };

  glUniformMatrix4fv(transfoLoc, 1, GL_FALSE, &orthoMat[0][0]);

  SAFE_GL(glEnableVertexAttribArray(attrib_position));
  SAFE_GL(glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
  SAFE_GL(glEnableVertexAttribArray(attrib_uv));
  SAFE_GL(glVertexAttribPointer(attrib_uv, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
  SAFE_GL(glEnableVertexAttribArray(attrib_color));
  SAFE_GL(glVertexAttribPointer(attrib_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, col)));

  for(auto& bag : bags)
  {
    glBindTexture(GL_TEXTURE_2D, bag.texture);
    glUniform1f(colormodeLoc, bag.colormode ? 1 : 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bag.triangles[0]) * bag.triangles.size(), bag.triangles.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, bag.triangles.size());
  }

  Stat("Draw Calls", bags.size());
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
}

void safeMain(Span<const String> args)
{
  SDL_Init(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  auto window = SDL_CreateWindow("Blaast", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  if(!window)
    throw std::runtime_error("SDL can't create window");

  auto context = SDL_GL_CreateContext(window);

  if(!context)
    throw std::runtime_error("SDL can't create OpenGL context");

  int ret = gladLoadGL();

  if(!ret)
    throw std::runtime_error("GLAD can't load OpenGL");

  SDL_GL_SetSwapInterval(1);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  auto const program = createShader();

  SAFE_GL(glUseProgram(program));

  GLuint vbo;
  glGenBuffers(1, &vbo);

  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  SteamGuiImpl gui;

  gui.whiteTexture = loadTextureFromFile("data/white.rgba", 2, 2);
  gui.fontTexture = loadTextureFromFile("data/font_256x256.rgba", 256, 256);
  const auto atlasTexture = createTextureFromPixels(loadAtlasFromAniFiles(ANIMATIONS));
  const auto bgTexture = createTextureFromPixels(decodePcx(loadFile("iso/data/res/field1.pcx")));

  const int transfoLoc = glGetUniformLocation(program, "transfo");
  assert(transfoLoc >= 0);

  const int colormodeLoc = glGetUniformLocation(program, "colormode");
  assert(colormodeLoc >= 0);

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

    g_Bags.clear();
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

    drawScreen(window, vbo, transfoLoc, colormodeLoc, g_Bags);
    SDL_GL_SwapWindow(window);
  }

  AppExit();

  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

