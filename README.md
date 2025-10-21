# DSS - Dynamic String Structure

### DSS is a generic dynamic byte buffer inspired by antirez/sds. DSS is binary safe byte container that internally implements safe reference sharing without any need of writing wrappers manually. DSS strings are mutable but there are APIs implemented based on copy-on-write that can be utilized while mutating shared references. Internally handled reference sharing and copy-on-write APIs makes DSS unique from antires/sds.

# Design

DSS contains `dss_hdr` struct internally that contains members to store meta data about the buffer. This struct also contains a 
flexible array member `buf` which is the actual buffer where DSS string is stored. DSS string is always null terminated making it usable with the
standard C string manipulating functions.

```
---------------------------------------------------------------------------
| Header: size, len, and ref_count | Binary safe buffer: *buf | Null term |
---------------------------------------------------------------------------
                                   |
                                   `-> *dss returned to the user
```
When a DSS string is created, it returns pointer to the null terminated byte buffer `buf`. Meta data  are `size` and `len` of type `uint64_t`, and `ref_count` of type
`uint32_t`. The member `size` book keeps the total size in bytes of the `dss_hdr` struct including the flexible array member `buf` and the null term, `len` 
book keeps the number of bytes occupied in the `buf`, and `ref_count` tracks the number of shared references. The flexible array member `buf` is not allocated 
separately but lives with the same sequence of memory allocation along with `dss_hdr`, where actual bytes are stored which is always concluded with the null terminator.
This technique reduce the need for multiple and separate memory allocation overhead for the struct and the string buffer as seen in other C string libraries.

# API functions Documentation

### Creating a `dss` string

There are two API functions available for creating a `dss` string.

```c
dss dss_new(const char *t);
dss dss_newb(const void *t, size_t len);
```

Both of the above two functions allocate memory in the heap and return `dss` string which is of type `char*`.

```c
dss s = dss_new("hello world!");
printf("DSS string: %s\n", s);
dss_free(s);

Output> hello world!
```

`dss_newb` where b stands for binary is used to create strings from binary data. Unlike `dss_new`, `dss_newb` doesn't assume the string to conclude with a null term
thus requires to pass an additional parameter `len` which is the total length in bytes of the binary. 

```c
 char t[4];
 t[0] = 'A';
 t[1] = '\0';
 t[2] = '\0';
 t[3] = 'B';
 dss s = dss_newb(t, 4);
 for (int i = 0; i < 4; i++) {
   printf("%02X ", s[i]);
 }
 printf("\n");
 printf("len: %zu\n", dss_len(s));

 Output> 41 00 00 42
         len: 5
```

In the example above, we can see that `dss_newb` is capable of storing binary. We also saw the output of `dss_len` to be `5` which is the total bytes stored 
in the buffer including the null terminator.

### Appending the `dss` buffer

There are two sets of functions to append bytes in the `dss` buffer. The first set of functions mutates the `dss` buffer. It requires the callee to return the latest
valid pointer to the recently mutated `dss` buffer back to the caller to prevent from dangling pointer issues.


```c
 dss dss_concat(dss s, const char *t);
 dss dss_concatb(dss s, const void *t, size_t len);
```
In the example below, we pass parameter of type `char**` to the `proc` function so that we can return the updated valid pointer back to the `main`. At first, ownership
is shared to `proc` (callee), then `proc` passes it back to `main` (caller). It is necessary because the appending function reallocate memory internally which shifts 
the memory position making the original memory block invalid, resulting in dangling pointer issue. 

```c
void proc(char **s) {
  char *new_str = dss_concat(*s, "world!!!");
  *s = new_str;
}

int main(void) {
  char *str = dss_new("hello");
  proc(&str);
  dss_free(str);
  return 0;
}
```
The below set of functions don't mutate the buffer but instead follow the copy-on-write (COW) semantics. 

```c
dss dss_concatcow(dss s, const char *t);
dss dss_concatcowb(dss s, const char *t, size_t len);
```
A reference sharing counter is implemented implicitly in `dss`. To be able to use the COW based concatenation functions, always pass the `dss` parameter to it by
incrementing the `ref_count` counter which tracks the number of references shared. 

```c
void proc(dss s) {
  char *new_str = dss_concatcow(s, "world!!");
  dss_free(new_str);
  /*No need to call dss_free(s)*/
  // dss_free(s)  
}

int main(void) {
  char *str = dss_new("hello");
  proc(dss_refshare(str));
  dss_free(str);
  return 0;
}
```
In the example above, we pass the parameter to the `proc` function using `dss_refshare`. `dss_refshare` increments the `ref_count` counter to track the references
shared and then returns the `dss` buffer. Always share the `dss` buffer through `dss_refshare` if reference counting has to be conducted. `dss_concatcow` internally
decreases the `ref_count` having no need to call `dss_free(s)` explicitly in the `proc` function. 

Reference counting shouldn't be used with appending function APIs that mutates the original buffer. That is because the mutating function APIs might have to reallocate
memory which may shift the buffer address in the memory and free the original buffer. This will lead to undefined behaviour and double free issue. Below is short
summary on where to use the `dss` internal reference counting and where not to use: 
 
### USE reference tracking:
-  while using COW related function APIs for appending the `dss` buffer such as `dss_concatcow` and `dss_concatcowb`. They only perform copy-on-write if there are
multiple references.
- for shared references in multiple processes. We won't be sure which process would end at last so reference tracking can be helpful to
only free the object when the last running process ends.

### DO NOT USE reference tracking:
- when working with mutating function APIs such as `dss_concat` and `dss_concatb`.
- when the process where object is shared mutates the object. In this case, the original reference should be updated with the valid
latest reference.



















