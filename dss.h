#ifndef __dss_h__
#define __dss_h__

#include <stdio.h>

/* Extra byte allocated for a null terminator (for C-string compatibility) */
#define DSS_NULLT 1

typedef char *dss;

dss dss_new(const char *);
dss dss_newb(const void *, size_t);
dss dss_concat(dss, const char *);
dss dss_concatb(dss, const void *, size_t);
dss dss_concatcow(dss, const char *);
dss dss_concatcowb(dss, const char *, size_t);
size_t dss_len(const dss);
dss dss_dup(const dss);
dss dss_empty(void);
void dss_free(dss);

dss dss_refshare(dss);

#endif
