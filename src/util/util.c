/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abtu.h"


/* \c ABTU_get_indent_str() returns a white-space string with the length of
 * \c indent.  The caller should free the memory returned. */
char *ABTU_get_indent_str(int indent)
{
    char *space;
    space = (char *)ABTU_malloc(sizeof(char) * (indent + 1));
    if (indent > 0) memset(space, ' ', indent);
    space[indent] = '\0';
    return space;
}
