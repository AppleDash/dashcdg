#ifndef _UTIL_H_INCLUDED
#define _UTIL_H_INCLUDED

#include <stdlib.h>
#include <GL/glew.h>

#define ATOMIC_INT int
#define ATOMIC_INT_GET(I) (__atomic_load_n(&(I), __ATOMIC_RELAXED))
#define ATOMIC_INT_SET(I, V) __atomic_store_n(&(I), (V), __ATOMIC_RELAXED)

#define UNUSED(X) (void)(X)
#define CHECK_MEM(X) if ((X) == NULL) { fprintf(stderr, "[%s] at line %d: failed to allocate memory\n", __FILE__, __LINE__); exit(1); }

int read_file(const char *path, char **buf, unsigned long *size);
GLuint load_shader_program(const char *name);
void backup_and_close_stdout_stderr(void);
void restore_stdout_stderr(void);

#endif
