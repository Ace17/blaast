#pragma once

#include "span.h"

struct SteamGui
{
  virtual void text(String text) = 0;
  virtual bool button(String text) = 0;
  virtual void checkbox(String text, bool& value) = 0;
  virtual void slider(String text, float& value) = 0;
  virtual void icon() = 0;
};

