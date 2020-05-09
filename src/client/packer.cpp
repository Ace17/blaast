#include "picture.h"
#include <algorithm>
#include <stdexcept>

std::vector<Vec2i> packRects(const std::vector<Vec2i>& sizes, Vec2i containerSize)
{
  struct Area
  {
    Vec2i pos, size;

    bool fits(Vec2i subsize) const
    {
      return subsize.x <= size.x && subsize.y <= size.y;
    }
  };

  std::vector<Area> freeAreas;

  auto allocRect = [&] (Vec2i size) -> Vec2i
    {
      int freeIdx = -1;

      for(int i = 0; i < (int)freeAreas.size(); ++i)
      {
        if(freeAreas[i].fits(size))
        {
          freeIdx = i;
          break;
        }
      }

      if(freeIdx == -1)
        throw std::runtime_error("out of 2D space");

      auto area = freeAreas[freeIdx];
      freeAreas.erase(freeAreas.begin() + freeIdx);

      if(area.size.x > size.x)
      {
        // there's still room on the right
        Area right = area;
        right.pos.x += size.x;
        right.size.x -= size.x;
        right.size.y = size.y;
        freeAreas.insert(freeAreas.begin() + freeIdx, right);
      }

      if(area.size.y > size.y)
      {
        // there's still room below
        Area below = area;
        below.pos.y += size.y;
        below.size.y -= size.y;
        freeAreas.insert(freeAreas.begin() + freeIdx + 1, below);
      }

      return area.pos;
    };

  freeAreas.push_back(Area{
    { 0, 0 }, containerSize });

  const int N = (int)sizes.size();

  struct CandidateRect
  {
    int origIdx;
    Vec2i size;
  };

  std::vector<CandidateRect> sortedRects;
  int k = 0;

  for(auto& size : sizes)
    sortedRects.push_back({ k++, size });

  auto byDecreasingHeight = [] (CandidateRect& a, CandidateRect& b)
    {
      if(a.size.y != b.size.y)
        return a.size.y > b.size.y;

      if(a.size.x != b.size.x)
        return a.size.x > b.size.x;

      return a.origIdx < b.origIdx;
    };

  std::sort(sortedRects.begin(), sortedRects.end(), byDecreasingHeight);

  std::vector<Vec2i> r(N);

  for(auto& candidate : sortedRects)
    r[candidate.origIdx] = allocRect(candidate.size + Vec2i{ 2, 2 }) + Vec2i{ 1, 1 };

  return r;
}

