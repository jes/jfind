/* String functions for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

/* concatenate the given strings in a buffer exactly large enough to contain
 * them, for example:
 *   strallocat(homedir, "/", filename, NULL);
 * might get you something like "/home/james/foo.c"
 */
char *strallocat(const char *s1, ...) {
    va_list argp;
    size_t neededbytes = strlen(s1);
    char *s;

    /* do a first pass to calculate how much space is needed */
    va_start(argp, s1);
    while((s = va_arg(argp, char *)))
        neededbytes += strlen(s);
    va_end(argp);

    char *buf = malloc(neededbytes + 1);

    /* do a second pass copying bytes */
    va_start(argp, s1);
    strcpy(buf, s1);
    while((s = va_arg(argp, char *)))
        strcat(buf, s);
    va_end(argp);

    return buf;
}
