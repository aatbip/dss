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

### Creating a new `dss` string

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

### 





