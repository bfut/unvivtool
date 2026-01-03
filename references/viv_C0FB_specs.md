<!-- viv_C0FB_specs.md - unofficial 0xFBC0 file format specification -->
```
Copyright (C) 2024 by Benjamin Futasz <https://github.com/bfut> (CC BY 4.0)
This file is licensed under CC BY 4.0. To view a copy of this
license, visit <https://creativecommons.org/licenses/by/4.0>
The author is in no way affiliated with any of the herein mentioned
trademarks/entities, unless stated otherwise.
Date: 24Nov2024
```
# Unofficial 0xFBC0 File Format Specification (.viv)
This uncompressed container format is a variant of BIGF, BIGH, BIG4.  Known
examples can be found with NFS:PU (PC, 2000) at
`GameData/Track/Data/track##a.viv` (##=00, 02..10).  These files begin with a
fixed-size directory header, followed by variable-size directory entries.  The
directory is followed by the contained data, offset to the specified positions.
NFS:PU does not appear to load any of these files, their purpose is unknown.

Nicknames:
* 0xFBC0, 0x8000FBC0, FBC0, C0FB, .viv, VIV

```C++
struct Directory {
/* 0x0 */  uint8_t           format[2];           /* C0 FB, 0xFBC0 */
/* 0x2 */  uint8_t           unknown[2];          /* 00 80, 0x8000 */
/* 0x4 */  uint16_t          n_entries;           /* big endian */
/* 0x6 */  struct DirEntry   entries[n_entries];
}
struct DirEntry {
/* 0x0 */  uint24_t          offset;              /* big endian */
/* 0x3 */  uint24_t          filesize;            /* big endian */
/* 0x6 */  uint8_t          *filename;            /* null-terminated */
};
```

NB1: Implemented in unvivtool 3.6 and later</br>
NB2: Elsewhere, 0xFB10 at file position 0x0 signifies RefPack compression.</br>
NB3: Also check *nfs5_can_specs.md - unofficial CAN file format specification*
<!-- eof -->
