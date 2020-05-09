#pragma once

#include <stddef.h>

template<typename T>
struct Span
{
  T* data = nullptr;
  int len = 0;

  Span() = default;
  Span(T* tab, int N) : data(tab), len(N) {}

  template<size_t N>
  Span(T (& tab)[N]) : data(tab), len(N) {}

  // construction from vector/string
  template<typename U, typename = decltype(((U*)0)->data())>
  Span(U const& s)
  {
    data = s.data();
    len = s.size();
  }

  T* begin() const { return data; }
  T* end() const { return data + len; }
  T& operator [] (int i) { return data[i]; }
  void operator += (int i) { data += i; len -= i; }
};

using String = Span<const char>;
String format(Span<char> buf, const char* fmt, ...);

