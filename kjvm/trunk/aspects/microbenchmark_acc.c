//==============================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 Christopher Stone. 
//
// Jem is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License, version 2, as published by the Free
// Software Foundation.
//
// Jem is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// Jem; if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
// Fifth Floor, Boston, MA 02110-1301, USA
//
//==============================================================================


#define BMPRINT(txt) t1 = ((u8_t)bt.a) << 32 | (u8_t)bt.b; \
                     t2 = ((u8_t)bt.c) << 32 | (u8_t)bt.d; \
                     t3 = t2 - t1; \
                     a = t3 >> 32; \
                     b = t3 & 0xffffffff; \
                     printf("%s 0x%lx%lx\n", txt, a, b);

#define FLUSHCACHE {int i; for(i=0; i<totalmem; i++) {benchmem[i] = 1;}}

before(); call(void initPrimitiveClasses(...))
{
    struct benchtime_s bt;
    u8_t t1, t2, t3;
    u4_t a, b;
    u4_t totalmem = 1024 * 1024;

    benchmem = jxmalloc(totalmem MEMTYPE_PROFILING);
    bench_empty(&bt);
    bench_empty(&bt);
    BMPRINT("empty");
    bench_store(&bt);
    bench_store(&bt);
    BMPRINT("store/hot");
    bench_store1(&bt);
    bench_store1(&bt);
    BMPRINT("store1/hot");
    FLUSHCACHE;
    bench_store1(&bt);
    BMPRINT("store1/cold");
    bench_load1(&bt);
    bench_load1(&bt);
    BMPRINT("load1/hot");
    FLUSHCACHE;
    bench_load1(&bt);
    BMPRINT("load1/cold");
    sys_panic("END OF BENCHMARK");
}

