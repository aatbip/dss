#include "dss.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  /*size of allocated memory in bytes for struct dss_hdr and buf */
  size_t size;
  /*number of bytes occupied in buf */
  size_t len;
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
  printf("len: %ld\n size: %ld\n", hdr->len, hdr->size);

  return hdr->buf;
}

dss dss_concat(dss s, const char *t) {
  size_t len = strlen(t);
  return dss_concatb(s, t, len);
}

dss dss_concatb(dss s, const void *t, size_t len) {
  dss_hdr *hdr = (dss_hdr *)((char *)s - sizeof(dss_hdr));

  /* Remaining bytes in buffer */
  size_t rem_buf = hdr->size - sizeof(dss_hdr) - hdr->len;

  /* Increase size by two times the length of t.
   * This approach is taken to minimize realloc call
   * everytime dss_concat is called.
   *
   * Note: should stop increasing size if it's already crossing
   * the limit. This logic should be added later.
   */
  if (rem_buf == 0 || rem_buf < len) {
    hdr->size = hdr->size + len * 2;
    hdr = (dss_hdr *)realloc(hdr, hdr->size);
  }
  /* In the below memcpy DSS_NULLT is subtracted because copy should
   * happen from the current position of null terminator */
  memcpy(hdr->buf + hdr->len - DSS_NULLT, t, len);
  hdr->len = hdr->len + len;
  hdr->buf[hdr->len + DSS_NULLT] = '\0';

  return hdr->buf;
}
