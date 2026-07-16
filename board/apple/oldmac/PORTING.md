# oldmac: model support and a 68030/68040 universal CD

Status of Macintosh models under the `oldmac` board, and the design for
extending it to 68030 machines (Mac IIsi) with a single CD that boots on both
68030- and 68040-class Macs.

## Model status

| Model                | Gestalt | CPU      | Status |
|----------------------|:-------:|----------|--------|
| Quadra 800           | 35      | 68040    | Working (QEMU `q800` + hardware) |
| Quadra 700           | 22      | 68040    | Code added, **awaiting hardware test** (QUADRA2 SCSI) |
| LC 475 / Performa 47x| 89      | 68LC040  | Code added, **awaiting hardware test** (no FPU, no Ethernet) |
| Quadra 605           | 94      | 68LC040  | Alias of LC 475 |
| Mac IIsi             | 18      | 68030    | **Designed only** (this document) |

Only the Quadra 800 is emulated by QEMU, so every other model can only be
validated on real hardware.

## Hardware map (real-hardware 0x50Fxxxxx bases)

| Model | VIA2/RBV | SCSI | SCC | Ether | ADB |
|-------|----------|------|-----|-------|-----|
| Q800/Q700/LC475 | VIA2 @0x50F02000 | 53C9x ESP | Z8530 @0x50F0C020 | SONIC (Q700/Q800) | Mac-II (VIA SR) |
| Q700 (differs)  | — | ESP @**0x50F0F000**, DRQ reg @**0xF9800024** | — | SONIC @0x50F0A000 | — |
| Mac IIsi        | **RBV @0x50F26000** | **NCR 5380** regs @0x50F10000, pseudo-DMA @0x50F06000 | Z8530 (Mac-II, base from ROM) | none | **Egret** |

VIA1 is at 0x50F00000 on every model.  The framebuffer address/geometry always
comes from the ROM bootinfo, so video is model-agnostic.

## The 68030-vs-68040 problem

The boot chain runs directly on the CPU and today uses **68040-only**
instructions that are *illegal* on a 68030 (they would fault immediately):

- `cpusha` — push+invalidate the caches (68040 copyback caches).
- `movec Rn,%dtt0` / `%dtt1` — transparent translation registers.

Locations that use them (all must become CPU-aware for a universal image):

- `macboot/driver.S` — sets DTT0, `cpusha` before entering the boot block.
- `macboot/bootblock_spl.S` — sets DTT0, MMU/cache teardown, `cpusha`.
- `arch/m68k/cpu/m680x0/cpu.c` — `cpusha` in `relocate_code()` and
  `flush_cache()`/`flush_dcache_range()`.
- `arch/m68k/lib/elf.c` — MMU/cache-off before entering Linux.

### 68030 equivalents (MC68030UM)

- Transparent translation: **TT0/TT1**, written with `pmove` (not `movec`);
  layout differs from the 040 TTRs.
- Caches: the 68030 has a 256-byte writethrough I-cache and a 256-byte
  writethrough D-cache.  Control is via **CACR** (`movec` — CACR *is* valid on
  the 030) using the CI/CD (clear instruction/data cache) bits.  Because the
  D-cache is writethrough there is no dirty data to push (no `cpusha`), but the
  I-cache still needs invalidation after writing code (relocation, boot-block
  load) and the D-cache needs invalidation before reading DMA'd data.
- MMU disable: clear **TC** (`pmove`), and the TT registers.

## Universal-CD architecture

Recommended: **one universal binary**, CPU-selected at runtime.

1. Build the shared C for a common baseline (`-mcpu=68030`; a 68040 executes
   68030 integer code).  **Verified:** disassembling the current `-mcpu=68040`
   `u-boot` binary shows the *only* 68040-only instructions are 3 `cpusha` and 5
   `movec %dttN/%ittN`, all inside our own `flush_cache`/`relocate_code`/
   `bootelf_exec` asm — ordinary compiled C contains none.  So even the existing
   68040 build is nearly 030-safe; switching the baseline to `-mcpu=68030` (or
   `-m68020-40`) makes it fully so, and only the handful of asm sites below need
   a CPU branch.
2. **Detect the CPU at runtime.**  The boot block already calls `_Gestalt`; add
   `gestaltProcessorType` (68030 = 4, 68040 = 5) and publish it in the bootinfo
   (a `BI_CPUTYPE`/`BI_MAC_CPUID` record) so both the boot block and U-Boot
   proper can branch on it.  U-Boot proper can also probe directly.
3. **Branch every cache/MMU asm** on the detected CPU: the 040 path
   (`cpusha`, `movec %dttN`) and the 030 path (`movec %cacr`, `pmove %tt0/%tc`).
   Both encodings live in the binary; only the correct one executes.  GAS allows
   per-block `.cpu`/`.chip` switches to encode both.

Result: one boot block, one SPL, one U-Boot, one CD that runs on 030 and 040.

Alternative (**two-binary**): a universal (asm) boot block detects the CPU and
loads either a `-mcpu=68030` or a `-mcpu=68040` SPL/U-Boot.  More robust codegen
per CPU, at the cost of a larger CD and a build that produces two payloads.

## Phased implementation plan

**Phase 1 — universal CPU foundation — DONE (68040 path verified on QEMU q800;
68030 path written per the MC68030UM, awaiting Mac IIsi hardware):**
- Runtime CPU detection: the boot block and driver read the Gestalt processor
  type (68030 = 4, 68040 = 5); U-Boot proper uses the weak `m68k_is_68040()`
  hook, which the board overrides from the detected model (IIsi = 68030).
- `driver.S`, `bootblock_spl.S`, `cpu.c` and `elf.c` are CPU-conditional: the
  68040 keeps `cpusha` + `movec %tc/%ittN/%dttN`; the 68030 uses `CACR` cache
  clears + `pmove %tc/%tt0/%tt1`.  `bootblock_spl.S` is assembled `.cpu 68030`
  (so `pmove` is accepted) with the 68040 `movec`s hand-encoded.
- Build baseline: left at `-mcpu=68040`.  objdump confirms the compiled C has no
  68040-only opcodes, so it already runs on a 68030; switching to `-mcpu=68030`
  is a still-open robustness refinement (touches the shared m68k build).
- Verified at every step: the Quadra 800 still boots to the prompt on QEMU with
  SCSI and networking working.

**Phase 2 — Mac IIsi devices:**
- **NCR 5380 SCSI driver** (`MAC_SCSI_OLD`): registers at 0x50F10000, pseudo-DMA
  window at 0x50F06000, DRQ/IRQ via the RBV.  This is the largest new piece; it
  is a completely different chip from the 53C9x ESP.  Ref Linux
  `drivers/scsi/mac_scsi.c` + `NCR5380.c`.
- **RBV** (0x50F26000) support for the SCSI DRQ/pseudo-DMA handshake and any
  interrupt gating the boot needs.  Ref `arch/m68k/include/asm/mac_via.h`.
- Model-table row for the IIsi (Gestalt 18): SCC base from the ROM, `scsi_base`
  handled by the 5380 driver rather than `board_esp_base()`, no Ethernet.
- **Egret ADB** is optional — the serial console is the primary UI and works
  without it; the Mac-II VIA-SR ADB driver does not apply to Egret machines.

**Phase 3 — universal build + CD:** wire the chosen strategy (one universal
binary, or two payloads selected by the boot block) into `tools/mkoldmaccd` and
the defconfig so a single `make` emits one CD image for both CPU classes.

## Blockers / testing

- **No QEMU model for any 68030 Mac** (QEMU m68k only has `q800`).  The entire
  030 path — CPU cache/MMU, the 5380 SCSI driver, the RBV — can only be validated
  on **real Mac IIsi hardware**.  Expect an iterate-on-hardware loop like the one
  the Quadra 800 bring-up used (serial console first, then bisect).
- The 68040 path stays QEMU-verifiable throughout and is the regression guard:
  no Phase-1 change may break the Quadra 800 boot under QEMU.

## References

- Linux `arch/m68k/mac/config.c` (model table), `drivers/scsi/mac_scsi.c` +
  `drivers/scsi/NCR5380.c` (5380), `arch/m68k/include/asm/mac_via.h` (RBV).
- MC68030 User's Manual — TT0/TT1 (`pmove`), CACR, TC.
- EMILE `second/MMU030.c` and `enter_kernel*.S` (68030 MMU-off, reference only).
