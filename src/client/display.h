#pragma once

#include <stdint.h>

struct Picture;

void display_init();
void display_cleanup();
void display_refresh();
void display_setFullscreen(bool enable);
intptr_t display_createTexture(const Picture&);

///////////////////////////////////////////////////////////////////////////////

#include "span.h"
#include "stats.h"
#include <SDL.h>

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

