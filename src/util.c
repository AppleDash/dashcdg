#include "util.h"
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>

static int stdoutCopy;
static int stderrCopy;

int read_file(const char *path, char **buf, unsigned long *size) {
    FILE *fp;

    fp = fopen(path, "r");

    if (!fp) {
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buf = (char *) malloc((*size) + 1);

    CHECK_MEM(*buf)

    if (fread(*buf, 1, *size, fp) != *size) {
        free(*buf);
        fclose(fp);
        return 0;
    }

    (*buf)[*size] = '\0';

    fclose(fp);

    return 1;
}

void backup_and_close_stdout_stderr(void) {
    stdoutCopy = dup(STDOUT_FILENO);
    stderrCopy = dup(STDERR_FILENO);

    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void restore_stdout_stderr(void) {
    dup2(stdoutCopy, STDOUT_FILENO);
    dup2(stderrCopy, STDERR_FILENO);

    close(stdoutCopy);
    close(stderrCopy);
}
