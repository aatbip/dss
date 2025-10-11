#include "dss.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DSS_HDR(s) ((dss_hdr *)((char *)s - sizeof(dss_hdr)))

typedef struct {
  /*size of allocated memory in bytes for struct dss_hdr and buf */
  uint32_t size;
  /* Number of bytes occupied in buf. len is total bytes in buf + null term.
   * That's why creating empty dss string using dss_empty should result in
   * len=1.*/
  uint32_t len;
  /*Tracks the number of references created. */
  uint32_t ref_count;
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
  hdr->ref_count = 1;

  return hdr->buf;
}

dss dss_concat(dss s, const char *t) {
  size_t len = strlen(t);
  return dss_concatb(s, t, len);
}

dss dss_concatb(dss s, const void *t, size_t len) {
  dss_hdr *hdr = DSS_HDR(s);

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
  dss_hdr *hdr = DSS_HDR(s);
  return hdr->len;
}

dss dss_dup(const dss s) {
  dss_hdr *hdr = DSS_HDR(s);
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
  dss_hdr *hdr = DSS_HDR(s);
  /* free only if ref_count equals to 0. */
  if (--hdr->ref_count == 0) {
    free(hdr);
  }
}

/* Always use dss_refshare when passing a dss string to another function
 * that will share ownership of it. This ensures dss_hdr->ref_count is
 * incremented correctly for reference tracking.
 *
 * Note:
 * - dss_refshare should be used only when transferring shared ownership.
 * - The callee (the receiver of the shared reference) is responsible
 *   for eventually calling dss_free() once done.
 * - You can pass the dss string directly (without dss_refshare) when
 *   the callee is only borrowing or fully taking ownership. In this case
 *   callee shouldn't call dss_free().
 */

dss dss_refshare(dss s) {
  DSS_HDR(s)->ref_count++;
  printf("cur ref: %d\n", DSS_HDR(s)->ref_count);
  return s;
}
