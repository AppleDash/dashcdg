#ifndef _UTIL_H_INCLUDED
#define _UTIL_H_INCLUDED

#include <GL/glew.h>

#define ATOMIC_INT int
#define ATOMIC_INT_GET(I) (__atomic_load_n(&(I), __ATOMIC_RELAXED))
#define ATOMIC_INT_SET(I, V) __atomic_store_n(&(I), (V), __ATOMIC_RELAXED)

int read_file(const char *path, char **buf, unsigned long *size);
GLuint load_shader_program(const char *name);

#endif
