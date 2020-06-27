# Small Deflate
SDeflate is a small bare bone lossless compression library in ANSI C (ISO C90)
that implements the Deflate (RFC 1951) compressed data format specification standard.

SDeflate is mainly tuned to get as much speed and compression ratio from as little code
as needed to keep the implementation as concise as possible. In addition since
this library implements the Deflate standard some limitations including a small
match window of only 32kb apply. So don't expect comparable speed or compression
ratio to some of the newer algorithms.

## Features
- Portable single header and source file duo written in ANSI C (ISO C90)
- Small ~300 LoC implementation (Deflate: 170 LoC, Inflate: 150 LoC)
- Dual license with either MIT or public domain
- Webassembly:
    - Deflate ~3.7 KB (~2.2KB compressed)
    - Inflate ~3.6 KB (~2.2KB compressed)

## Usage
For deflating add sdefl(h)[.h+.c] into your project and include `sdefl.h`. To actually
compress memory first call `sdefl_bound` to calculate the maximum number of compressed
output and allocate an compress output memory buffer and pass it along with your
to compress input data and a compression level between `SDEFL_LVL_MIN` and
`SDEFL_LVL_MAX` to the main compression function `sdeflate`.

For inflating add sinfl(.h+.c) into your project and include `sinfl.h`. To
decompress a previously compressed block of memory call `sinflate`. Pass
your compressed block along with its size and an allocated output buffer
with size of the uncompressed block.

## Benchmark

### SDEFLH
| Compressor name         | Compression| Decompress.| Compr. size | Ratio |
| ---------------         | -----------| -----------| ----------- | ----- |
| sdeflh 1.0 -0           |   115 MB/s |    94 MB/s |    40004116 | 40.00 |
| sdeflh 1.0 -1           |   102 MB/s |    96 MB/s |    38940674 | 38.93 |
| sdeflh 1.0 -5           |    56 MB/s |   100 MB/s |    36577183 | 36.57 |
| sdeflh 1.0 -7           |    45 MB/s |   100 MB/s |    36523781 | 36.52 |
| libdeflate 1.3 -1       |   147 MB/s |   667 MB/s |    39597378 | 39.60 |
| libdeflate 1.3 -6       |    69 MB/s |   689 MB/s |    36648318 | 36.65 |
| libdeflate 1.3 -9       |    13 MB/s |   672 MB/s |    35197141 | 35.20 |
| libdeflate 1.3 -12      |  8.13 MB/s |   670 MB/s |    35100568 | 35.10 |
| zlib 1.2.11 -1          |    72 MB/s |   307 MB/s |    42298774 | 42.30 |
| zlib 1.2.11 -6          |    24 MB/s |   313 MB/s |    36548921 | 36.55 |
| zlib 1.2.11 -9          |    20 MB/s |   314 MB/s |    36475792 | 36.48 |
| miniz 1.0 -1            |   122 MB/s |   208 MB/s |    48510028 | 48.51 |
| miniz 1.0 -6            |    27 MB/s |   260 MB/s |    36513697 | 36.51 |
| miniz 1.0 -9            |    23 MB/s |   261 MB/s |    36460101 | 36.46 |

Results on the [Silesia compression corpus](http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia):

| File    |   Original | `sdefl 0`  	| `sdefl 5` 	| `sdefl 7` |
| :------ | ---------: | -----------------: | ---------: | ----------: |
| dickens | 10.192.446 |  4,306,115|  3,883,830|   3,872,395 |
| mozilla | 51.220.480 | 21,446,457 | 20,190,172 |  20,135,328 |
| mr      |  9.970.564 | 3,976,467 |  3,800,152 |   3,791,038 |
| nci     | 33.553.445 | 4,056,751 |  3,118,187 |   3,029,206 |
| ooffice |  6.152.192 | 3,387,661 |  3,247,680 |   3,242,686 |
| osdb    | 10.085.684 | 3,937,560 |  3,663,956 |   3,663,922 |
| reymont |  6.627.202 | 2,311,181 |  1,896,375 |   1,866,021 |
| samba   | 21.606.400 | 6,426,970 |  5,749,948 |   5,735,605 |
| sao     |  7.251.944 | 5,574,768 |  5,550,965 |   5,546,685 |
| webster | 41.458.703 | 14,034,753 | 12,110,193 |  12,042,736 |
| xml     |  5.345.280 | 922,931|    714,539 |     701,820 |
| x-ray   |  8.474.240 | 6,389,985 |  6,327,069 |   6,327,069 |

### SDEFL
| Compressor name         | Compression| Decompress.| Compr. size | Ratio |
| ---------------         | -----------| -----------| ----------- | ----- |
| sdefl 1.0 -0            |   115 MB/s |    94 MB/s |    46489930 | 46.49 |
| sdefl 1.0 -1            |   102 MB/s |    96 MB/s |    45291608 | 45.29 |
| sdefl 1.0 -5            |    56 MB/s |   100 MB/s |    43983562 | 43.98 |
| sdefl 1.0 -8            |    45 MB/s |   100 MB/s |    43914863 | 43.91 |
| libdeflate 1.3 -1       |   147 MB/s |   667 MB/s |    39597378 | 39.60 |
| libdeflate 1.3 -6       |    69 MB/s |   689 MB/s |    36648318 | 36.65 |
| libdeflate 1.3 -9       |    13 MB/s |   672 MB/s |    35197141 | 35.20 |
| libdeflate 1.3 -12      |  8.13 MB/s |   670 MB/s |    35100568 | 35.10 |
| zlib 1.2.11 -1          |    72 MB/s |   307 MB/s |    42298774 | 42.30 |
| zlib 1.2.11 -6          |    24 MB/s |   313 MB/s |    36548921 | 36.55 |
| zlib 1.2.11 -9          |    20 MB/s |   314 MB/s |    36475792 | 36.48 |
| miniz 1.0 -1            |   122 MB/s |   208 MB/s |    48510028 | 48.51 |
| miniz 1.0 -6            |    27 MB/s |   260 MB/s |    36513697 | 36.51 |
| miniz 1.0 -9            |    23 MB/s |   261 MB/s |    36460101 | 36.46 |

Results on the [Silesia compression corpus](http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia):

| File    |   Original | `sdefl 0`  	| `sdefl 5` 	| `sdefl 7` |
| :------ | ---------: | -----------------: | ---------: | ----------: |
| dickens | 10.192.446 |  5,323,512|  4,865,325|   4,859,501 |
| mozilla | 51.220.480 | 22,690,633 | 21,798,440 |  21,756,742 |
| mr      |  9.970.564 | 4,921,002 |  4,570,440 |   4,560,160 |
| nci     | 33.553.445 | 4,557,728 |  3,931,585 |   3,889,564 |
| ooffice |  6.152.192 | 3,684,312 |  3,582,462 |   3,579,649 |
| osdb    | 10.085.684 | 4,328,329 |  4,216,607 |   4,216,534 |
| reymont |  6.627.202 | 2,712,863 |  2,304,258 |   2,272,241 |
| samba   | 21.606.400 | 6,744,238 |  6,283,100 |   6,270,438 |
| sao     |  7.251.944 | 6,102,068 |  5,938,370 |   5,936,135 |
| webster | 41.458.703 | 16,345,668 | 14,834,226 |  14,764,309 |
| xml     |  5.345.280 | 1,043,013|    828,667 |     818,717 |
| x-ray   |  8.474.240 | 7,687,883 |  7,642,441 |   7,642,441 |


## License
```
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Micha Mettke
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
```
