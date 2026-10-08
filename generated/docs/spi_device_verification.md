# SPI_DEVICE Verification Artifact

Generated from the device schema.

## Register checks

- `CONTROL` offset=`0x00000000` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `CFG` offset=`0x00000004` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `STATUS` offset=`0x00000008` access=`RO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `TX` offset=`0x0000000C` access=`WO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `RX` offset=`0x00000010` access=`RO` reset=`0x00000000` mask=`0xFFFFFFFF`

## Generator validation

- Register names are unique.
- Register offsets are unique.
- Register offsets are 4-byte aligned.
- Access policies are validated.
- Reset values are validated.
- Register masks are validated.
- Memory ranges are validated for overlap.
- Memory ranges are validated for uint64 overflow.
- The memory map is generated as a data-driven table.
- Register metadata is generated from the schema.
- Software offsets and masks are generated.
