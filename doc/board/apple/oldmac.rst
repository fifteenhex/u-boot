.. SPDX-License-Identifier: GPL-2.0+

U-Boot on the classic 68k Macintosh
===================================

The ``oldmac`` board runs U-Boot on classic (Motorola 680x0) Apple Macintosh
computers.  It is started by the machine's own Apple ROM exactly the way Mac OS
is - from a Macintosh boot block on a SCSI CD-ROM or hard disk - so no firmware
replacement is needed.  The target is real hardware; QEMU's ``q800`` machine
(which emulates a real Quadra 800) is the development and test vehicle.

The boot chain is an ordinary U-Boot multi-stage boot dressed up as a bootable
Mac volume::

    Apple ROM  ->  Mac boot block  ->  U-Boot SPL  ->  U-Boot proper

The CD or disk carries an Apple driver-descriptor record, an Apple partition map
and a small SCSI disk driver, just as a real bootable Mac volume does.  The ROM
loads that driver and the blessed boot block; the boot block loads the SPL, and
the SPL loads U-Boot proper.

Supported machines
------------------

The Gestalt machine type reported by the ROM selects the peripheral layout at
runtime, so a single build supports several Quadra/Centris-class models:

* Quadra 800 (68040) - the primary model, and what QEMU's ``q800`` emulates.
* Quadra 700 (68040) - QUADRA2-style pseudo-DMA SCSI.
* LC 475 / Quadra 605 (68LC040, frequently upgraded to a full 68040).

The 68030 Mac IIsi code paths also exist but are experimental.

Building
--------

.. code-block:: bash

    $ export CROSS_COMPILE=m68k-linux-gnu-
    $ make oldmac_defconfig
    $ make

This builds the SPL (``spl/u-boot-spl.bin``), U-Boot proper (``u-boot.bin``) and
a bootable Macintosh CD image, ``oldmac.iso``.

Running under QEMU
------------------

A Quadra 800 ROM image is required.  It is copyrighted and is not distributed
with U-Boot; supply your own as ``MacROM.bin``.  Then:

.. code-block:: bash

    $ qemu-system-m68k -M q800 -bios MacROM.bin -cdrom oldmac.iso \
          -serial mon:stdio -display none

U-Boot's console appears on the emulated serial (modem) port.

Serial console
--------------

The console is the Zilog 8530 SCC channel A - the Macintosh modem port - running
at 9600 baud, 8N1.  On Quadra-class machines it lives at ``0x50F0C020``; the SPL
and U-Boot proper take the real base from the ROM where a model places it
elsewhere.

Storage (SCSI)
--------------

Mass storage is the AMD 53C9x ("ESP") SCSI controller fitted to
Quadra/Centris-class machines, driven in polled pseudo-DMA mode: the Macintosh
has no scatter/gather DMA engine for it, so data is moved a 16-bit word at a
time whenever the controller raises DRQ.  The usual ``scsi scan`` and
``scsi read`` commands work, and both the SPL and U-Boot proper load from the
bus this way.

The controller base and the DRQ source are model-specific and are chosen from
the detected machine:

* Quadra 800, LC 475, Quadra 605 (``MAC_SCSI_QUADRA``): ESP at ``0x50F10000``,
  DRQ read from the VIA2 interrupt-flag register.
* Quadra 700 (``MAC_SCSI_QUADRA2``): ESP at ``0x50F0F000``, DRQ read from a
  pseudo-DMA register in NuBus/video space.

Video console
-------------

When the ROM reports a framebuffer, U-Boot brings up a video console on it
alongside the serial console.  The address, depth, dimensions and stride come
from the Mac bootinfo, and U-Boot draws straight into the display's own memory
rather than reserving RAM for it.

At 8bpp the framebuffer holds colour-lookup-table indices, so what appears on
screen depends on the CLUT.  On the Quadra 700 and 800 U-Boot programs the DAFB
CLUT itself, so the boot logo and text come out in their intended colours
regardless of the palette the ROM happened to leave loaded.

The LC 475 / Quadra 605 report their built-in video at a logical address that is
only valid while the ROM's MMU is on.  The SPL resolves it to the real physical
framebuffer before paging is turned off, so both U-Boot's console and Linux's
early framebuffer console keep working.

Networking
----------

The on-board National DP8393x ("SONIC") Ethernet of the Quadra 700 and 800 is
supported; ``dhcp``, ``ping`` and ``tftp`` work once the link is up.  The driver
is bound only on machines that actually have the controller, so a model without
it does not end up with a dead network device.

U-Boot also scans the NuBus slots by reading each card's declaration ROM and can
enumerate a DP8390-based NuBus Ethernet card, though that card's datapath is not
yet functional.

Keyboard (ADB)
--------------

An Apple Desktop Bus keyboard attached through the VIA can be used as a U-Boot
input device, so the machine can be driven from its own keyboard and framebuffer
rather than a serial terminal.  ``stdin`` defaults to ``serial,adb-kbd``.

Machine and CPU detection
-------------------------

The peripheral layout is chosen at runtime from the Gestalt machine type the ROM
records in the Mac bootinfo, so a single image serves every supported model, and
bringing up another Quadra/Centris-class machine is mostly a table entry.

The FPU is probed rather than assumed from the model.  Many machines that shipped
with an FPU-less 68LC040 (such as the LC 475 and Quadra 605) were later upgraded
to a full 68040, so U-Boot executes a floating-point no-op under a temporary trap
handler to find out whether an FPU is actually present, and reports that to Linux
instead of guessing from the model name.
