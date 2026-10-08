# RV_TIMER Verification Artifact

Generated from the device schema.

## Register checks

- `CTRL` offset=`0x00000000` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `CFG0` offset=`0x00000004` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `TIMER_V_LOWER` offset=`0x00000008` access=`RO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `TIMER_V_UPPER` offset=`0x0000000C` access=`RO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `COMPARE_LOWER` offset=`0x00000010` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `COMPARE_UPPER` offset=`0x00000014` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `INTR_STATE` offset=`0x00000018` access=`W1C` reset=`0x00000000` mask=`0xFFFFFFFF`
- `INTR_ENABLE` offset=`0x0000001C` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`

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
