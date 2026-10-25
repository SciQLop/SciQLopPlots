#include <cstdio>
#include <cstring>

int clamp_index(int i, int n);

static int check(const char* name, int got, int want)
{
    if (got == want)
    {
        std::printf("PASS   : Calc::%s()\n", name);
        return 0;
    }
    std::printf("FAIL!  : Calc::%s() got %d, want %d\n", name, got, want);
    return 1;
}

int main(int argc, char** argv)
{
    const char* test = argc > 1 ? argv[1] : "";
    if (std::strcmp(test, "low") == 0)
        return check("low", clamp_index(-5, 10), 0);
    if (std::strcmp(test, "high") == 0)
        return check("high", clamp_index(12, 10), 9);
    if (std::strcmp(test, "edge") == 0)
        return check("edge", clamp_index(10, 10), 9);
    if (std::strcmp(test, "broken") == 0)
        return check("broken", clamp_index(4, 10), 5);
    return check("mid", clamp_index(4, 10), 4);
}
