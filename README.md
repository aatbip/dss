# DSS - Dynamic String Structure

DSS is a generic dynamic byte buffer inspired by antirez/sds. DSS is binary safe byte container that internally implements safe reference sharing without 
any need of writing wrappers manually. DSS strings are mutable but there are APIs implemented based on copy-on-write that can be utilized while mutating shared 
references. Internally handled reference sharing and copy-on-write APIs make DSS unique from SDS.

# Design

DSS declares `dss_hdr` struct that contains members to store meta data about the buffer. This struct also contains a 
flexible array member `buf` which is the actual buffer where DSS string is stored. DSS string is always null terminated making it usable with the
standard C string manipulating functions.

```text
---------------------------------------------------------------------------
| Header: size, len, and ref_count | Binary safe buffer: *buf | Null term |
---------------------------------------------------------------------------
                                   |
                                   `-> *dss returned to the user
```
When a DSS string is created, it returns pointer to the null terminated byte buffer `buf`. Meta data  are `size` and `len` of type `uint64_t`, and `ref_count` of type
`uint32_t`. The member `size` book keeps the total size in bytes allocated by the `dss_hdr` struct including the flexible array member `buf` and the null term, `len` 
book keeps the number of bytes occupied in the `buf`, and `ref_count` tracks the number of shared references. The flexible array member `buf` is not allocated 
separately but lives with the same sequence of memory along with `dss_hdr`. `buf` points to the memory where the null terminated bytes are stored. This technique 
reduce the need for multiple and separate memory allocation overhead for the struct and the string buffer as seen in other C string libraries.

# API functions Documentation

## Creating a `dss` string

There are two API functions available for creating a `dss` string.

```c
dss dss_new(const char *t);
dss dss_newb(const void *t, size_t len);
```

Both of the above two functions allocate memory in the heap, initialize it with the string byte and return the pointer to the memory `dss` which is of type `char*`.

```c
dss s = dss_new("hello world!");
printf("DSS string: %s\n", s);
dss_free(s);

Output> hello world!
```

`dss_newb` where b stands for binary is used to create strings from binary data. Unlike `dss_new`, `dss_newb` doesn't assume the string to conclude with a null term
thus requires to pass an additional parameter `len` which is the total length in byte of the binary. 

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

In the example above, we can see that `dss_newb` is capable of storing binary. Notice the output of `dss_len` to be `5` which is the total bytes stored 
in the buffer including the null terminator.

## Concatenating the `dss` buffer

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
 
#### USE reference tracking:
-  while using COW related function APIs for appending the `dss` buffer such as `dss_concatcow` and `dss_concatcowb`. They only perform copy-on-write if there are
multiple references.
- for shared references in multiple processes. We won't be sure which process would end at last so reference tracking can be helpful to
only free the object when the last running process ends. Remember to free the object from all the places where references are shared.

#### DO NOT USE reference tracking:
- when working with mutating function APIs such as `dss_concat` and `dss_concatb`.
- when the process where object is shared mutates the object. In this case, the original reference should be updated with the valid
latest reference.

```c
dss dss_grow(dss, size_t len);
```

`dss_grow` expands the `dss` buffer to at least the specified length. It will do nothing and returns the original buffer if the requested length is 
less than or equal to the length of the buffer. Otherwise, it will expand the buffer to the requested length `len` padding zero to the newly allocated
regions and ensures null terminator at the last byte. The API is used to guarantee that the buffer is initialized up to the specified length.

```c
char *s = dss_new("hello");
s = dss_grow(s, 7);
s[5] = '!';
printf("%s\n", s);
dss_free(s);

Output> hello!
```
Subscripting assignment is safe in the above example since `dss_grow` expands the buffer up to the given length. 

## Destroying the `dss` buffer
```c
void dss_free(dss s);
```
`dss_free` checks for the `ref_count` count. If `ref_count` is equal to 0 then it gets the pointer to `dss` header and frees it. If `ref_count` is greater than 0 then
it only decreases the `ref_count` and waits to free until only a single reference of the object exist. 

## String length

```c
size_t dss_len(const dss s);
```
`dss_len` returns the length of the `dss` buffer along with the null terminator. Output in the example below is 6 because of the extra null terminator emphasizing 
that `dss` always book keeps length and size information by adding the `DSS_NULLT` whose value is 1.

```c
char *s = dss_new("hello");
printf("len: %zu\n", dss_len(s));
dss_free(s);

Output> len: 6
```

`dss_len` has O(1) complexity because `dss` internally book keeps the length count.

## Creating an empty `dss` string

```c
dss dss_empty(void);
```
Creates an empty string by allocating a 1-byte buffer initialized to `0x00` `DSS_NULLT`. 

```c
char *s = dss_empty();
printf("len: %zu\n", dss_len(s));
dss_free(s);

Output> len: 2
```
In the above example, `dss_len` output on an empty `dss` string is 1 because `dss_len` gives the output by adding the `DSS_NULLT`.

## Cloning the `dss` buffer

```c
dss dss_dup(const dss s);
```
It copies the `dss` string into a new buffer and returns the pointer to the new buffer. It also resets the `ref_count` counter to 1 for the newly created
buffer.

```c
void proc(dss s) {
  char *s2 = dss_dup(s);
  dss_free(s);
  dss_free(s2);
}

int main(void) {
  char *s1 = dss_new("hello");
  proc(dss_refshare(s1));
  dss_free(s1);
  return 0;
}
```
In the example above, `s2` points to a newly allocated copy of the buffer referenced by `s1`, with its own `ref_count` reset to 1.

## Formatting strings 

```c
dss dss_catprintf(dss s, dss (concat_func*)(dss s, const char *t), const char *fmt, ...);
```

`dss_catprintf` can format string using format specifier and then concatenate the formatted string to the original string buffer.

```c
char *s = dss_new("The sum is: ");
char *temp = s;
int a = 14, b = 21;
s = dss_catprintf(dss_refshare(s), dss_concatcow, "%d", a + b);
printf("%s\n", s);
dss_free(s);
dss_free(temp);

Output> The sum is: 35  
```

This function takes 4 parameters as input. First parameter is the `dss` string itself to which the formatted string should be concatenated, second parameter is the 
function pointer to the concatenation function, third parameter is the format string where format specifiers can be added, then finally arguments for the format
specifiers if any. In the second parameter, pointer to either of `dss_concat` or `dss_concatcow` can be passed, which depends upon if we want to mutate the original
string or not as discussed in their sections above.

`dss_catprintf` can be used to create `dss` string directly from the format specifier.

```c
char *name = "world";
char *s = dss_catprintf(dss_empty(), dss_concat, "hello, %s", name);
printf("%s\n", s);
dss_free(s);

Output> hello world
```

It can also be used to convert numbers numbers into `dss` string.

```c
int num = 10021277;
dss nums = dss_catprintf(dss_empty(), dss_concat, "%d", num);
printf("%s\n", nums);
dss_free(nums);

Output> 10021277                                                                                                               
        len: 9  
```

## Trimming the `dss` string

```c
dss dss_trim(dss s, int start, int end);
```
This can be used to trim the `dss` string from `start` to `end`. This will shrink the memory to fit the trimmed string then return it.

```c
dss s1 = dss_new("hello world");
s1 = dss_trim(s1, 0, 5);
printf("s1: %s\n", s1);
dss_free(s1);
dss s2 = dss_new("hey you");
s2 = dss_trim(s2, 0, -5);
printf("s2: %s\n", s2);

Output> s1: hello                                                                                                                                          
        s2: hey  
```

# Error handling

The `dss` APIs return that returns `dss` buffer can also return `NULL` for memory allocation related errors which can be checked for handling errors.

# Performance benchmark

Multiple tests were performed to evaluate the performance and memory behavior of DSS. Experiments were designed to measure both throughput (concatenation performance) 
and memory efficiency under two distinct workloads:

1. Repeated concatenation of small binary chunks
2. Concatenation of a few large memory buffers

All experiments were executed on the same environment under identical conditions. The system detail is as follows:

**OS:** Ubuntu 22.04.5 LTS \
**Kernel:** Linux kernel 6.8.0-85-generic \
**CPU:** Intel Core i7-10750H CPU (6 core, 12 threads) \
**Memory:** 16 GB DDR4 \
**Compiler:** GCC 11.4.0 with -O2 optimization

Memory usage was profiled using Valgrind-3.18.1, and timing measurements were  obtained using a monotonic high-resolution clock (`clock_gettime(CLOCK_MONOTONIC)`), 
providing nanosecond precision and recorded in milliseconds for reporting.

## Experiment 1 — Repeated Concatenation of Small Binary Data

In this test, 5 bytes of binary data were repeatedly concatenated into a dss buffer 1 billion times, resulting in a final buffer size of approximately 5 GB.
This test stresses allocation overhead and reallocation frequency with extremely high operation counts on small payloads.

```text
| Metric              | Value (ms / Bytes) |
| ------------------- | ------------------ |
| Time taken (ms)     | 6845.9522          |
| Peak memory (Bytes) | 5368713160         |
```

Below is the Valgrind Massif heap memory profile graph, representing the evolution of heap usage during execution:

```text
    GB
5.000^                                   #################################### 
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                  :::::::::::::::::#                                    
     |                  :                #                                    
     |                  :                #                                    
     |                  :                #                                    
     |                  :                #                                    
     |         @@@@@@@@@:                #                                    
     |         @        :                #                                    
     |         @        :                #                                    
     |    :::::@        :                #                                    
     |  :::    @        :                #                                    
   0 +----------------------------------------------------------------------->GB
     0                                                                   10.00

Number of snapshots: 36
Detailed snapshots: [9, 19, 29, 32 (peak)]
```

The graph shows an exponential memory growth pattern. This reflects the exponential buffer growth strategy in DSS with each concatenation triggering 
a reallocation to larger capacity blocks.



### Experiment 2 — Concatenation of Large Memory Blocks

In this test, a 1 GB buffer was pre-allocated, initialized, and concatenated into a dss buffer five times, producing a final combined size of 5 GB. 
This workload emphasizes throughput and peak allocation efficiency for fewer, larger operations.

```text
| Metric              | Value (ms / Bytes) |
| ------------------- | ------------------ |
| Time taken (ms)     | 2493.3176          |
| Peak memory (Bytes) | 9663684496         |

```


Below graph shows Valgrind Massif heap memory profile.

```text
    GB
9.000^                                   ################################     
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                                   #                                    
     |                    :::::::::::::::#                                    
     |                    :              #                                    
     |                    :              #                                    
     |                    :              #                                    
     |                    :              #                                    
     |            :::::::::              #                                    
     |            :       :              #                                    
     |            :       :              #                                    
     |            :       :              #                                    
     |    :::::::::       :              #                               :::: 
     |    :       :       :              #                               :    
   0 +----------------------------------------------------------------------->GB
     0                                                                   18.00

Number of snapshots: 11
Detailed snapshots: [6 (peak)]
```

The graph shows that the peak memory usage exceeds 9 GB, almost double the final 5 GB string size.

### Analysis and Engineering Insight

* **Performance**:

    DSS demonstrates strong throughput characteristics, completing the small-chunk concatenation benchmark in ~6.8s and large-buffer concatenation in ~2.5s, 
    both for 5 GB of final data. The throughput gain in Experiment 2 highlights the low overhead of bulk operations compared to repetitive small concatenations.

* **Memory Efficiency**:

    Memory consumption is significantly higher than the actual data size peaking at 9.6 GB for a 5 GB dataset.
    
    This is primarily due to:

    1. Exponential growth factor in the reallocation logic (`dss_expand`), leading to temporary over-allocation.
    2. Lack of memory shrinkage after concatenation completes and capping exponential expansion beyond a threshold.

* **Optimization Targets**:

    The `dss_expand()` function needs improvement to:

  - Cap exponential expansion beyond a reasonable threshold.
  - Implement adaptive growth (e.g., capped at 1.5× past a certain buffer size).
  - Introduce buffer compaction/shrinking after large concatenation sequences.

### Summary

DSS achieves high performance at the cost of excessive memory overhead. The Valgrind Massif profiles clearly visualize the aggressive exponential 
expansion behavior and its impact on peak heap usage. Introducing controlled growth heuristics and post-expansion optimizations in `dss_expand` will 
significantly improve memory efficiency without compromising speed.
