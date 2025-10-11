#include "dss.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  /*size of allocated memory in bytes for struct dss_hdr and buf */
  uint32_t size;
  /* Number of bytes occupied in buf. len is total bytes in buf + null term.
   * That's why creating empty dss string using dss_empty should result in
   * len=1.*/
  uint32_t len;
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
    return NULL;
  }

  hdr->size = sizeof(dss_hdr) + len + DSS_NULLT;
  /* hdr->len totals with DSS_NULLT because hdr->len means number of bytes
   * currently occupied in the buf which includes the null terminator as well.
   */
  hdr->len = len + DSS_NULLT;

  memcpy(hdr->buf, (const char *)s, len);
  hdr->buf[hdr->len] = '\0';

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
  hdr->buf[hdr->len] = '\0';

  return hdr->buf;
}

size_t dss_len(const dss s) {
  dss_hdr *hdr = (dss_hdr *)((char *)s - sizeof(dss_hdr));
  return hdr->len;
}

dss dss_dup(const dss s) {
  dss_hdr *hdr = (dss_hdr *)((char *)s - sizeof(dss_hdr));
  dss_hdr *dup_hdr = (dss_hdr *)malloc(hdr->size);
  if (!dup_hdr) {
    fprintf(stderr, "Not able to allocate memory.");
    return NULL;
  }
  memcpy(dup_hdr, hdr, hdr->size);
  return dup_hdr->buf;
}

/*Create an empty dss string i.e. nothing goes in the buffer but
 * a null term. Because of the presence of the null term, dss_len should
 * return 1.*/
dss dss_empty() {
  ;
  return dss_newb("", 0);
}

void dss_free(dss s) {
  dss_hdr *hdr = (dss_hdr *)((char *)s - sizeof(dss_hdr));
  free(hdr);
}
