#include "strlcpy.h"

#include <string.h>

char *
strlcpy(char *dest, const char *src, size_t n)
{
	strncpy(dest, src, n);
	if (strlen(src) >= n)
		dest[n] = '\0';
	return dest;
}
