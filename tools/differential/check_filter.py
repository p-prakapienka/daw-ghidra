"""FixedPointSVFilter: the original against the reconstruction.

Usage (from the repository root):
    CAUSTIC_SO=/path/to/armv7/libcaustic.so python3 tools/differential/check_filter.py \
        build/differential-driver

Compares the constructor's tables and initial state for both flags, then
ProcessCV over a set of scenarios: every mode, cutoff and resonance across
their ranges and past them, envelope inverted or not, gate open and closed,
mono and stereo, with and without modulation. Prints each mismatch and exits
non-zero if there is any.
"""

import itertools
import subprocess
import sys

import caustic_emu as ce

FILTER_SIZE = 0x844
ADSR_SIZE = 0x64
CV_SIZE = 0x60


def input_sample(index):
    return ((index * 37) % 512 - 256) << 15


def modulation_sample(kind, index):
    return 0 if kind == 0 else index * 65536 - 0x400000


class Driver:
    def __init__(self, path):
        self.process = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        text=True)

    def run(self, line):
        self.process.stdin.write(line + '\n')
        self.process.stdin.flush()
        result = {}
        while True:
            reply = self.process.stdout.readline().split()
            if not reply or reply[0] == 'end':
                return result
            result[reply[0]] = [int(word) for word in reply[1:]]


def word(value):
    return '%08x' % (ce.f2u(value) if isinstance(value, float) else value)


def original_filter(lib, flag):
    address = lib.alloc(FILTER_SIZE)
    lib.call('FixedPointSVFilter::FixedPointSVFilter(bool)', (address, flag))
    return address


def state(lib, address):
    return [lib.i32(address + 0x818), lib.i32(address + 0x82C), lib.i32(address + 0x830)]


def compare(label, ours, theirs, problems):
    if ours == theirs:
        return
    first = next(i for i, (a, b) in enumerate(zip(ours, theirs)) if a != b) \
        if len(ours) == len(theirs) else None
    problems.append('%s: differs%s (ours %s, original %s)' % (
        label, '' if first is None else ' first at %d' % first,
        ours[first] if first is not None else len(ours),
        theirs[first] if first is not None else len(theirs)))


def check_constructor(lib, driver, problems):
    for flag in (1, 0):
        ours = driver.run('ctor %d' % flag)
        address = original_filter(lib, flag)
        compare('ctor(%d) cutoff table' % flag, ours['cutoff'], lib.ints(address + 0x0C, 256),
                problems)
        compare('ctor(%d) damping table' % flag, ours['damping'], lib.ints(address + 0x40C, 256),
                problems)
        compare('ctor(%d) state' % flag, ours['state'], state(lib, address), problems)


def check_process(lib, driver, scenario, problems):
    (flag, mode, cutoff, resonance, invert, envelope, voice, gate, channels, modulation) = scenario
    attack, decay, sustain, release = envelope
    hertz, tracking, advance = voice
    samples = 128
    line = ' '.join(['process'] + [word(v) for v in (
        flag, mode, cutoff, resonance, invert, attack, decay, sustain, release, hertz, tracking,
        advance, gate, samples, channels, modulation)])
    ours = driver.run(line)

    adsr = lib.alloc(ADSR_SIZE)
    adsr_code = lib.ranges('ADSR::')
    lib.call('ADSR::ADSR()', (adsr,), real=adsr_code)
    for offset, value in zip((0x04, 0x08, 0x0C, 0x10), (attack, decay, sustain, release)):
        lib.put_f32(adsr + offset, value)
    lib.call('ADSR::OnParamModified()', (adsr,), real=adsr_code)

    values = lib.alloc(4 * samples)
    lib.put_ints(values, [modulation_sample(modulation, i) for i in range(samples)])
    source = lib.alloc(0x10)
    lib.put_u32(source + 0x0C, values)

    address = original_filter(lib, flag)
    lib.put_u32(address + 0x08, mode)
    lib.put_f32(address + 0x838, cutoff)
    lib.put_f32(address + 0x83C, resonance)
    lib.write(address + 0x840, bytes([invert]))
    lib.put_u32(address + 0x80C, source)
    lib.put_u32(address + 0x814, adsr)

    voltage = lib.alloc(CV_SIZE)
    lib.write(voltage, bytes(ours['cv']))

    slots = samples * (2 if channels == 2 else 1)
    buffer = lib.alloc(4 * slots)
    lib.put_ints(buffer, [input_sample(i) for i in range(slots)])
    lib.call('FixedPointSVFilter::ProcessCV', (address, voltage, buffer, samples, channels),
             real=adsr_code)
    if lib.stubbed:
        problems.append('%s: original called stubbed %s' % (scenario, sorted(set(lib.stubbed))))

    compare('%s output' % (scenario,), ours['out'], lib.ints(buffer, slots), problems)
    compare('%s state' % (scenario,), ours['state'], state(lib, address), problems)


def scenarios():
    envelopes = [(0.01, 0.05, 0.6, 0.1), (0.5, 0.0, 1.0, 0.0)]
    voices = [(440.0, 0.0, 1000), (110.0, 0.7, 200), (1760.0, 1.0, 5000)]
    base = list(itertools.product(
        (1,), (1, 2, 3), (0.02, 0.3, 0.8, 1.0), (0.0, 0.5, 0.9), (0, 1),
        envelopes[:1], voices[:1], (1,), (1,), (0,)))
    extra = [
        (0, 1, 0.5, 0.5, 0, envelopes[0], voices[0], 1, 1, 0),   # standard variant
        (1, 0, 0.5, 0.5, 0, envelopes[0], voices[0], 1, 1, 0),   # bypass
        (1, 7, 0.5, 0.5, 0, envelopes[0], voices[0], 1, 1, 0),   # unknown mode
        (1, 1, 1.5, 1.5, 0, envelopes[0], voices[0], 1, 1, 0),   # past both ranges
        (1, 1, 0.5, -0.5, 0, envelopes[0], voices[0], 1, 1, 0),  # negative resonance
        (1, 2, 0.6, 0.4, 0, envelopes[0], voices[1], 1, 2, 0),   # stereo, tracking
        (1, 3, 0.6, 0.4, 1, envelopes[1], voices[2], 0, 1, 0),   # released
        (1, 1, 0.4, 0.7, 0, envelopes[0], voices[0], 1, 1, 1),   # modulation
        (1, 1, 0.4, 0.7, 1, envelopes[1], voices[1], 1, 2, 1),
    ]
    return base + extra


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    lib = ce.Library()
    driver = Driver(sys.argv[1])
    problems = []
    check_constructor(lib, driver, problems)
    cases = scenarios()
    for scenario in cases:
        check_process(lib, driver, scenario, problems)
    for problem in problems:
        print(problem)
    print('%d ProcessCV scenarios, %d mismatches' % (len(cases), len(problems)))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
