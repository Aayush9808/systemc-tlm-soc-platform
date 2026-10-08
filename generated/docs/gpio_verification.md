# GPIO Verification Artifact

Generated from the device schema.

## Register checks

- `DATA_IN` offset=`0x00000000` access=`RO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `DIRECT_OUT` offset=`0x00000004` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `MASKED_OUT_LOWER` offset=`0x00000008` access=`WO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `MASKED_OUT_UPPER` offset=`0x0000000C` access=`WO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `DIRECT_OE` offset=`0x00000010` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `MASKED_OE_LOWER` offset=`0x00000014` access=`WO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `MASKED_OE_UPPER` offset=`0x00000018` access=`WO` reset=`0x00000000` mask=`0xFFFFFFFF`
- `INTR_STATE` offset=`0x0000001C` access=`W1C` reset=`0x00000000` mask=`0xFFFFFFFF`
- `INTR_ENABLE` offset=`0x00000020` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `INTR_RISE` offset=`0x00000024` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`
- `INTR_FALL` offset=`0x00000028` access=`RW` reset=`0x00000000` mask=`0xFFFFFFFF`

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
