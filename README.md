# upd72020x-load

upd72020x-load is a Linux userspace firmware loader for Renesas uPD720201/uPD720202 familiy USB 3.0 controllers. 
It provides the functionality to upload the firmware required for certain extension cards before they can used with Linux.

## Usage

 * First, a firmware image for the chipset is required.
   It might be extracted from the update tool for Windows (either downloaded from the Internet or obtained from a driver disk shipped with the card).
   I am using firmware version 2.0.2.6 with my uPD720202-based card. I extracted the firmware image from an updater named `k2026fwup1` with SHA256 hash `9fe8fa750e45580ab594df2ba9435b91f10a6f1281029531c13f179cd8a6523c`. The firmware image has the SHA256 has `177560c224c73d040836b17082348036430ecf59e8a32d7736ce2b80b2634f97`.
 * The firmware image is uploaded into the chipset RAM using the command `./upd72020x-load -u -b 0x02 -d 0x00 -f 0x0 -i K2026.mem` with -b, -d and -f specifying the PCI bus, device and function address.
   This process is non-persistent, the chipset RAM is cleared when the power supply is removed.
 * The error message `ERROR: SET_DATAx never go to zero` is apparently sometimes(?) caused by a conflict with the XHCI kernel driver. 
   Use `echo -n 0000:02:00.0 > /sys/bus/pci/drivers/xhci_hcd/unbind` before and `echo -n 0000:02:00.0 > /sys/bus/pci/drivers/xhci_hcd/bind` after running upd72020x-load, with the correct PCI address for your computer.
 * The script `upd72020x-check-and-init` automates the upload process. It parses the output of `dmesg` for uPD72020x controllers which failed to initialize during boot and attemps to upload the firmware file to them.
   It also performs the driver unbind/bind commands to work around the conflict with the XHCI kernel driver.
   I simply call the script from rc.local, but a SystemD service file is also included.
   For using SystemD, please adjust the paths/environment variables in the unit file according to your install locations of script, loader and firmware image.
   If no environment variables are set, the script presumes that loader and firmware image are co-located with itself in the same directory.
 * Code to read and write the (optional) EEPROM (commands `-r` and `-w`) is implemented as well.
   Reading should work, the write feature is untested, but shares the codebase with the upload feature. 
   However, the documentation indicates a special memory layout for the EEPROM. 
   Besides the firmware, it may contain a Vendor Specific Configuration Data section (VSCD) (see also discussion in [#4](https://github.com/markusj/upd72020x-load/issues/4)).
   This should be taken into account when trying to write to the EEPROM since this tool does not deal with the memory layout.
   Thus, I strongly discourage you from using the `-w` command unless you surely known what you are doing.

## Some technical details

The uPD72020x USB 3.0 chipset familiy supports two modes of operation.
Either the firmware is stored at an external EEPROM chip and downloaded into the chipset at boot time, or it must be uploaded into the chipset by the operating system / driver.
The second option always works and overrides any firmware stored into the EEPROM of the card.

The story behind this tool and more technical details are discussed in a [blog post](https://mjott.de/blog/881-renesas-usb-3-0-controllers-vs-linux/).

## Closing remarks

This tool was only tested with an uPD720202 based extension card. 
The up- and download protocol of the uPD720201 chipset is identical according to the specification, and reports confirmed the tool to work for them, too.

And of course: Use this tool at your own risk!

## Acknowledgements

This code is based on the code which was written the comments section of [this blogpost](http://billauer.co.il/blog/2015/11/renesas-rom-setpci/).

 * The SystemD unit file was originally provided by K.Ohta (@Artanejp)
 * Vendor and device IDs for uPD720201 chipsets were provided by @j1warren
 * The workaround for kernel drivers interfering with the upload process was found by @cranphin
