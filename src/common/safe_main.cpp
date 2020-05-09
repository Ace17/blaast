#include "safe_main.h"
#include <cstring> // strlen
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
  try
  {
    std::vector<String> args;

    for(int i = 0; i < argc; ++i)
    {
      String s;
      s.data = argv[i];
      s.len = strlen(argv[i]);
      args.push_back(s);
    }

    safeMain(args);
    return 0;
  }
  catch(const std::exception& e)
  {
    fprintf(stderr, "Fatal: %s\n", e.what());
    return 1;
  }
}

