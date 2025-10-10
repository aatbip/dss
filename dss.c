#include "dss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int size;
  int len;
  char buf[];
} dss_hdr;

dss dss_new(const char *s) {
  size_t len = strlen(s);
  return dss_newb(s, len);
}

dss dss_newb(const void *s, size_t len) {
  dss_hdr *hdr = (dss_hdr *)malloc(sizeof(dss_hdr) + len + DSS_NULLT);

  if (!hdr) {
    fprintf(stderr, "Not able to allocate memory.");
    exit(EXIT_FAILURE);
  }

  hdr->size = sizeof(dss_hdr) + len + DSS_NULLT;
  hdr->len = len + DSS_NULLT;

  memcpy(hdr->buf, (const char *)s, len);
  hdr->buf[hdr->len] = '\0';

  return hdr->buf;
}
