#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>

struct InputFile
{
  InputFile(const char* filename)
    : fp(fopen(filename, "rb"))
  {
    if(!fp)
      throw std::runtime_error(std::string("Could not open '") + filename + "'");
  }

  ~InputFile() { fclose(fp); }

  int u8()
  {
    int b = 0;
    read(&b, 1);
    return b;
  }

  int u16()
  {
    int r = 0;
    read(&r, 2);
    return r;
  }

  int u32()
  {
    int r = 0;
    read(&r, 4);
    return r;
  }

  void read(void* ptr, size_t size)
  {
    if(fread(ptr, 1, size, fp) != size)
      throw std::runtime_error("File read error");
  }

  long tell() const { return ftell(fp); }

  void skip(size_t amount)
  {
    if(0 != fseek(fp, amount, SEEK_CUR))
      throw std::runtime_error("File seek error");
  }

private:
  FILE* const fp;
};

