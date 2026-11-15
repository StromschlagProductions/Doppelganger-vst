#include "TestSupport.h"
int main (int argc, char** argv)
{
    CHECK (argc == 2); auto x = voice (48000, 16);
    FILE* f = std::fopen (argv[1], "wb"); CHECK (f != nullptr);
    CHECK (std::fwrite (x.data(), sizeof (float), x.size(), f) == x.size()); CHECK (std::fclose (f) == 0);
}
