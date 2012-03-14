#include "Agent.h"
#include <assert.h>
#include <stdlib.h>

wchar_t *heapMbsToWcs(const char *text)
{
    size_t len = mbstowcs(NULL, text, 0);
    if (len == (size_t)-1)
        return NULL;
    wchar_t *ret = new wchar_t[len + 1];
    size_t len2 = mbstowcs(ret, text, len + 1);
    assert(len == len2);
    return ret;
}

int main(int argc, char *argv[])
{
    assert(argc == 5);
    Agent agent(heapMbsToWcs(argv[1]),
                heapMbsToWcs(argv[2]),
                atoi(argv[3]),
                atoi(argv[4]));
    agent.run();
}
