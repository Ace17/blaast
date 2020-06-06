#include <stdint.h>
#include <stdio.h>
#include <string.h> // memcmp

#include <stdexcept>
#include <vector>

#include "file.h"
#include "picture.h"
#include "span.h"

namespace
{
#define enforce(condition) \
  enforceFunc(condition, # condition, __FILE__, __LINE__)

void enforceFunc(bool condition, const char* caption, const char* file, int line)
{
  if(condition)
    return; // ok

  std::string msg;
  msg += file;
  msg += "(";
  msg += std::to_string(line);
  msg += "): ";
  msg += "Condition violation: ";
  msg += caption;
  throw std::runtime_error(msg);
}

int readLE(Span<const uint8_t>& pointer, int n)
{
  int result = 0;

  for(int i = 0; i < n; ++i)
  {
    result |= pointer[0] << (8 * i);
    pointer += 1;
  }

  return result;
}

template<size_t N>
constexpr uint32_t FOURCC(const char (& tab)[N])
{
  static_assert(N == 5);
  uint32_t r = 0;

  for(int i = 0; i < 4; ++i)
    r = (r << 8) | (uint8_t)tab[3 - i];

  return r;
}

struct Chunk
{
  Span<const uint8_t> data;
  uint32_t fourcc;
};

Chunk readChunk(Span<const uint8_t>& in)
{
  Chunk r {};
  r.fourcc = readLE(in, 4);
  int len = readLE(in, 4);
  int id = readLE(in, 2);
  (void)id;

  r.data = in;
  r.data.len = len;

  in += len;

  return r;
}

Pixel convertPixelToRgba(uint16_t input, uint16_t colorKey)
{
  Pixel pel;
  pel.r = ((input >> 10) & 0b11111) << 3;
  pel.g = ((input >> 5) & 0b11111) << 3;
  pel.b = ((input >> 0) & 0b11111) << 3;
  pel.a = input == colorKey ? 0 : 0xff;
  return pel;
}

void decompressRleToRgba(const uint8_t* input, Pixel* output, uint32_t inputSize, uint16_t colorKey)
{
  const auto inputEnd = input + inputSize;

  while(input < inputEnd)
  {
    const auto symbol = *input++;
    const auto len = 1 + (symbol & 0x7f);

    if(symbol & 0x80)
    {
      uint16_t pel = input[0] + input[1] * 256;
      input += 2;

      auto rgba = convertPixelToRgba(pel, colorKey);

      for(int i = 0; i < len; ++i)
        *output++ = rgba;
    }
    else
    {
      for(int i = 0; i < len; ++i)
      {
        uint16_t pel = input[0] + input[1] * 256;
        input += 2;

        auto rgba = convertPixelToRgba(pel, colorKey);
        *output++ = rgba;
      }
    }
  }
}

Picture autocrop(Picture srcPic)
{
  int minX = srcPic.size.x;
  int maxX = 0;
  int minY = srcPic.size.y;
  int maxY = 0;

  for(int y = 0; y < srcPic.size.y; ++y)
  {
    for(int x = 0; x < srcPic.size.x; ++x)
    {
      if(srcPic.pixels[x + y * srcPic.size.x].a)
      {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
      }
    }
  }

  Picture dstPic;
  dstPic.size.x = maxX - minX;
  dstPic.size.y = maxY - minY;
  dstPic.pixels.resize(dstPic.size.x * dstPic.size.y);
  dstPic.origin = srcPic.origin;
  dstPic.origin.x -= minX;
  dstPic.origin.y -= minY;

  {
    int srcPos = minX + srcPic.size.x * minY;
    int dstPos = 0;

    for(int y = 0; y < dstPic.size.y; ++y)
    {
      memcpy(&dstPic.pixels[dstPos], &srcPic.pixels[srcPos], dstPic.size.x * sizeof(Pixel));

      dstPos += dstPic.size.x;
      srcPos += srcPic.size.x;
    }
  }

  if(0)
  {
    int origSize = srcPic.size.x * srcPic.size.y;
    int croppedSize = dstPic.size.x * dstPic.size.y;
    int reduction = origSize - croppedSize;
    fprintf(stderr, "gain: %.2f%% (%+d)\n", float(reduction) * 100.0f / float(origSize), reduction);
  }

  return dstPic;
}

Picture parse_CIMG(Chunk chunk)
{
  enforce(chunk.data.len >= 32);

  const auto type = readLE(chunk.data, 2);
  const auto reserved3 = readLE(chunk.data, 2);
  const auto additional_size = readLE(chunk.data, 4);

  enforce(additional_size >= 24);
  enforce(additional_size <= chunk.data.len);

  const auto reserved = readLE(chunk.data, 4);
  const auto width = readLE(chunk.data, 2);
  const auto height = readLE(chunk.data, 2);
  const auto hotspot_x = readLE(chunk.data, 2);
  const auto hotspot_y = readLE(chunk.data, 2);
  const auto colorKey = readLE(chunk.data, 2);
  const auto reserved2 = readLE(chunk.data, 2);

  (void)reserved;
  (void)reserved2;
  (void)reserved3;

  if(type != 0x04)// 16bit-per-pixel RLE ?
  {
    printf("warning: image type (%d) is not 0x4\n", type);
    throw std::runtime_error("invalid image type");
  }

  const int flags = readLE(chunk.data, 4);

  if(flags == 0xC0011)
  {
    const auto compressed_size = readLE(chunk.data, 4) - 12;
    const auto uncompressed_size = readLE(chunk.data, 4);
    (void)uncompressed_size;

    Span<const uint8_t> compressedData(chunk.data.data, compressed_size);
    chunk.data += compressed_size;

    Picture r;
    r.size = { width, height };
    r.pixels.resize(width * height + 128);
    r.origin = { hotspot_x, hotspot_y };
    decompressRleToRgba(compressedData.data, r.pixels.data(), compressed_size, colorKey);
    r.pixels.resize(width * height);

    if(0)
      r.pixels[hotspot_x + hotspot_y * width] = { 255, 0, 0, 255 };

    return r;
  }
  else
  {
    if(0)
    {
      int pixelCount = width * height;
      fprintf(stderr, "------------------\n");
      fprintf(stderr, "%dx%d (%d)\n", width, height, pixelCount);
      fprintf(stderr, "blockSize=%d\n", chunk.data.len);

      FILE* fp = fopen("block.raw", "wb");
      fwrite(chunk.data.data, 1, chunk.data.len, fp);
      fclose(fp);
    }

    Picture r;
    r.pixels.resize(64 * 64);
    r.size = { 64, 64 };

    for(auto& p : r.pixels)
      p = { 1, 1, 0, 1 };

    return r;
  }
}

Picture parse_FRAM(Chunk aniChunk)
{
  bool hasData = false;
  String name {};

  Picture pic {};

  while(aniChunk.data.len > 0)
  {
    Chunk chunk = readChunk(aniChunk.data);
    switch(chunk.fourcc)
    {
    case FOURCC("FNAM"):
      {
        enforce(chunk.data.len > 0);
        enforce(chunk.data.len < 16);

        enforce(!name.data);
        name = { (const char*)chunk.data.data, chunk.data.len };

        break;
      }
    case FOURCC("CIMG"):
      {
        enforce(!hasData);
        pic = parse_CIMG(chunk);
        hasData = true;
        break;
      }
    case FOURCC("HEAD"):
      {
        break;
      }
    default:
      {
        if(0)
          fprintf(stderr, "skipping: '%.*s'\n", 4, (char*)&chunk.fourcc);
      }
    }
  }

  if(!name[0])
    printf("warning: frame has no name, skipped\n");

  if(!hasData)
    printf("warning: frame has no data, skipped\n");

  if(0)
    printf("%.*s: %dx%d\n", name.len, name.data, pic.size.x, pic.size.y);

  return autocrop(pic);
}

void rleDecompressLine(Span<const uint8_t>& s, uint8_t* dst, int stride)
{
  int i = 0;

  while(i < stride)
  {
    int runLen = 1;
    auto value = s[0];
    s += 1;

    if(value >= 0xc0)
    {
      runLen = value & 0x3f;
      value = s[0];
      s += 1;
    }

    while(i < stride && runLen--)
    {
      *dst++ = value;
      ++i;
    }
  }
}
}

std::vector<uint8_t> loadFile(const char* path)
{
  FILE* fp = fopen(path, "rb");

  if(!fp)
  {
    std::string isoPath = "iso/";
    isoPath += path;
    fp = fopen(isoPath.c_str(), "rb");
  }

  if(!fp)
    throw std::runtime_error("Can't open file '" + std::string(path) + "' for reading");

  fseek(fp, 0, SEEK_END);
  auto size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  std::vector<uint8_t> r(size);
  fread(r.data(), 1, r.size(), fp);
  fclose(fp);

  return r;
}

std::vector<Picture> decodeAni(Span<const uint8_t> in)
{
  if(in.len < 10 || memcmp(in.data, "CHFILEANI ", 10))
    throw std::runtime_error("Not an ANI file");

  in += 10;

  auto file_length = readLE(in, 4);
  auto reserved1 = readLE(in, 2);
  enforce(reserved1 == 0);

  enforce(file_length <= in.len);
  in.len = file_length;

  std::vector<Picture> pictures;

  while(in.len > 0)
  {
    Chunk chunk = readChunk(in);
    switch(chunk.fourcc)
    {
    case FOURCC("FRAM"):
      pictures.push_back(parse_FRAM(chunk));
      break;
    default:

      if(0)
        fprintf(stderr, "skipping: '%.*s'\n", 4, (char*)&chunk.fourcc);

      break;
    }
  }

  return pictures;
}

Picture decodePcx(Span<const uint8_t> s)
{
  const int Manufacturer = readLE(s, 1);
  const int Version = readLE(s, 1);
  const int IsCompressed = readLE(s, 1);
  const int BitsPerPixel = readLE(s, 1);
  const int Xmin = readLE(s, 2);
  const int Ymin = readLE(s, 2);
  const int width_minus1 = readLE(s, 2);
  const int height_minus1 = readLE(s, 2);
  const int Hres = readLE(s, 2);
  const int Vres = readLE(s, 2);
  (void)Hres, (void)Vres;

  s += 16 * 3; // skip colormap

  const int Reserved = readLE(s, 1);
  const int PlaneCount = readLE(s, 1);
  const int StrideInBytes = readLE(s, 2);

  enforce(Xmin == 0);
  enforce(Ymin == 0);
  enforce(PlaneCount == 1);
  enforce(Reserved == 0);
  enforce(IsCompressed == 1);
  enforce(Version == 5);
  enforce(Manufacturer == 0xA);
  enforce(BitsPerPixel == 8);

  s += 60;

  auto const width = width_minus1 + 1;
  auto const height = height_minus1 + 1;
  std::vector<uint8_t> decompressed(width * height);

  for(int row = 0; row < height; ++row)
    rleDecompressLine(s, decompressed.data() + row * width, StrideInBytes);

  enforce(s[0] == 0x0C); // palette marker
  s += 1;

  enforce(s.len == 768);

  Pixel palette[256];

  for(auto& color : palette)
  {
    color.r = s[0];
    color.g = s[1];
    color.b = s[2];
    color.a = 0xff;
    s += 3;
  }

  Picture p {};
  p.size = { width, height };
  p.pixels.resize(width * height);

  for(int row = 0; row < height; ++row)
    for(int col = 0; col < width; ++col)
      p.pixels[col + row * width] = palette[decompressed[col + row * width]];

  return p;
}

