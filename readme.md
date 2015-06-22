TLB size and huge pages on ARMv7 cores
======================================

Context
-------

In a computer using virtual memory, [page tables](https://en.wikipedia.org/wiki/Page_table) are used to map virtual addresses to physical addresses and to set the R/W/E permissions for each page. Regular page tables on ARMv7 are up to two levels deep and [LPAE](https://en.wikipedia.org/wiki/Physical_Address_Extension) tables can be up to three levels deep. To avoid walking the page table for each memory access, the pages in use are cached in a [TLB](https://en.wikipedia.org/wiki/Translation_lookaside_buffer). Cortex-A implementations use a modified Harvard architecture (separate datapaths for instructions and data) with separate small and fast L1 TLBs and a slower and larger unified L2 TLB.


Huge pages
----------

The regular page size is 4KiB on most architectures, including ARMv7. To reduced the TLB pressure for applications which work with large datasets or which have large / fragmented code, huge pages (on Linux on ARMv7, 2MiB with LPAE or 1MiB without) can be used. ARMv7 support was added in the 3.11 version of the mainline Linux kernel - [patch 1](https://github.com/torvalds/linux/commit/dde1b65110), [patch2](https://github.com/torvalds/linux/commit/0b19f9335), [patch 3](https://github.com/torvalds/linux/commit/1355e2a6) and [patch 4](https://github.com/torvalds/linux/commit/8d962507) - support for [transparent huge pages](https://lwn.net/Articles/423592/).

The issue is that many ARMv7 cores from ARM don't properly support caching huge pages in their L1 TLB. To quote the [Cortex-A15 TRM](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438i/CACCECAH.html):

> If the page tables map the memory region to a larger granularity than 4K, it only allocates one mapping for the particular 4K region to which the current access corresponds.

The L2 TLB generally supports huge pages, but it can also cache a high number of entries. Even with 4 KiB pages, I don't expect L2 TLB misses to cause a significant slowdown for most applications.

The table below summarizes the TLB capabilities and sizes for ARMv7 Cortex-A cores:

| Core       | LPAE support | L1 data TLB size | L1 data huge page support | L1 inst. TLB size | L1 inst. huge page support | L2 TLB size | L2 TLB huge page support |
|------------|--------------|------------------|---------------------------|-------------------|----------------------------|-------------|--------------------------|
| Cortex-A5  | N            | 10                       | ?            | 10           | ?         | 128                       | Y   |
| Cortex-A7  | Y            | 10                       | ?            | 10           | ?         | 256                       | Y   |
| Cortex-A8  | N            | 32                       | Y            | 32           | Y         | N/A                       | N/A |
| Cortex-A9  | N            | 32                       | ?            | 32 or 64     | ?         | 4 + (64, 128, 256 or 512) | Y   |
| Cortex-A15 | Y            | 32 (reads) + 32 (writes) | Optional 1MB | 32           | N         | 512                       | Y   |
| Cortex-A17 | Y            | 32                       | 1 MB only    | 32, 48 or 64 | 1 MB only | 1024                      | Y   |


In the TRMs for A5, A7 and A9 it's not clear what sizes are supported by the L1 micro TLBs.

Cortex-A8 doesn't seem to have a unified TLB.

None of the LPAE-enabled cores seem to support 2MB pages in the L1 TLBs.


Runtime detection
-----------------

Given the large number of cores with vague specifications or vendor-configurable options, I thought it would be interesting to develop a tool and a technique to determine the configuration by observing runtime behaviour, without any access to specs.

I'm introducing the unimaginatively named tlb_test utility, which runs on ARM GNU/Linux systems.


Configurations determined using tlb_test
----------------------------------------

| System | LPAE support | L1 data TLB size | L1 data huge page support | L1 inst. TLB size | L1 inst. huge page support | L2 TLB size | L2 TLB huge page support |
|------------|--------------|------------------|---------------------------|-------------------|----------------------------|-------------|--------------------------|
| Odroid-X2 (Exynos 4412 Prime, Cortex-A9) | N | 32 | ? | ? | ? | 4 + 128 | Y |



Theory of operation
-------------------

The basic idea is to load data (it's only testing the data TLB) from a configurable number of different pages, in quick succession, while minimising the effect of other sources of timing noise. The function *test()* in *tlb_test.s* reads *total_ops* words from *buffer*, each read incrementing the pointer by (4096 + 8) bytes, with wrap-around every *page_cnt* reads.

When actively reading from more pages than the TLB size, performance will suddenly decrease. By using a buffer larger than (L1-data-TLB-size * regular-page-size) and smaller than (L1-data-TLB-size * huge-page-size) allocated using huge pages, we can determine if the L1 TLB can cache huge pages.


