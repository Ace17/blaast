#include "stats.h"
#include <map>
#include <string>
#include <vector>

namespace
{
std::vector<StatVal> g_Values;
std::map<std::string, int> g_Map;
}

void Stat(String name, float value)
{
  std::string key(name.data, name.data + name.len);
  auto i = g_Map.find(key);

  if(i == g_Map.end())
  {
    g_Map[key] = (int)g_Values.size();
    g_Values.push_back({ name, value });
    i = g_Map.find(key);
  }

  g_Values[i->second] = { name, value };
}

int getStatCount()
{
  return (int)g_Values.size();
}

StatVal getStat(int idx)
{
  return g_Values.at(idx);
}

