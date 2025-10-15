#include "dss.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DSS_HDR(s) ((dss_hdr *)((char *)s - sizeof(dss_hdr)))

typedef struct __attribute__((__packed__)) {
  /*size of allocated memory in bytes for struct dss_hdr and buf */
  uint64_t size;
  /* Number of bytes occupied in buf. len is total bytes in buf + null term.
   * That's why creating empty dss string using dss_empty should result in
   * len=1.*/
  uint64_t len;
  /*Tracks the number of references created. */
  uint32_t ref_count;
  char buf[];
} dss_hdr;

/*It reallocates memory if required otherwise returns the same address*/
static dss_hdr *dss_expand(dss_hdr *hdr, size_t len) {

  /* DSS_NULLT is not included in calculating size_t needed because
   * hdr->len already includes null term*/
  size_t needed = hdr->len + len;
  size_t total = hdr->size - sizeof(dss_hdr);

  if (needed <= total)
    return hdr;

  /* Increase size by two times the length of t.
   * This approach is taken to minimize realloc call
   * everytime dss_concat is called.
   *
   * Note: should stop increasing size if it's already crossing
   * the limit. This logic should be added later.
   */
  size_t new_cap = total * 2;
  if (new_cap < needed)
    new_cap = needed * 2;

  hdr = realloc(hdr, sizeof(dss_hdr) + new_cap);
  if (!hdr) {
    perror("realloc failed");
    exit(1);
  }
  hdr->size = sizeof(dss_hdr) + new_cap;
  return hdr;
}

/* Appends byte and add null term at the end */
static inline dss_hdr *dss_append_bytes(dss_hdr *hdr, const void *t,
                                        size_t len) {
  /* In the below memcpy DSS_NULLT is subtracted because copy should
   * happen from the current position of null terminator */
  memcpy(hdr->buf + hdr->len - DSS_NULLT, t, len);
  hdr->len = hdr->len + len;
  hdr->buf[hdr->len] = '\0';
  return hdr;
}

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

  if (len == 0)
    return hdr->buf;

  /*Reallocate memory if required*/
  hdr = dss_expand(hdr, len);

  dss_append_bytes(hdr, t, len);

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
 *   for eventually calling dss_free() once done except if callee
 *   decides to use dss_concatcow for mutating the shared object, in which case,
 *   dss_concatcow will implicitly unshare the shared object and trasfer the
 * ownership back to the caller.
 * - You can pass the dss string directly (without dss_refshare) when
 *   the callee is only borrowing. In this case
 *   callee shouldn't call dss_free() and no ref_count is incremented.
 */

dss dss_refshare(dss s) {
  DSS_HDR(s)->ref_count++;
  return s;
}

/* dss_concatcow implements Copy-on-write if more than 1 references is detected.
 * If multiple references is not detected then proceeds with dss_concatb.
 * Always use dss_refshare(dss s) for the first parameter dss s. The callee is
 * not required to dss_free the shared reference object because dss_concatcow
 * will unshare the ownership from callee implicitly.
 */
dss dss_concatcow(dss s, const char *t) {
  size_t len = strlen(t);
  return dss_concatcowb(s, t, len);
}

dss dss_concatcowb(dss s, const char *t, size_t len) {
  dss_hdr *hdr = DSS_HDR(s);
  /*check if refcount > 1, if true then apply cow*/
  if (hdr->ref_count > 1) {
    /*deep copy the dss string*/
    dss dups = dss_dup(s);
    dss_hdr *dup_hdr = DSS_HDR(dups);

    if (len == 0)
      return dup_hdr->buf;

    /*Reallocate memory if required*/
    dup_hdr = dss_expand(dup_hdr, len);

    dss_append_bytes(dup_hdr, t, len);

    /*Decrease the reference to transfer ownership back to the caller*/
    hdr->ref_count--;
    return dup_hdr->buf;
  }
  return dss_concatb(s, t, len);
}

dss dss_grow(dss s, size_t len) {
  dss_hdr *hdr = DSS_HDR(s);
  if (len > hdr->len) {
    hdr = dss_expand(hdr, len);
    memset(hdr->buf + hdr->len, 0, len);
    hdr->len = len;
    hdr->buf[hdr->len] = '\0';
    return hdr->buf;
  }
  return s;
}
