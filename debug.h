#ifndef DEBUG_H
#define DEBUG_H

#define debug(...) do { fprintf(stderr, "DEBUG: %s: ", __func__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0);

#endif /* DEBUG_H */
