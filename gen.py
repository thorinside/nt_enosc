import numpy as np

# Sizes (must match your C++ constants!)
sine_size = 513
cheby_tables = 16
cheby_size = 513
fold_size = 1025


class MagicSine:
    def __init__(self, freq):
        # C++: freq_ = 2π * freq
        self.omega = 2.0 * np.pi * freq
        self.sinz = 0.0
        self.cosz = 1.0

    def process(self):
        # C++ inline in dsp.hh:
        # sinz_ += omega * cosz_;
        # cosz_ -= omega * sinz_;
        self.sinz += self.omega * self.cosz
        self.cosz -= self.omega * self.sinz
        return self.sinz


# SINE TABLE: Buffer<std::pair<s1_15, s1_15>, sine_size>
def gen_sine():
    osc = MagicSine(1.0 / (sine_size - 1))
    prev = osc.process()
    out = []
    scale = (1 << 15) - 1  # for inclusive fixed-point
   
    for _ in range(sine_size):
        # quantize previous sample
        qv = int(prev * scale)
        # next raw sample
        curr = osc.process()
        qc = int(curr * scale)
        # store value and delta as floats converted back
        out.append((qv / scale, (qc - qv) / scale))
        prev = curr
    return out

# CHEBY TABLE: Buffer<Buffer<f, cheby_size>, cheby_tables>
def gen_cheby():
    cheby = np.zeros((cheby_tables, cheby_size), dtype=np.float32)
    # cheby[0] = [-1..1]
    cheby[0] = np.linspace(-1.0, 1.0, cheby_size)
    # cheby[1] = 2*cheby[0]^2 - 1
    cheby[1] = 2 * cheby[0] ** 2 - 1
    # cheby[n] = 2*cheby[0]*cheby[n-1] - cheby[n-2]
    for n in range(2, cheby_tables):
        cheby[n] = 2 * cheby[0] * cheby[n-1] - cheby[n-2]
    return cheby

# FOLD TABLE: Buffer<std::pair<f, f>, fold_size>
def gen_fold():
    out = []
    folds = 6.0
    prev = 0.0
    for i in range(fold_size):
        x = i / (fold_size - 3)
        x = folds * (2.0 * x - 1.0)
        g = 1.0 / (1.0 + abs(x))
        p = 16.0 / (2.0 * np.pi) * x * g
        while p > 1.0:
            p -= 1.0
        while p < 0.0:
            p += 1.0
        xx = -g * (x + np.sin(p * np.pi * 2.0))
        out.append((prev, xx - prev) if i else (xx, 0.0))
        prev = xx
    return out

# FOLD_MAX TABLE: Buffer<f, (fold_size-1)//2+1>
def gen_fold_max(fold):
    size = (fold_size - 1) // 2 + 1
    out = []
    maxval = 0.0
    start = (fold_size - 1) // 2
    for i in range(size):
        maxval = max(maxval, abs(fold[i+start][0]))
        out.append(0.92 / (maxval + 0.00001))
    return out

# TRIANGLES TABLE: Buffer<Buffer<f, 9>, 9>
# You must fill triangles_12ths yourself (from your actual table!)
triangles_12ths = [
    [0,3,6,7,10,12,13,15,17],
    [0,2,4,6,8,10,12,14,16],
    [0,1,3,6,7,10,12,13,17],
    [0,3,5,8,10,12,13,15,17],
    [0,1,3,6,7,9,10,12,15],
    [0,3,6,7,10,12,14,16,18],
    [0,2,5,7,10,13,15,17,19],
    [0,3,5,8,10,12,15,17,19],
    [0,2,4,6,8,10,12,14,16]
]

def gen_triangles():
    arr = np.zeros((9, 9), dtype=np.float32)
    for i in range(9):
        for j in range(9):
            arr[i][j] = triangles_12ths[i][j] / 12.0
    return arr

# === Write .cc file ===

def write_cc():
    with open("dynamic_data.cc", "w") as f:
        f.write('#include "dynamic_data.hh"\n\n')

        # SINE
        f.write(f'const Buffer<std::pair<s1_15, s1_15>, {sine_size}> DynamicData::sine = {{{{{{\n')
        for v, d in gen_sine():
            f.write(f'    {{ {v:.8f}_s1_15, {d:.8f}_s1_15 }},\n')
        f.write('}}};\n\n')

        # CHEBY
        cheby = gen_cheby()
        f.write(f'const Buffer<Buffer<f, {cheby_size}>, {cheby_tables}> DynamicData::cheby = {{{{{{\n')
        for row in cheby:
            f.write('    { ')
            f.write(', '.join(f'{x:.8f}_f' for x in row))
            f.write(' },\n')
        f.write('}}};\n\n')

        # FOLD
        fold = gen_fold()
        f.write(f'const Buffer<std::pair<f, f>, {fold_size}> DynamicData::fold = {{{{{{\n')
        for v, d in fold:
            f.write(f'    {{ {v:.8f}_f, {d:.8f}_f }},\n')
        f.write('}}};\n\n')

        # FOLD_MAX
        fold_max = gen_fold_max(fold)
        size = (fold_size - 1) // 2 + 1
        f.write(f'const Buffer<f, {size}> DynamicData::fold_max = {{{{{{\n')
        for x in fold_max:
            f.write(f'    {x:.8f}_f,\n')
        f.write('}}};\n\n')

        # TRIANGLES
        triangles = gen_triangles()
        f.write('const Buffer<Buffer<f, 9>, 9> DynamicData::triangles = {{{\n')
        for row in triangles:
            f.write('    { ')
            f.write(', '.join(f'{x:.8f}_f' for x in row))
            f.write(' },\n')
        f.write('}}};\n\n')

if __name__ == "__main__":
    write_cc()

