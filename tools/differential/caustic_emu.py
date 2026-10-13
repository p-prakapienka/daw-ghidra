"""Run functions from an original armv7 libcaustic.so under unicorn.

The library is loaded at BASE with its relative relocations applied. Every
imported symbol gets its own trampoline address, and every PLT stub is named,
so a call to an import can be told apart by name. The libm functions the
engine uses are provided by the host's libm (the one the reconstruction links
against), memset/memcpy/memmove are provided, operator new allocates, and any
other import returns at once.

A call runs one function for real. Calls it makes into the rest of the library
are stubbed (they return with r0 unchanged) unless their address range is
listed in `real`. Floats travel in core registers: the 3.2.0 (beta 7) build is
softfp.
"""

import ctypes
import ctypes.util
import os
import struct
import subprocess

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from unicorn import UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, Uc
from unicorn.arm_const import (
    UC_ARM_REG_C1_C0_2, UC_ARM_REG_CPSR, UC_ARM_REG_FPEXC, UC_ARM_REG_LR,
    UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_SP, UC_CPU_ARM_CORTEX_A15)

BASE = 0x40000000
LOW_SIZE = 0x200000       # page 0 upwards: every word points back at LOW_STUB
LOW_STUB = 0x100
IMPORTS = 0x10000         # one 4-byte trampoline per imported symbol
HEAP, HEAP_SIZE = 0x60000000, 0x08000000
STACK, STACK_SIZE = 0x7F000000, 0x00100000
RETURN = 0x1F0            # LR for the outermost call; reaching it ends the run

R = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]


def f2u(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def u2f(word):
    return struct.unpack('<f', struct.pack('<I', word & 0xFFFFFFFF))[0]


_LIBM = ctypes.CDLL(ctypes.util.find_library('m'))


def _float_function(name, arity):
    function = getattr(_LIBM, name)
    function.argtypes = [ctypes.c_float] * arity
    function.restype = ctypes.c_float
    return lambda args: f2u(function(*(u2f(word) for word in args[:arity])))


# The host's libm, called directly: the reconstruction links against the same
# library, so both sides share it and differences are the code's, not libm's.
# (numpy's float32 ufuncs are not used: they may take their own SIMD paths.)
LIBM = {name: _float_function(name, arity) for name, arity in (
    ('powf', 2), ('fmodf', 2), ('atan2f', 2),
    ('floorf', 1), ('ceilf', 1), ('sqrtf', 1), ('sinf', 1), ('cosf', 1), ('tanf', 1),
    ('expf', 1), ('logf', 1), ('log10f', 1), ('fabsf', 1), ('exp2f', 1), ('log2f', 1))}


class Library:
    def __init__(self, path=None):
        path = path or os.environ.get('CAUSTIC_SO')
        if not path or not os.path.exists(path):
            raise FileNotFoundError('set CAUSTIC_SO to an armv7 libcaustic.so')
        self.path = path
        with open(path, 'rb') as handle:
            self.elf = ELFFile(handle)
            self._load()

    def _load(self):
        elf = self.elf
        self.symbols = {}
        self.sizes = {}
        for symbol in elf.get_section_by_name('.dynsym').iter_symbols():
            if symbol.name and symbol['st_value']:
                self.symbols.setdefault(symbol.name, symbol['st_value'] & ~1)
                self.sizes.setdefault(symbol.name, symbol['st_size'])
        names = list(self.symbols)
        demangled = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True,
                                   text=True, check=True).stdout.split('\n')
        self.demangled = dict(zip(names, demangled))

        uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        uc.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_A15)
        uc.reg_write(UC_ARM_REG_C1_C0_2, uc.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))
        uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        uc.mem_map(0, LOW_SIZE)
        uc.mem_write(0, struct.pack('<I', LOW_STUB) * (LOW_SIZE // 4))

        top = max(seg['p_vaddr'] + seg['p_memsz'] for seg in elf.iter_segments()
                  if seg['p_type'] == 'PT_LOAD')
        uc.mem_map(BASE, (top + 0xFFF) & ~0xFFF)
        for seg in elf.iter_segments():
            if seg['p_type'] == 'PT_LOAD':
                uc.mem_write(BASE + seg['p_vaddr'], seg.data())

        self.imports = {}  # trampoline address -> symbol name
        slots = {}
        dynsym = elf.get_section_by_name('.dynsym')
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            for reloc in section.iter_relocations():
                kind = reloc['r_info_type']
                where = BASE + reloc['r_offset']
                if kind == 23:  # R_ARM_RELATIVE
                    word = struct.unpack('<I', uc.mem_read(where, 4))[0]
                    uc.mem_write(where, struct.pack('<I', (word + BASE) & 0xFFFFFFFF))
                elif kind in (2, 21, 22):  # ABS32, GLOB_DAT, JUMP_SLOT
                    name = dynsym.get_symbol(reloc['r_info_sym']).name
                    if name in self.symbols:
                        target = BASE + self.symbols[name]
                    else:
                        if name not in slots:
                            slots[name] = IMPORTS + 4 * len(slots)
                            self.imports[slots[name]] = name
                        target = slots[name]
                    uc.mem_write(where, struct.pack('<I', target))
        # PLT stubs: a 20-byte header, then 12 bytes per .rel.plt entry, in
        # order. A call lands on the stub, so the stub address names it.
        plt = elf.get_section_by_name('.plt')
        relplt = elf.get_section_by_name('.rel.plt')
        if plt is not None and relplt is not None:
            for index, reloc in enumerate(relplt.iter_relocations()):
                name = dynsym.get_symbol(reloc['r_info_sym']).name
                self.imports[BASE + plt['sh_addr'] + 20 + 12 * index] = name
        uc.mem_map(HEAP, HEAP_SIZE)
        uc.mem_map(STACK, STACK_SIZE)
        self.uc = uc
        self._heap = HEAP + 0x1000

    # -- memory -----------------------------------------------------------

    def alloc(self, size, fill=0):
        address = self._heap
        self._heap = (address + size + 0x3F) & ~0xF
        self.uc.mem_write(address, bytes([fill]) * size)
        return address

    def write(self, address, data):
        self.uc.mem_write(address, bytes(data))

    def read(self, address, size):
        return bytes(self.uc.mem_read(address, size))

    def u32(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def i32(self, address):
        return struct.unpack('<i', self.read(address, 4))[0]

    def f32(self, address):
        return struct.unpack('<f', self.read(address, 4))[0]

    def ints(self, address, count):
        return list(struct.unpack('<%di' % count, self.read(address, 4 * count)))

    def put_u32(self, address, value):
        self.write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def put_f32(self, address, value):
        self.write(address, struct.pack('<f', value))

    def put_ints(self, address, values):
        self.write(address, struct.pack('<%di' % len(values), *values))

    # -- symbols ----------------------------------------------------------

    def function(self, prefix):
        """(address, size) of the first function whose demangled name starts
        with prefix."""
        for name, demangled in self.demangled.items():
            if demangled.startswith(prefix) and self.sizes.get(name):
                return self.symbols[name], self.sizes[name]
        raise KeyError(prefix)

    def ranges(self, *prefixes):
        found = []
        for name, demangled in self.demangled.items():
            if self.sizes.get(name) and demangled.startswith(prefixes):
                start = self.symbols[name]
                found.append((start, start + self.sizes[name]))
        return found

    # -- calls ------------------------------------------------------------

    def call(self, prefix, args=(), real=(), limit=50_000_000):
        """Call the function named by prefix with up to four core-register
        arguments (ints, or floats already converted with f2u) and return r0."""
        start, size = self.function(prefix)
        allowed = [(BASE + start, BASE + start + size)]
        allowed += [(BASE + lo, BASE + hi) for lo, hi in real]
        uc = self.uc
        news = {BASE + self.symbols[n] for n in ('_Znwj', '_Znaj') if n in self.symbols}
        stubbed = []

        def ret(machine):
            lr = machine.reg_read(UC_ARM_REG_LR)
            cpsr = machine.reg_read(UC_ARM_REG_CPSR)
            cpsr = cpsr | 0x20 if lr & 1 else cpsr & ~0x20
            machine.reg_write(UC_ARM_REG_CPSR, cpsr)
            machine.reg_write(UC_ARM_REG_PC, lr & ~1)

        def hook(machine, address, _size, _data):
            for lo, hi in allowed:
                if lo <= address < hi:
                    return
            if address == RETURN:
                return
            regs = [machine.reg_read(r) for r in R]
            name = self.imports.get(address)
            if name in LIBM:
                machine.reg_write(UC_ARM_REG_R0, LIBM[name](regs))
            elif name == 'memset':
                self.write(regs[0], bytes([regs[1] & 0xFF]) * regs[2])
            elif name in ('memcpy', 'memmove'):
                self.write(regs[0], self.read(regs[1], regs[2]))
            elif address in news or name in ('_Znwj', '_Znaj'):
                machine.reg_write(UC_ARM_REG_R0, self.alloc(regs[0]))
            else:
                stubbed.append(name or hex(address - BASE))
            ret(machine)

        handle = uc.hook_add(UC_HOOK_CODE, hook)
        try:
            sp = STACK + STACK_SIZE - 0x1000
            uc.reg_write(UC_ARM_REG_SP, sp)
            uc.reg_write(UC_ARM_REG_LR, RETURN)
            for register, value in zip(R, args):
                uc.reg_write(register, value & 0xFFFFFFFF)
            for index, value in enumerate(args[4:]):
                self.put_u32(sp + 4 * index, value)
            uc.emu_start(BASE + start, RETURN, count=limit)
        finally:
            uc.hook_del(handle)
        if uc.reg_read(UC_ARM_REG_PC) != RETURN:
            raise RuntimeError('%s did not return (pc %#x)'
                               % (prefix, uc.reg_read(UC_ARM_REG_PC) - BASE))
        self.stubbed = stubbed
        return uc.reg_read(UC_ARM_REG_R0)
