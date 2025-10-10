#ifndef __dss_h__
#define __dss_h__

#include <stdio.h>

/* Extra byte allocated for a null terminator (for C-string compatibility) */
#define DSS_NULLT 1

struct dss_hdr;
typedef char *dss;

dss dss_new(const char *);
dss dss_newb(const void *, size_t);
dss dss_concat(dss, const char *);
dss dss_concatb(dss, const void *, size_t);

#endif
