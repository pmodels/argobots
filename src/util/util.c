/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abtu.h"
#include <math.h>


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

/* \c ABTU_get_int_len() returns the string length of the integer \c num. */
int ABTU_get_int_len(size_t num)
{
    return (num == 0) ? 1 : (int)(log10(num) + 1);
}
