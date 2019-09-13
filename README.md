# cefore-filter
This is a cache algortihm plugin of a frequency-based cache admission algorithm, Filter, for csmgrd of Cefore. 

Cefore is a software platform that enables CCN-like communications and csmgrd is an implementation of a content cache running on Cefore. For details about Cefore and csmgrd, please see https://cefore.net/.

## Required Libraries
This software itself is self-contained. For required libraries for Cefore and csmgrd, please the Section 1.1 of the cefore's user manual (https://cefore.net/Cefore-UserMannual.pdf).

## Bulid
0. Get a cefore source code provided by Cefore Team. Then, build the source code. Note that we simply refer the path for your cefore source code directory to CEFORE_DIR, hereafter. For details about the build, please see the Section 2.3 of the cefore's user manual (https://cefore.net/Cefore-UserMannual.pdf).

```
wget https://cefore.net/dlfile.php?file=cefore-0.8.1.zip
```

1. Create a directory named "filter" under "CEFORE_DIR/src/csmgrd/plugin/lib". Then, put filter.c and lib on the directory.

2. Get `cache_replace_lib.c`, `cache_replace_lib.h` and `Makefile.in` from the directory `CEFORE_DIR/src/csmgrd/plugin/lib/fifo`. Then, put these three files on `CEFORE_DIR/src/csmgrd/plugin/lib/filter`.

3. Create a header file and a Makefile of filter for cefore cache plugin. Then, put these two files on `CEFORE_DIR/src/csmgrd/plugin/lib/filter`.

    To create header file, `cp CEFORE_DIR/src/csmgrd/plugin/lib/fifo/fifo.h CEFORE_DIR/src/csmgrd/plugin/lib/filter` and `mv fifo.h filter.h`.

    To create header file, `cp CEFORE_DIR/src/csmgrd/plugin/lib/fifo/Makefile.am CEFORE_DIR/src/csmgrd/plugin/lib/filter`. Then, replace the term "fifo" with "filter" in the gotten Makefile.am.

4. For configuration of build, add the line `CEFORE_DIR/src/csmgrd/plugin/lib/filter/Makefile` under the comment `check csmgr` in `CEFORE_DIR/configure.ac`.

5. Rebuild the cefore as follows:
```
autoconf
automake
make
sudo make install
sudo ldconfig
```

## Usage

Modify the line `CACHE_ALGORITHM=` to `CACHE_ALGORITHM=libcsmgrd_filter` in  csmgrd cofigure file `csmgrd.conf`.
Then,run Cefore. You can use filter cache plugin.


The following documents (in Japanese) help you bulding and configuring the custom cache plugin for csmgrd.

- http://www.ieice.org/~icn/wp-content/uploads/2017/08/ICN_hands_on.pdf

- http://www.ieice.org/~icn/wp-content/uploads/2018/08/hands_on_01_Cefore.pdf


## Licence
This software is released under the MIT License, see LICENSE.txt.

## Contributors
Junji Takemasa

Yuki Koizumi

Toru Hasegawa

## Brieaf Description of Filter
Filter is a frequency-based cache admission algorithm, which decides whether an incoming Data packet is inserted into the cache or not, with both lightweight computation and high cache hit rate. It is designed for high-speed Information Centric Netwokring (ICN) software routers.
Filter improves both computation speed and cache hit rate of the cache eviction algorithm, which decides which Data packet is evicted from the cache, by bypassing wasteful cache eviction computations for unpouplar Data packets based on their frequency counter. For details about the algorithm design and performance benchmarks, please see our paper in IPSJ Journal 2019.

## References
1. J. Takemasa, Y. Koizumi and T. Hasegawa, "Lightweight Cache Admission Algorithm for Fast NDN Software Routers," Journal of Information Processing, vol. 27, pp. 125-134, 2019.

2. J. Takemasa, K. Taniguchi, Y. Koizumi and T. Hasegawa, "Identifying Highly Popular Content Improves Forwarding Speed of NDN Software Router," In Proceedings of IEEE GLOBECOM Workshop on Information Centric Networking Solutions for Real World Applications (ICNSRA), pp. 1–6, 2016.
