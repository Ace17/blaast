#include "display.h"

#include <cassert>
#include <stdexcept>
#include <string>

#include "glad/glad.h"
#include <SDL.h>

#include "picture.h"
#include "trianglebag.h"

#ifdef NDEBUG
#define SAFE_GL(a) a
#else
#define SAFE_GL(a) \
  do { a; ensureGl(# a, __LINE__); } while (0)
#endif

namespace
{
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

void drawScreen(SDL_Window* window, int vbo, int transfoLoc, int colormodeLoc, std::vector<TriangleBag> const& bags)
{
  AutoProfile aprof("Draw Time (ms)");
  glClearColor(0.10, 0.10, 0.20, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  int width, height;
  SDL_GetWindowSize(window, &width, &height);

  glViewport(0, 0, width, height);

  const float scale = std::min(width / 640.0f, height / 480.0f);

  const float orthoMat[4][4] =
  {
    { 2.0f * scale / width, 0.0f, 0.0f, 0.0f },
    { 0.0f, -2.0f * scale / height, 0.0f, 0.0f },
    { 0.0f, 0.0f, -1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f, 1.0f },
  };

  SAFE_GL(glUniformMatrix4fv(transfoLoc, 1, GL_FALSE, &orthoMat[0][0]));

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

int transfoLoc, colormodeLoc;
SDL_GLContext context;
SDL_Window* window;
GLuint vbo;
}

std::vector<TriangleBag> g_Bags;

void display_init()
{
  SDL_Init(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

  window = SDL_CreateWindow("Blaast", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  if(!window)
    throw std::runtime_error("SDL can't create window");

  context = SDL_GL_CreateContext(window);

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

  glGenBuffers(1, &vbo);

  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  transfoLoc = glGetUniformLocation(program, "transfo");
  assert(transfoLoc >= 0);

  colormodeLoc = glGetUniformLocation(program, "colormode");
  assert(colormodeLoc >= 0);
}

void display_cleanup()
{
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void display_refresh()
{
  drawScreen(window, vbo, transfoLoc, colormodeLoc, g_Bags);
  SDL_GL_SwapWindow(window);
  g_Bags.clear();
}

intptr_t display_createTexture(const Picture& pic)
{
  GLuint r;
  glGenTextures(1, &r);
  glBindTexture(GL_TEXTURE_2D, r);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic.size.x, pic.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic.pixels.data());

  return r;
}

void display_setFullscreen(bool enable)
{
  auto flags = enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
  SDL_SetWindowFullscreen(window, flags);
}

