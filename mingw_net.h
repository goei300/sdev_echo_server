#pragma once

#ifdef WIN32

typedef int socklen_t;
int inet_pton(int af, const char *src, void *dst);

#endif // WIN32
