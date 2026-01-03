<!-- nfs5_can_specs.md - unofficial CAN file format specification -->
```
Copyright (C) 2024 by Benjamin Futasz <https://github.com/bfut> (CC BY 4.0)
This file is licensed under CC BY 4.0. To view a copy of this
license, visit <https://creativecommons.org/licenses/by/4.0>
The author is in no way affiliated with any of the herein mentioned
trademarks/entities, unless stated otherwise.
Date: 22Nov2024
```
# Unofficial CAN File Format Specification (.can)
Unknown purpose.  Part of NFS:PU (PC, 2000).  Known examples are archived in the
`GameData/Track/Data/track##a.viv` (##=00, 02..10) set.
```C++
struct Header {
/* 0x0 */  uint16_t  filesize;  /* little endian */
/* 0x2 */  uint8_t   unknown;  /* 03 */
/* 0x3 */  uint8_t   unknown[5];  /* null */
/* 0x8 */  uint16_t  unknown;  /* ? variable */
/* 0xA */  uint16_t  unknown;  /* 06 00 */
};
// sizeof(Header) == 12 (0xC)
```
NB: Also check *viv_C0FB_specs.md - unofficial 0xFBC0 file format specification*
<!-- eof -->
