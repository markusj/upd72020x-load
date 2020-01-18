# upd72020x-load

upd72020x-load is a Linux userspace firmware loader for Renesas uPD720201/uPD720202 familiy USB 3.0 controllers. 
It provides the functionality to upload the firmware required for certain extension cards before they can used with Linux.

## Usage

 * First, a firmware image for the chipset is required.
   It might be extracted from the update tool for Windows (either downloaded from the Internet or obtained from a driver disk shipped with the card).
   I am using version 2.0.2.6. I extracted the firmware image from an updater named `k2026fwup1` with SHA256 hash `9fe8fa750e45580ab594df2ba9435b91f10a6f1281029531c13f179cd8a6523c`. The firmware image has the SHA256 has `177560c224c73d040836b17082348036430ecf59e8a32d7736ce2b80b2634f97`.
 * The firmware image is uploaded using the command `./upd72020x-load -u -b 0x02 -d 0x00 -f 0x0 -i K2026.mem` with -b, -d and -f specifying the PCI bus, device and function address.
 * The script `check-and-init` simplifies the process since it automatically parses the output of `dmesg` for uPD720202 controllers which failed to initialize during boot and attemps to upload the firmware file to them.
   I simply call it from rc.local.
 * Code to read and write the (optional) EEPROM (commands `-r` and `-w`) is implemented as well.
   Reading should work, the write feature is untested, but shares the codebase with the upload feature.

## Some technical details

The uPD72020x USB 3.0 chipset familiy supports two modes of operation.
Either the firmware is stored at an external EEPROM chip and downloaded into the chipset at boot time, or it must be uploaded into the chipset by the operating system / driver.
The second option always works and overrides any firmware stored into the EEPROM of the card.

The story behind this tool and more technical details are discussed in a [blog post](https://mjott.de/blog/881-renesas-usb-3-0-controllers-vs-linux/).

## Closing remarks

This tool was only tested with an uPD720202 based extension card. 
The up- and download protocol of the uPD720201 chipset is identical according to the specification.
However, certain magic numbers, e.g. device identifiers and strings which are checked before any attempt to read or write data, need most likely to be changed for uPD720201 based devices.

And of course: Use this tool at your own risk!

## Acknowledgements

This code is based on the code which was written the comments section of [this blogpost](http://billauer.co.il/blog/2015/11/renesas-rom-setpci/).

