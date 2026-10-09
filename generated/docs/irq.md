# IRQ

- Version: 1
- Address width: 32
- Register width: 32

| Register | Offset | Access | Reset | Mask |
|---|---:|---|---:|---:|
| PENDING | 0x00000000 | RW | 0x00000000 | 0xFFFFFFFF |
| ENABLE | 0x00000004 | RW | 0x00000000 | 0xFFFFFFFF |
| CLAIM | 0x00000008 | RO | 0x00000000 | 0xFFFFFFFF |
| COMPLETE | 0x0000000C | WO | 0x00000000 | 0xFFFFFFFF |
