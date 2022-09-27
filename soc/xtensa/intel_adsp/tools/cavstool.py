#!/usr/bin/env python3
# Copyright(c) 2022 Intel Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
import os
import sys
import struct
import logging
import asyncio
import time
import subprocess
import ctypes
import mmap
import argparse

start_output = True

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("cavs-fw")

PAGESZ = 4096
HUGEPAGESZ = 2 * 1024 * 1024
HUGEPAGE_FILE = "/dev/hugepages/cavs-fw-dma.tmp."

# SRAM windows.  Each appears in a 128k region starting at 512k.
#
# Window 0 is the FW_STATUS area, and 4k after that the IPC "outbox"
# Window 1 is the IPC "inbox" (host-writable memory, just 384 bytes currently)
# Window 2 is unused by this script
# Window 3 is winstream-formatted log output
OUTBOX_OFFSET   = (512 + (0 * 128)) * 1024 + 4096
INBOX_OFFSET    = (512 + (1 * 128)) * 1024
DEBUG_OFFSET    = (512 + (2 * 128)) * 1024
TRACE_OFFSET    = (512 + (3 * 128)) * 1024

# ADSPCS bits
CRST   = 0
CSTALL = 8
SPA    = 16
CPA    = 24

class HDAStream:
    # creates an hda stream with at 2 buffers of buf_len
    def __init__(self, stream_id: int, adsp):
        self.adsp = adsp
        self.stream_id = stream_id
        self.base = self.adsp.hdamem + 0x0080 + (stream_id * 0x20)
        log.info(f"Mapping registers for hda stream {self.stream_id} at base {self.base:x}")

        self.hda = Regs(self.adsp.hdamem)
        self.hda.GCAP    = 0x0000
        self.hda.GCTL    = 0x0008
        self.hda.DPLBASE = 0x0070
        self.hda.DPUBASE = 0x0074
        self.hda.SPBFCH  = 0x0700
        self.hda.SPBFCTL = 0x0704
        self.hda.PPCH    = 0x0800
        self.hda.PPCTL   = 0x0804
        self.hda.PPSTS   = 0x0808
        self.hda.SPIB = 0x0708 + stream_id*0x08
        self.hda.freeze()

        self.regs = Regs(self.base)
        self.regs.CTL  = 0x00
        self.regs.STS  = 0x03
        self.regs.LPIB = 0x04
        self.regs.CBL  = 0x08
        self.regs.LVI  = 0x0c
        self.regs.FIFOW = 0x0e
        self.regs.FIFOS = 0x10
        self.regs.FMT = 0x12
        self.regs.FIFOL= 0x14
        self.regs.BDPL = 0x18
        self.regs.BDPU = 0x1c
        self.regs.freeze()

        self.dbg0 = Regs(self.adsp.hdamem + 0x0084 + (0x20*stream_id))
        self.dbg0.DPIB = 0x00
        self.dbg0.EFIFOS = 0x10
        self.dbg0.freeze()

        self.reset()

    def __del__(self):
        self.reset()

    def config(self, buf_len: int):
        log.info(f"Configuring stream {self.stream_id}")
        self.buf_len = buf_len
        log.info("Allocating huge page and setting up buffers")
        self.mem, self.hugef, self.buf_list_addr, self.pos_buf_addr, self.n_bufs = self.setup_buf(buf_len)

        log.info("Setting buffer list, length, and stream id and traffic priority bit")
        self.regs.CTL = ((self.stream_id & 0xFF) << 20) | (1 << 18) # must be set to something other than 0?
        self.regs.BDPU = (self.buf_list_addr >> 32) & 0xffffffff
        self.regs.BDPL = self.buf_list_addr & 0xffffffff
        self.regs.CBL = buf_len
        self.regs.LVI = self.n_bufs - 1
        self.mem.seek(0)
        self.debug()
        log.info(f"Configured stream {self.stream_id}")

    def write(self, data):

        bufl = min(len(data), self.buf_len)
        log.info(f"Writing data to stream {self.stream_id}, len {bufl}, SPBFCTL {self.hda.SPBFCTL:x}, SPIB {self.hda.SPIB}")
        self.mem[0:bufl] = data[0:bufl]
        self.mem[bufl:bufl+bufl] = data[0:bufl]
        self.hda.SPBFCTL |= (1 << self.stream_id)
        self.hda.SPIB += bufl
        log.info(f"Wrote data to stream {self.stream_id}, SPBFCTL {self.hda.SPBFCTL:x}, SPIB {self.hda.SPIB}")

    def start(self):
        log.info(f"Starting stream {self.stream_id}, CTL {self.regs.CTL:x}")
        self.regs.CTL |= 2
        log.info(f"Started stream {self.stream_id}, CTL {self.regs.CTL:x}")

    def stop(self):
        log.info(f"Stopping stream {self.stream_id}, CTL {self.regs.CTL:x}")
        self.regs.CTL &= 2
        time.sleep(0.1)
        self.regs.CTL |= 1
        log.info(f"Stopped stream {self.stream_id}, CTL {self.regs.CTL:x}")

    def setup_buf(self, buf_len: int):
        (mem, phys_addr, hugef) = self.adsp.map_phys_mem(self.stream_id)

        log.info(f"Mapped 2M huge page at 0x{phys_addr:x} for buf size ({buf_len})")

        # create two buffers in the page of buf_len and mark them
        # in a buffer descriptor list for the hardware to use
        buf0_len = buf_len
        buf1_len = buf_len
        bdl_off = buf0_len + buf1_len
        # bdl is 2 (64bits, 16 bytes) per entry, we have two
        mem[bdl_off:bdl_off + 32] = struct.pack("<QQQQ",
                                                phys_addr,
                                                buf0_len,
                                                phys_addr + buf0_len,
                                                buf1_len)
        dpib_off = bdl_off+32

        # ensure buffer is initialized, sanity
        for i in range(0, buf_len*2):
            mem[i] = 0

        log.info("Filled the buffer descriptor list (BDL) for DMA.")
        return (mem, hugef, phys_addr + bdl_off, phys_addr+dpib_off, 2)

    def debug(self):
        log.debug("HDA %d: PPROC %d, CTL 0x%x, LPIB 0x%x, BDPU 0x%x, BDPL 0x%x, CBL 0x%x, LVI 0x%x",
                 self.stream_id, (self.adsp.hda.PPCTL >> self.stream_id) & 1, self.regs.CTL, self.regs.LPIB, self.regs.BDPU,
                 self.regs.BDPL, self.regs.CBL, self.regs.LVI)
        log.debug("    FIFOW %d, FIFOS %d, FMT %x, FIFOL %d, DPIB %d, EFIFOS %d",
                 self.regs.FIFOW & 0x7, self.regs.FIFOS, self.regs.FMT, self.regs.FIFOL, self.dbg0.DPIB, self.dbg0.EFIFOS)
        log.debug("    status: FIFORDY %d, DESE %d, FIFOE %d, BCIS %d",
                 (self.regs.STS >> 5) & 1, (self.regs.STS >> 4) & 1, (self.regs.STS >> 3) & 1, (self.regs.STS >> 2) & 1)

    def reset(self):
        # Turn DMA off and reset the stream.  Clearing START first is a
        # noop per the spec, but absolutely required for stability.
        # Apparently the reset doesn't stop the stream, and the next load
        # starts before it's ready and kills the load (and often the DSP).
        # The sleep too is required, on at least one board (a fast
        # chromebook) putting the two writes next each other also hangs
        # the DSP!
        log.info(f"Resetting stream {self.stream_id}")
        self.debug()
        self.regs.CTL &= ~2 # clear START
        time.sleep(0.1)
        # set enter reset bit
        self.regs.CTL = 1
        while (self.regs.CTL & 1) == 0: pass
        # clear enter reset bit to exit reset
        self.regs.CTL = 0
        while (self.regs.CTL & 1) == 1: pass

        log.info(f"Disable SPIB and set position 0 of stream {self.stream_id}")
        self.hda.SPBFCTL = 0
        self.hda.SPIB = 0

        #log.info("Setting dma position buffer and enable it")
        #self.hda.DPUBASE = self.pos_buf_addr >> 32 & 0xffffffff
        #self.hda.DPLBASE = self.pos_buf_addr & 0xfffffff0 | 1

        log.info(f"Enabling dsp capture (PROCEN) of stream {self.stream_id}")
        self.hda.PPCTL |= (1 << self.stream_id)

        self.debug()
        log.info(f"Reset stream {self.stream_id}")

class ADSPTool():
    def __init__(self, readonly) -> None:
        self.cavs15 = False
        self.cavs18 = False
        self.cavs25 = False
        self.ipc_timestamp = 0
        self.hda_streams = dict()
        self.readonly = readonly

    def mask(self, bit):
        if self.cavs25:
            return 0b1 << bit
        if self.cavs18:
            return 0b1111 << bit
        if self.cavs15:
            return 0b11 << bit

    def load_firmware(self, fw_file):
        try:
            fw_bytes = open(fw_file, "rb").read()
        except Exception as e:
            log.error(f"Could not read firmware file: `{fw_file}'")
            log.error(e)
            sys.exit(1)

        (magic, sz) = struct.unpack("4sI", fw_bytes[0:8])
        if magic == b'XMan':
            log.info(f"Trimming {sz} bytes of extended manifest")
            fw_bytes = fw_bytes[sz:len(fw_bytes)]

        hda = self.hda
        # This actually means "enable access to BAR4 registers"!
        self.hda.PPCTL |= (1 << 30) # GPROCEN, "global processing enable"

        log.info("Resetting HDA device")

        hda.GCTL = 0
        while hda.GCTL & 1: pass
        hda.GCTL = 1
        while not hda.GCTL & 1: pass

        dsp = self.dsp
        log.info(f"Stalling and Resetting DSP cores, ADSPCS = 0x{dsp.ADSPCS:x}")
        dsp.ADSPCS |= self.mask(CSTALL)
        dsp.ADSPCS |= self.mask(CRST)
        while (dsp.ADSPCS & self.mask(CRST)) == 0: pass

        log.info(f"Powering down DSP cores, ADSPCS = 0x{dsp.ADSPCS:x}")
        dsp.ADSPCS &= ~self.mask(SPA)
        while dsp.ADSPCS & self.mask(CPA): pass

        hda_ostream_id = self.hda_ostream_id
        log.info(f"Configuring HDA stream {hda_ostream_id} to transfer firmware image")
        (buf_list_addr, num_bufs) = self.setup_dma_mem(fw_bytes)

        sd = self.sd
        sd.CTL = 1
        while (sd.CTL & 1) == 0: pass
        sd.CTL = 0
        while (sd.CTL & 1) == 1: pass
        sd.CTL = (1 << 20) # Set stream ID to anything non-zero
        sd.BDPU = (buf_list_addr >> 32) & 0xffffffff
        sd.BDPL = buf_list_addr & 0xffffffff
        sd.CBL = len(fw_bytes)
        sd.LVI = num_bufs - 1
        hda.PPCTL |= (1 << hda_ostream_id)

        # SPIB ("Software Position In Buffer") is an Intel HDA extension
        # that puts a transfer boundary into the stream beyond which the
        # other side will not read.  The ROM wants to poll on a "buffer
        # full" bit on the other side that only works with this enabled.
        hda.SPBFCTL |= (1 << hda_ostream_id)
        hda.SD_SPIB = len(fw_bytes)

        # Start DSP.  Host needs to provide power to all cores on 1.5
        # (which also starts them) and 1.8 (merely gates power, DSP also
        # has to set PWRCTL). On 2.5 where the DSP has full control,
        # and only core 0 is set.
        log.info(f"Starting DSP, ADSPCS = 0x{dsp.ADSPCS:x}")
        dsp.ADSPCS = self.mask(SPA)
        while (dsp.ADSPCS & self.mask(CPA)) == 0: pass

        log.info(f"Unresetting DSP cores, ADSPCS = 0x{dsp.ADSPCS:x}")
        dsp.ADSPCS &= ~self.mask(CRST)
        while (dsp.ADSPCS & 1) != 0: pass

        log.info(f"Running DSP cores, ADSPCS = 0x{dsp.ADSPCS:x}")
        dsp.ADSPCS &= ~self.mask(CSTALL)

        # Wait for the ROM to boot and signal it's ready.  This not so short
        # sleep seems to be needed; if we're banging on the memory window
        # during initial boot (before/while the window control registers
        # are configured?) the DSP hardware will hang fairly reliably.
        log.info(f"Wait for ROM startup, ADSPCS = 0x{dsp.ADSPCS:x}")
        time.sleep(1)
        while (dsp.SRAM_FW_STATUS >> 24) != 5: pass

        # Send the DSP an IPC message to tell the device how to boot.
        # Note: with cAVS 1.8+ the ROM receives the stream argument as an
        # index within the array of output streams (and we always use the
        # first one by construction).  But with 1.5 it's the HDA index,
        # and depends on the number of input streams on the device.
        stream_idx = hda_ostream_id if self.cavs15 else 0
        ipcval = (  (1 << 31)            # BUSY bit
                    | (0x01 << 24)       # type = PURGE_FW
                    | (1 << 14)          # purge_fw = 1
                    | (stream_idx << 9)) # dma_id
        log.info(f"Sending IPC command, HIPIDR = 0x{ipcval:x}")
        dsp.HIPCIDR = ipcval

        log.info(f"Starting DMA, FW_STATUS = 0x{dsp.SRAM_FW_STATUS:x}")
        sd.CTL |= 2 # START flag

        self.wait_fw_entered(timeout_s=2)

        # Turn DMA off and reset the stream.  Clearing START first is a
        # noop per the spec, but absolutely required for stability.
        # Apparently the reset doesn't stop the stream, and the next load
        # starts before it's ready and kills the load (and often the DSP).
        # The sleep too is required, on at least one board (a fast
        # chromebook) putting the two writes next each other also hangs
        # the DSP!
        sd.CTL &= ~2 # clear START
        time.sleep(0.1)
        sd.CTL |= 1
        log.info(f"cAVS firmware load complete")

    def map_regs(self):
        p = runx(f"grep -iEl 'PCI_CLASS=40(10|38)0' /sys/bus/pci/devices/*/uevent")
        pcidir = os.path.dirname(p)

        # Platform/quirk detection.  ID lists cribbed from the SOF kernel driver

        did = int(open(f"{pcidir}/device").read().rstrip(), 16)
        self.cavs15 = did in [ 0x5a98, 0x1a98, 0x3198 ]
        self.cavs18 = did in [ 0x9dc8, 0xa348, 0x02c8, 0x06c8, 0xa3f0 ]
        self.cavs25 = did in [ 0xa0c8, 0x43c8, 0x4b55, 0x4b58, 0x7ad0, 0x51c8 ]

        # Check sysfs for a loaded driver and remove it
        if os.path.exists(f"{pcidir}/driver"):
            mod = os.path.basename(os.readlink(f"{pcidir}/driver/module"))
            found_msg = f"Existing driver \"{mod}\" found"
            if self.readonly:
                log.info(found_msg)
            else:
                log.warning(found_msg + ", unloading module")
                runx(f"rmmod -f {mod}")
                # Disengage runtime power management so the kernel doesn't put it to sleep
                log.info(f"Forcing {pcidir}/power/control to always 'on'")
                with open(f"{pcidir}/power/control", "w") as ctrl:
                    ctrl.write("on")

        # Make sure PCI memory space access and busmastering are enabled.
        # Also disable interrupts so as not to confuse the kernel.
        with open(f"{pcidir}/config", "wb+") as cfg:
            cfg.seek(4)
            cfg.write(b'\x06\x04')

        # Standard HD Audio Registers
        (self.hdamem, _) = self.bar_map(pcidir, 0)
        hda = Regs(self.hdamem)
        hda.GCAP    = 0x0000
        hda.GCTL    = 0x0008
        hda.SPBFCTL = 0x0704
        hda.PPCTL   = 0x0804

        # Find the ID of the first output stream
        hda_ostream_id = (hda.GCAP >> 8) & 0x0f # number of input streams
        log.info(f"Selected output stream {hda_ostream_id} (GCAP = 0x{hda.GCAP:x})")
        hda.SD_SPIB = 0x0708 + (8 * hda_ostream_id)
        self.hda_ostream_id = hda_ostream_id
        hda.freeze()
        self.hda = hda

        # Standard HD Audio Stream Descriptor
        sd = Regs(self.hdamem + 0x0080 + (hda_ostream_id * 0x20))
        sd.CTL  = 0x00
        sd.CBL  = 0x08
        sd.LVI  = 0x0c
        sd.BDPL = 0x18
        sd.BDPU = 0x1c
        sd.freeze()
        self.sd = sd

        # Intel Audio DSP Registers

        (self.bar4_mem, self.bar4_mmap) = self.bar_map(pcidir, 4)
        dsp = Regs(self.bar4_mem)
        dsp.ADSPCS         = 0x00004
        dsp.HIPCTDR        = 0x00040 if self.cavs15 else 0x000c0
        dsp.HIPCTDA        =                        0x000c4 # 1.8+ only
        dsp.HIPCTDD        = 0x00044 if self.cavs15 else 0x000c8
        dsp.HIPCIDR        = 0x00048 if self.cavs15 else 0x000d0
        dsp.HIPCIDA        =                        0x000d4 # 1.8+ only
        dsp.HIPCIDD        = 0x0004c if self.cavs15 else 0x000d8
        dsp.SRAM_FW_STATUS = 0x80000 # Start of first SRAM window
        dsp.freeze()

        self.dsp = dsp


    async def ipc_delay_done(self):
        await asyncio.sleep(0.1)
        self.dsp.HIPCTDA = 1<<31

    # Super-simple command language, driven by the test code on the DSP
    def ipc_command(self, data, ext_data):
        send_msg = False
        done = True
        log.debug ("ipc data %d, ext_data %x", data, ext_data)
        if data == 0: # noop, with synchronous DONE
            pass
        elif data == 1: # async command: signal DONE after a delay (on 1.8+)
            if not self.cavs15:
                done = False
                asyncio.ensure_future(self.ipc_delay_done())
        elif data == 2: # echo back ext_data as a message command
            send_msg = True
        elif data == 3: # set ADSPCS
            self.dsp.ADSPCS = ext_data
        elif data == 4: # echo back microseconds since last timestamp command
            t = round(time.time() * 1e6)
            ext_data = t - self.ipc_timestamp
            self.ipc_timestamp = t
            send_msg = True
        elif data == 5: # copy word at outbox[ext_data >> 16] to inbox[ext_data & 0xffff]
            src = OUTBOX_OFFSET + 4 * (ext_data >> 16)
            dst =  INBOX_OFFSET + 4 * (ext_data & 0xffff)
            for i in range(4):
                self.bar4_mmap[dst + i] = self.bar4_mmap[src + i]
        elif data == 6: # HDA RESET (init if not exists)
            stream_id = ext_data & 0xff
            if stream_id in self.hda_streams:
                self.hda_streams[stream_id].reset()
            else:
                hda_str = HDAStream(stream_id, self)
                self.hda_streams[stream_id] = hda_str
        elif data == 7: # HDA CONFIG
            stream_id = ext_data & 0xFF
            buf_len = ext_data >> 8 & 0xFFFF
            hda_str = self.hda_streams[stream_id]
            hda_str.config(buf_len)
        elif data == 8: # HDA START
            stream_id = ext_data & 0xFF
            self.hda_streams[stream_id].start()
            self.hda_streams[stream_id].mem.seek(0)

        elif data == 9: # HDA STOP
            stream_id = ext_data & 0xFF
            self.hda_streams[stream_id].stop()
        elif data == 10: # HDA VALIDATE
            stream_id = ext_data & 0xFF
            hda_str = self.hda_streams[stream_id]
            hda_str.debug()
            is_ramp_data = True
            hda_str.mem.seek(0)
            for (i, val) in enumerate(hda_str.mem.read(256)):
                if i != val:
                    is_ramp_data = False
                # log.info("stream[%d][%d]: %d", stream_id, i, val) # debug helper
            log.info("Is ramp data? " + str(is_ramp_data))
            ext_data = int(is_ramp_data)
            log.info(f"Ext data to send back on ramp status {ext_data}")
            send_msg = True
        elif data == 11: # HDA HOST OUT SEND
            stream_id = ext_data & 0xff
            buf = bytearray(256)
            for i in range(0, 256):
                buf[i] = i
            self.hda_streams[stream_id].write(buf)
        elif data == 12: # HDA PRINT
            stream_id = ext_data & 0xFF
            buf_len = ext_data >> 8 & 0xFFFF
            hda_str = self.hda_streams[stream_id]
            # check for wrap here
            pos = hda_str.mem.tell()
            read_lens = [buf_len, 0]
            if pos + buf_len >= hda_str.buf_len*2:
                read_lens[0] = hda_str.buf_len*2 - pos
                read_lens[1] = buf_len - read_lens[0]
            # validate the read lens
            assert (read_lens[0] + pos) <= (hda_str.buf_len*2)
            assert read_lens[0] % 128 == 0
            assert read_lens[1] % 128 == 0
            buf_data0 = hda_str.mem.read(read_lens[0])
            hda_msg0 = buf_data0.decode("utf-8", "replace")
            sys.stdout.write(hda_msg0)
            if read_lens[1] != 0:
                hda_str.mem.seek(0)
                buf_data1 = hda_str.mem.read(read_lens[1])
                hda_msg1 = buf_data1.decode("utf-8", "replace")
                sys.stdout.write(hda_msg1)
            pos = hda_str.mem.tell()
            sys.stdout.flush()
        else:
            log.warning(f"cavstool: Unrecognized IPC command 0x{data:x} ext 0x{ext_data:x}")
            if not self.fw_is_alive():
                if self.readonly:
                    log.info("DSP power seems off")
                    self.wait_fw_entered()
                else:
                    log.warning("DSP power seems off?!")
                    time.sleep(2) # potential spam reduction

                return

        self.dsp.HIPCTDR = 1<<31 # Ack local interrupt, also signals DONE on v1.5
        if self.cavs18:
            time.sleep(0.01) # Needed on 1.8, or the command below won't send!

        if done and not self.cavs15:
            self.dsp.HIPCTDA = 1<<31 # Signal done
        if send_msg:
            self.dsp.HIPCIDD = ext_data
            self.dsp.HIPCIDR = (1<<31) | ext_data

    def setup_dma_mem(self, fw_bytes):
        (mem, phys_addr, _) = self.map_phys_mem(self.hda_ostream_id)
        mem[0:len(fw_bytes)] = fw_bytes

        log.info("Mapped 2M huge page at 0x%x to contain %d bytes of firmware"
            % (phys_addr, len(fw_bytes)))

        # HDA requires at least two buffers be defined, but we don't care about
        # boundaries because it's all a contiguous region. Place a vestigial
        # 128-byte (minimum size and alignment) buffer after the main one, and put
        # the 4-entry BDL list into the final 128 bytes of the page.
        buf0_len = HUGEPAGESZ - 2 * 128
        buf1_len = 128
        bdl_off = buf0_len + buf1_len
        mem[bdl_off:bdl_off + 32] = struct.pack("<QQQQ",
                                                phys_addr, buf0_len,
                                                phys_addr + buf0_len, buf1_len)
        log.info("Filled the buffer descriptor list (BDL) for DMA.")
        return (phys_addr + bdl_off, 2)

    global_mmaps = [] # protect mmap mappings from garbage collection!

    # Maps 2M of contiguous memory using a single page from hugetlbfs,
    # then locates its physical address for use as a DMA buffer.
    def map_phys_mem(self, stream_id):
        # Make sure hugetlbfs is mounted (not there on chromeos)
        os.system("mount | grep -q hugetlbfs ||"
                + " (mkdir -p /dev/hugepages; "
                + "  mount -t hugetlbfs hugetlbfs /dev/hugepages)")

        # Ensure the kernel has enough budget for one new page
        free = int(runx("awk '/HugePages_Free/ {print $2}' /proc/meminfo"))
        if free == 0:
            tot = 1 + int(runx("awk '/HugePages_Total/ {print $2}' /proc/meminfo"))
            os.system(f"echo {tot} > /proc/sys/vm/nr_hugepages")

        hugef_name = HUGEPAGE_FILE + str(stream_id)
        hugef = open(hugef_name, "w+")
        hugef.truncate(HUGEPAGESZ)
        mem = mmap.mmap(hugef.fileno(), HUGEPAGESZ)
        log.info("type of mem is %s", str(type(mem)))
        self.global_mmaps.append(mem)
        os.unlink(hugef_name)

        # Find the local process address of the mapping, then use that to extract
        # the physical address from the kernel's pagemap interface.  The physical
        # page frame number occupies the bottom bits of the entry.
        mem[0] = 0 # Fault the page in so it has an address!
        vaddr = ctypes.addressof(ctypes.c_int.from_buffer(mem))
        vpagenum = vaddr >> 12
        pagemap = open("/proc/self/pagemap", "rb")
        pagemap.seek(vpagenum * 8)
        pent = pagemap.read(8)
        paddr = (struct.unpack("Q", pent)[0] & ((1 << 55) - 1)) * PAGESZ
        pagemap.close()
        return (mem, paddr, hugef)

    # Maps a PCI BAR and returns the in-process address
    def bar_map(self, pcidir, barnum):
        f = open(pcidir + "/resource" + str(barnum), "r+")
        mm = mmap.mmap(f.fileno(), os.fstat(f.fileno()).st_size)
        self.global_mmaps.append(mm)
        log.info("Mapped PCI bar %d of length %d bytes."
                % (barnum, os.fstat(f.fileno()).st_size))
        return (ctypes.addressof(ctypes.c_int.from_buffer(mm)), mm)

    def fw_is_alive(self):
        return self.dsp.SRAM_FW_STATUS & ((1 << 28) - 1) == 5 # "FW_ENTERED"

    def wait_fw_entered(self, timeout_s=None):
        log.info("Waiting %s for firmware handoff, FW_STATUS = 0x%x",
                "forever" if timeout_s is None else f"{timeout_s} seconds",
                self.dsp.SRAM_FW_STATUS)
        hertz = 100
        attempts = None if timeout_s is None else timeout_s * hertz
        while True:
            alive = self.fw_is_alive()
            if alive:
                break
            if attempts is not None:
                attempts -= 1
                if attempts < 0:
                    break
            time.sleep(1 / hertz)

        if not alive:
            log.warning("Load failed?  FW_STATUS = 0x%x", self.dsp.SRAM_FW_STATUS)
        else:
            log.info("FW alive, FW_STATUS = 0x%x", self.dsp.SRAM_FW_STATUS)

# Syntactic sugar to make register block definition & use look nice.
# Instantiate from a base address, assign offsets to (uint32) named registers as
# fields, call freeze(), then the field acts as a direct alias for the register!
class Regs:
    def __init__(self, base_addr):
        vars(self)["base_addr"] = base_addr
        vars(self)["ptrs"] = {}
        vars(self)["frozen"] = False
    def freeze(self):
        vars(self)["frozen"] = True
    def __setattr__(self, name, val):
        if not self.frozen and name not in self.ptrs:
            addr = self.base_addr + val
            self.ptrs[name] = ctypes.c_uint32.from_address(addr)
        else:
            self.ptrs[name].value = val
    def __getattr__(self, name):
        return self.ptrs[name].value

def runx(cmd):
    return subprocess.check_output(cmd, shell=True).decode().rstrip()


class MemWin():
    def __init__(self, bar4_mmap, window) -> None:
        self.window = window
        self.bar4_mmap = bar4_mmap

    def read(self, start, length):
        # This SHOULD be just "mem[start:start+length]", but slicing an mmap
        # array seems to be unreliable on one of my machines (python 3.6.9 on
        # Ubuntu 18.04).  Read out bytes individually.
        try:
            out = b''.join(self.bar4_mmap[self.window + x].to_bytes(1, 'little')
                            for x in range(start, start + length))

            #v = memoryview(self.bar4_mmap)
            #out = bytes(v[self.window + start:self.window + start + length])
            return out
        except IndexError as ie:
            # A FW in a bad state may cause winstream garbage
            log.error("IndexError in bar4_mmap[%d + %d]", self.window, start)
            log.error("bar4_mmap.size()=%d", self.bar4_mmap.size())
            raise ie

class Winstream(MemWin):

    def __init__(self, bar4_mmap, win, args) -> None:
        self.no_history = args.no_history
        super().__init__(bar4_mmap, win)

    def header(self):
        header = struct.unpack("<IIII", self.read(0, 16))
        return header

    # Python implementation of the same algorithm in sys_winstream_read(),
    # see there for details.
    def read_data(self, last_seq):
        while True:
            (wlen, start, end, seq) = self.header()
            if last_seq == 0:
                last_seq = seq if self.no_history else (seq - ((end - start) % wlen))
            if seq == last_seq or start == end:
                return (seq, "")
            behind = seq - last_seq
            if behind > ((end - start) % wlen):
                return (seq, "")
            copy = (end - behind) % wlen
            suffix = min(behind, wlen - copy)
            result = self.read(16 + copy, suffix)
            if suffix < behind:
                result += self.read(16, behind - suffix)
            (wlen, start1, end, seq1) = self.header()
            if start1 == start and seq1 == seq:
                # Best effort attempt at decoding, replacing unusable characters
                # Found to be useful when it really goes wrong
                return (seq, result.decode("utf-8", "replace"))


class Mtrace(MemWin):
    def __init__(self, bar4_mmap, win, args) -> None:
        self.no_history = args.no_history
        super().__init__(bar4_mmap, win)

    def header(self):
        header = struct.unpack("<IIII", self.read(8192, 16))
        return header

    def read_data(self, last_seq):
        while True:
            (data_len, a, seq, c) = self.header()
            print(seq)
            if last_seq == 0:
                last_seq = seq
            time.sleep(0.3)
            if last_seq == seq:
                return (seq, "")
            result = self.read(8192 + 16 + last_seq, seq)
            #os.write(sys.stdout.fileno(), data)
            return (seq, result.decode("utf-8", "replace"))

def logger(args):
    last_seq = 0
    win = None
    if args.window == '0':
        win = TRACE_OFFSET
    elif args.window == '2':
        win = DEBUG_OFFSET

    if args.stream == 'winstream':
        stream = Winstream(adsp.bar4_mmap, win, args)
    elif args.stream == 'mtrace':
        stream = Mtrace(adsp.bar4_mmap, win, args)

    while start_output is True:
        time.sleep(0.03)
        (last_seq, output) = stream.read_data(last_seq)
        if output:
            sys.stdout.write(output)
            sys.stdout.flush()


def load(args):
    if not args.fw_file:
        log.error("Firmware file argument missing")
        sys.exit(1)

    adsp.load_firmware(args.fw_file)
    time.sleep(0.1)
    if not args.quiet:
        sys.stdout.write("--\n")


    #await asyncio.sleep(0.03)
    if adsp.dsp.HIPCIDA & 0x80000000:
        adsp.dsp.HIPCIDA = 1<<31 # must ACK any DONE interrupts that arrive!
    if adsp.dsp.HIPCTDR & 0x80000000:
        adsp.ipc_command(adsp.dsp.HIPCTDR & ~0x80000000, adsp.dsp.HIPCTDD)

def parse_args():
    ap = argparse.ArgumentParser(description="DSP loader/logger tool")

    ap.add_argument("-q", "--quiet", action="store_true",
                    help="No loader output, just DSP logging")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="More loader output, DEBUG logging level")

    sub_parsers = ap.add_subparsers(help='commands', dest='action')
    parser_log = sub_parsers.add_parser('log', help='Show logging console')
    parser_log.add_argument("-n", "--no-history", action="store_true",
                    help="No current log buffer at start, just new output")

    parser_log.add_argument("-w", "--window")
    parser_log.add_argument("-s", "--stream")

    parser_load = sub_parsers.add_parser('load', help='Load firmware')
    parser_load.add_argument("fw_file", nargs="?", help="Firmware file")

    return ap.parse_args()

if __name__ == "__main__":

    args = parse_args()
    if args.quiet:
        log.setLevel(logging.WARN)
    elif args.verbose:
        log.setLevel(logging.DEBUG)

    try:
        adsp  = ADSPTool(args)
        adsp.map_regs()
    except Exception as e:
        log.error("Could not map device in sysfs; run as root?")
        log.error(e)
        sys.exit(1)

    log.info(f"Detected cAVS {'1.5' if adsp.cavs15 else '1.8+'} hardware")

    adsp.wait_fw_entered()

    try:
        if args.action == 'load':
            load(args)
        elif args.action == 'log':
            logger(args)
    except KeyboardInterrupt:
        start_output = False
