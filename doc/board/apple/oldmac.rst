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
