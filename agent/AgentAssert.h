#ifndef AGENTASSERT_H
#define AGENTASSERT_H

#define ASSERT(x) do { if (!(x)) assertFail(__FILE__, __LINE__, #x); } while(0)

void assertFail(const char *file, int line, const char *cond);

#endif // AGENTASSERT_H
