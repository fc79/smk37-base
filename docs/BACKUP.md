# Backing up the SMK-37's flash, and putting it back

Before any custom firmware goes on an SMK-37, read its whole flash (1 MiB) and keep it. Two ranges are unique to
your keyboard and are in no package: `0xC0000-0xC1FFF` (Bluetooth pairing, the key / pad / wheel calibration
`trimdat`) and `0xFF000-0xFFFFF` (key_mac). The rest of the stock image can always come back from M-Vave's package.

This reads the flash **without writing anything** to the keyboard. It was done from a Mac on an SMK-37 Pro (stock
v015); Linux and Windows need the same steps with the stock jl-uboot-tool (no adapter).

## What you need

- Python 3 with `mido python-rtmidi` (the MIDI side) and, on a Mac, `pyusb` plus libusb (`brew install libusb`).
- [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) (kagaimiq, MIT) with its requirements
  (`crcmod pyyaml pycryptodomex tqdm`).
- On a Mac, the adapter in `tools/backup/jl-uboot-tool-macos/`: jl-uboot-tool talks SCSI through Linux's SG_IO or
  Windows' SPTI; the adapter speaks USB Bulk-Only Transport over libusb instead. Into a checkout of jl-uboot-tool:

  ```sh
  cp tools/backup/jl-uboot-tool-macos/macport.py JLTOOL/scsiio/
  cp tools/backup/jl-uboot-tool-macos/mac_*.py JLTOOL/
  cd JLTOOL && patch -p1 < .../tools/backup/jl-uboot-tool-macos/jl-uboot-tool-macos.patch
  ```

  It needs `sudo` (macOS has to let go of the disk). Try it first on any USB stick, read only:
  `sudo python3 mac_selftest.py usb:VVVV:PPPP` (`python3 mac_selftest.py` lists the devices).

## Reading the flash

1. **The mask-ROM bootloader, without writing.** The stock firmware enters it on the UBOOT soft key
   `F0 22 24 35 7D F7`, sent as one 8-byte USB-MIDI transfer on cable 0, the port "SMK-37 Pro-Private". So do
   SMK BASE and its update loader. The ROM shows up as a USB disk `WL82 UBOOT1.00` (4C4A:8057) and **goes back to the
   firmware by itself after a short while** if nobody talks to it, so start the reader first:

   ```sh
   cd JLTOOL && sudo python3 jluboottool.py --device usb:4c4a:8057 --chip wl82 \
       "read 0 0x100000 dump-1.bin" "read 0 0x100000 dump-2.bin"
   ```

   It prints `Waiting for [usb:4c4a:8057]...`. Then, in another terminal:

   ```sh
   python3 tools/backup/uboot_key.py --send
   ```

   The reader loads its program into the chip's RAM (nothing in flash), prints the chip key (`0x980F`) and the flash
   id, and reads the flash twice (~2 s each). Only `read`: never `write`, `erase`, `erasechip` or `burnchipkey` here.
   (On Linux / Windows: `--device /dev/sgN` or the disk's path, no `sudo` adapter.)

2. **Check the dumps:**

   ```sh
   python3 tools/backup/verify_dump.py dump-1.bin dump-2.bin [flash.bin]
   ```

   Both must be 1,048,576 bytes and identical, with `trimdat` at 0xC1224. With `flash.bin` unpacked from the stock
   package your keyboard ran, it also shows how much of the app area matches.

3. **Power-cycle** the keyboard: what ran before comes back. Keep the dump in two places.

## Putting it back

Not tried yet: the steps are the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) guide's (section 5.1),
in whole 4 KiB sectors, in the mask-ROM bootloader. **Never** write `0x0000-0x3FFF`, `0xC0000-0xC1FFF` or
`0xFF000-0xFFFFF` unless they were damaged, and then only from your own dump.

```sh
# the app area (0x4000-0x9CFFF) and the stock settings (0x9D000-0xBFFFF), cut from your dump
python3 -c "d=open('dump-1.bin','rb').read(); open('app-store.bin','wb').write(d[0x4000:0xC0000])"
sudo python3 jluboottool.py --device usb:4c4a:8057 --chip wl82 "erase 0x4000 0xBC000"
sudo python3 jluboottool.py --device usb:4c4a:8057 --chip wl82 "write 0x4000 app-store.bin"
sudo python3 jluboottool.py --device usb:4c4a:8057 --chip wl82 "read 0x4000 0xBC000 check.bin"
cmp app-store.bin check.bin
```

jl-uboot-tool's `erase` works in 4 KiB sectors up to a 64 KiB boundary and in 64 KiB blocks only where a whole block
lies in the range, so `0x4000 + 0xBC000` stays inside `0x4000-0xBFFFF`.

The other way back is M-Vave's package for your keyboard, with M-Vave's updater or the
[Firmware Hub](https://thefirmwarehub.com/smk37/). One report says the official updater, run from a custom firmware,
erased an SMK-37's calibration: keep your dump.
