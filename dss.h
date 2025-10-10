#ifndef __dss_h__
#define __dss_h__

#include <stdio.h>

struct dss_hdr;
typedef char *dss;

dss dss_new(const char *);
dss dss_newb(const void *, size_t);

#endif
