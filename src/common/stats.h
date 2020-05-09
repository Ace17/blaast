#pragma once

#include "span.h"

struct StatVal
{
  String name;
  float val;
};

void Stat(String name, float value);

int getStatCount();
StatVal getStat(int idx);

