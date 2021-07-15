# bitflip
Error injection in Artix-7 FPGA
## Use
```bash
bitflip frame_address bit_offset raw_file
```
with frame_address and bit_offset extracted from .ll file, and raw_file is a file generated from a read-back in openOCD.
The raw_file must be 2 frames long.
