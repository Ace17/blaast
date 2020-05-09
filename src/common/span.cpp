#include "span.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

String format(Span<char> buf, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf.data, buf.len, fmt, args);
  va_end(args);
  return { buf.data, (int)strlen(buf.data) };
}

