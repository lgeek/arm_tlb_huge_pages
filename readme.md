TLB size and huge pages on ARM cores
====================================

Context
-------

In a computer using virtual memory, [page tables](https://en.wikipedia.org/wiki/Page_table) are used to map virtual addresses to physical addresses and to set the R/W/E permissions for each page. Regular page tables on ARMv7 are up to two levels deep and ARMv8 and ARMv7 [LPAE](https://en.wikipedia.org/wiki/Physical_Address_Extension) tables can be up to three levels deep. To avoid walking the page table for each memory access, the pages in use are cached in a [TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer). Cortex-A implementations use a modified Harvard architecture (separate datapaths for instructions and data) with separate small and fast L1 TLBs and a slower and larger unified L2 TLB.


Huge pages
----------

The regular page size is 4KiB on most architectures, including ARM. To reduced the TLB pressure for applications which work with large datasets or which have large / fragmented code, huge pages (on Linux, 2MiB with ARMv7 LPAE / ARMv8 or 1MiB on ARMv7 without LPAE) can be used. Large page support for LPAE-enabled systems was added in the 3.11 version of the mainline Linux kernel - [patch 1](https://github.com/torvalds/linux/commit/dde1b65110), [patch2](https://github.com/torvalds/linux/commit/0b19f9335), [patch 3](https://github.com/torvalds/linux/commit/1355e2a6) and [patch 4](https://github.com/torvalds/linux/commit/8d962507) - support for [transparent huge pages](https://lwn.net/Articles/423592/).

The issue is that many ARMv7 cores from ARM don't properly support caching huge pages in their L1 TLB. To quote the [Cortex-A15 TRM](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438i/CACCECAH.html):

> If the page tables map the memory region to a larger granularity than 4K, it only allocates one mapping for the particular 4K region to which the current access corresponds.

The L2 TLB generally supports huge pages, but it can also cache a high number of entries. Even with 4 KiB pages, I don't expect L2 TLB misses to cause a significant slowdown for most applications.

The table below summarizes the TLB capabilities and sizes for ARM Cortex-A cores:

| Core       | LPAE support | L1 data TLB size | L1 data huge page support | L1 inst. TLB size | L1 inst. huge page support | L2 TLB size | L2 TLB huge page support |
|------------|--------------|------------------|---------------------------|-------------------|----------------------------|-------------|--------------------------|
| Cortex-A5  | N            | 10                       | ?            | 10           | ?         | 128                       | Y   |
| Cortex-A7  | Y            | 10                       | ?            | 10           | ?         | 256                       | Y   |
| Cortex-A8  | N            | 32                       | Y            | 32           | Y         | N/A                       | N/A |
| Cortex-A9  | N            | 32                       | ?            | 32 or 64     | ?         | 4 + (64, 128, 256 or 512) | Y   |
| Cortex-A15 | Y            | 32 (reads) + 32 (writes) | Optional 1MB | 32           | N         | 512                       | Y   |
| Cortex-A17 | Y            | 32                       | 1 MB only    | 32, 48 or 64 | 1 MB only | 1024                      | Y   |
| Cortex-A53 | N/A (64-bit) | 10                       | ?            | 10           | ?         | 512                       | Y   |
| Cortex-A57 | N/A (64-bit) | 32                       | 1 MB only    | 48           | 1 MB only | 1024                      | Y   |
| Cortex-A72 | N/A (64-bit) | 32                       | 1 MB only    | 48           | 1 MB only | 1024                      | Y   |



In the TRMs for A5, A7, A9 and A53 it's not clear what sizes are supported by the L1 micro TLBs.

Cortex-A8 doesn't seem to have a unified TLB.

None of the LPAE-enabled cores seem to support 2MB pages in the L1 TLBs.


Runtime detection
-----------------

Given the large number of cores with vague specifications or vendor-configurable options, I thought it would be interesting to develop a tool and a technique to determine the configuration by observing runtime behaviour, without any access to specs.

I'm introducing the unimaginatively named tlb test utility, which runs on ARM GNU/Linux systems.


Configurations determined using tlb test
----------------------------------------

| System | LPAE support | L1 data TLB size | L1 data huge page support | L1 inst. TLB size | L1 inst. huge page support | L2 TLB size | L2 TLB huge page support |
|------------|--------------|------------------|---------------------------|-------------------|----------------------------|-------------|--------------------------|
| Odroid-X2 (Exynos 4412 Prime, Cortex-A9) | N            | 32     | ? | 32     | ? | 4 + 128 | Y |
| Xilinx Zynq Z-7045 (Cortex-A9)           | N            | 32     | ? | 32     | ? | 4 + 128 | Y |
| ARM Juno LITTLE core (Cortex-A53)        | N/A (64-bit) | 10     | N | 10     | Y | 512     | Y |
| Rockchip RK3288 (Cortex-A17)             | Y            | 32     | Y | 32     | Y | 1024    | Y |
| Tegra K1 T124 (Cortex-A15)               | Y          | 32(+32?) | N | 32     | N | 512     | Y |
| Tegra K1 T132 (NVIDIA Denver)            | N/A (64-bit) | 256(?) | ? | 128(?) | ? | ?       | ? |
| APM883208 (APM X-Gene)                   | N/A (64-bit) | 20     | Y | 10     | N | 1024    | Y |

Theory of operation
-------------------

The basic idea is to load data (it's only testing the data TLB) from a configurable number of different pages, in quick succession, while minimising the effect of other sources of timing noise. The function *test()* in *tlb_test.s* reads *total_ops* words from *buffer*, each read incrementing the pointer by (4096 + 8) bytes, with wrap-around every *page_cnt* reads.

When actively reading from more pages than the TLB size, performance will suddenly decrease. By using a buffer larger than (L1-data-TLB-size * regular-page-size) and smaller than (L1-data-TLB-size * huge-page-size) allocated using huge pages, we can determine if the L1 TLB can cache huge pages.


Example
-------

As an example and sanity check, I've ran *tlb_test* on a Tegra K1 (Cortex-A15) SoC. I'm using [perf](https://perf.wiki.kernel.org/index.php/Main_Page) to confirm the causes of overhead. It's not strictly required and just execution time is good enough to use tlb_test. First, let's confirm the size of the L1 data TLB:

```
$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 16

 Performance counter stats for './tlb_test 16':

     7,001,546,456 instructions              #    0.00  insns per cycle        
     1,000,544,697 L1-dcache-loads                                             
            45,560 L1-dcache-load-misses     #    0.00% of all L1-dcache hits  
            50,669 dTLB-load-misses                                            

       2.368960939 seconds time elapse


$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 32

 Performance counter stats for './tlb_test 32':

     7,001,545,342 instructions              #    0.00  insns per cycle        
     1,000,545,046 L1-dcache-loads                                             
            45,619 L1-dcache-load-misses     #    0.00% of all L1-dcache hits  
            70,292 dTLB-load-misses                                            

       2.362317209 seconds time elapsed


$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 33

 Performance counter stats for './tlb_test 33':

     7,001,525,903 instructions              #    0.00  insns per cycle        
     1,000,538,344 L1-dcache-loads                                             
            44,769 L1-dcache-load-misses     #    0.00% of all L1-dcache hits  
        48,294,461 dTLB-load-misses                                            

       2.416565829 seconds time elapsed


$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 40

 Performance counter stats for './tlb_test 40':

     7,002,175,820 instructions              #    0.00  insns per cycle        
     1,000,795,312 L1-dcache-loads                                             
            69,207 L1-dcache-load-misses     #    0.01% of all L1-dcache hits  
       956,369,999 dTLB-load-misses                                            

       3.886506385 seconds time elapsed
```


Starting with 33 pages, the number of dTLB misses increases dramatically. Even without perf, we could easily deduce the TLB size is 32 using the timing information.

Next, let's confirm the size of the L2 TLB:

```
$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 48

 Performance counter stats for './tlb_test 48':

     7,002,547,418 instructions              #    0.00  insns per cycle        
     1,000,921,383 L1-dcache-loads                                             
            86,844 L1-dcache-load-misses     #    0.01% of all L1-dcache hits  
     1,000,052,404 dTLB-load-misses                                            

       4.002053526 seconds time elapsed


$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 256

 Performance counter stats for './tlb_test 256':

     7,003,012,833 instructions              #    0.00  insns per cycle        
     1,001,087,251 L1-dcache-loads                                             
           120,170 L1-dcache-load-misses     #    0.01% of all L1-dcache hits  
     1,000,078,542 dTLB-load-misses                                            

       3.995740584 seconds time elapsed


$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 512

 Performance counter stats for './tlb_test 512':

     7,002,650,367 instructions              #    0.00  insns per cycle        
     1,000,948,480 L1-dcache-loads                                             
           140,842 L1-dcache-load-misses     #    0.01% of all L1-dcache hits  
     1,000,061,642 dTLB-load-misses                                            

       4.002590827 seconds time elapsed


$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 513

 Performance counter stats for './tlb_test 513':

     7,002,650,847 instructions              #    0.00  insns per cycle        
     1,000,938,797 L1-dcache-loads                                             
           127,003 L1-dcache-load-misses     #    0.01% of all L1-dcache hits  
     1,000,060,931 dTLB-load-misses                                            

       4.037016451 seconds time elapsed
   
       
$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 520

 Performance counter stats for './tlb_test 520':

     7,003,596,287 instructions              #    0.00  insns per cycle        
     1,001,290,282 L1-dcache-loads                                             
           179,866 L1-dcache-load-misses     #    0.02% of all L1-dcache hits  
     1,000,092,440 dTLB-load-misses                                            

       4.549982772 seconds time elapsed

$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 1024

 Performance counter stats for './tlb_test 1024':

     7,008,804,035 instructions              #    0.00  insns per cycle        
     1,003,187,234 L1-dcache-loads                                             
           465,421 L1-dcache-load-misses     #    0.05% of all L1-dcache hits  
     1,000,212,790 dTLB-load-misses                                            

      16.194994147 seconds time elapsed
```

Note how the performance with 48, 256 and 512 pages is practically identical, while with 520 it is significantly slower. When the data size is double the L2 TLB size (1024 pages), execution slows down by a factor of 4x.

Now let's see if using huge pages can help. First, when the data size would fit in the L2 TLB using normal pages, but not in the L1 TLB:

```
$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 40 -huge

 Performance counter stats for './tlb_test 40 -huge':

     7,003,023,486 instructions              #    0.00  insns per cycle        
     1,000,989,951 L1-dcache-loads                                             
           149,260 L1-dcache-load-misses     #    0.01% of all L1-dcache hits  
       960,840,312 dTLB-load-misses                                            

       3.917991822 seconds time elapsed
```

Esentially no difference in execution time, a good confirmation that the L1 TLB only caches 4KiB out of a huge page (arg, ARM, whyyyy?!?).

And let's also test huge pages on the L2 TLB, which should work:

```
$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 520 -huge

 Performance counter stats for './tlb_test 520 -huge':

     7,003,126,596 instructions              #    0.00  insns per cycle        
     1,000,893,472 L1-dcache-loads                                             
        27,634,720 L1-dcache-load-misses     #    2.76% of all L1-dcache hits  
     1,000,077,382 dTLB-load-misses                                            

       3.874247451 seconds time elapsed

$ perf stat -e instructions,L1-dcache-loads,L1-dcache-load-misses,dTLB-load-misses ./tlb_test 1024 -huge

 Performance counter stats for './tlb_test 1024 -huge':

     7,004,239,868 instructions              #    0.00  insns per cycle        
     1,001,151,686 L1-dcache-loads                                             
     1,000,103,661 L1-dcache-load-misses     #   99.90% of all L1-dcache hits  
     1,000,126,224 dTLB-load-misses                                            

       5.313755445 seconds time elapsed
```

Success, huge pages are indeed cached correctly by the L2 TLB. Note that now almost all loads miss in the L1 data cache. We're loading 1024 words, each in a separate (64 byte) cache line. The L1 cache on this core is only 32 KiB in size, therefore we're completely thrashing it at each iteration. When using 4KiB pages this is not an issue because our virtual memory is uninitialized, so all pages point to the same physical zero page and the L1 data cache is [physically indexed, physically tagged](https://en.wikipedia.org/wiki/CPU_cache#Address_translation).

