from PIL import Image
import struct, io, os

src = "/Users/leeson/Desktop/ProcessGuardCpp/res/shield.png"
out = "/Users/leeson/Desktop/ProcessGuardCpp/res/ProcessGuard.ico"

img = Image.open(src).convert("RGBA")

sizes = [256, 48, 32, 16]
imgs = [img.resize((s, s), Image.LANCZOS) for s in sizes]

entries = []

for s, im in zip(sizes, imgs):
    if s == 256:
        buf = io.BytesIO()
        im.save(buf, format="PNG")
        entries.append(buf.getvalue())
    else:
        w = h = s
        bih = struct.pack(
            '<IIIHHIIIIII',
            40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0
        )
        pixels = list(im.getdata())
        rows = [pixels[y*w:(y+1)*w] for y in range(h)]
        rows.reverse()
        raw = b"".join(bytes([b, g, r, a]) for row in rows for r, g, b, a in row)
        row_sz = ((w + 31) // 32) * 4
        and_mask = b""
        for row in rows:
            bits = 0
            byte_data = b""
            for j, (r, g, b, a) in enumerate(row):
                if a < 128:
                    bits |= (1 << (7 - (j % 8)))
                if j % 8 == 7:
                    byte_data += bytes([bits])
                    bits = 0
            if w % 8 != 0:
                byte_data += bytes([bits])
            pad = row_sz - len(byte_data)
            and_mask += byte_data + b'\x00' * pad
        entries.append(bih + raw + and_mask)

n = len(sizes)
data_offset = 6 + 16 * n
offsets = []
cur = data_offset
for d in entries:
    offsets.append(cur)
    cur += len(d)

with open(out, "wb") as f:
    f.write(struct.pack('<HHH', 0, 1, n))
    for s, d, off in zip(sizes, entries, offsets):
        w = 0 if s == 256 else s
        h = 0 if s == 256 else s
        f.write(struct.pack('<BBBBHHII', w, h, 0, 0, 1, 32, len(d), off))
    for d in entries:
        f.write(d)

print(f"OK: {out} ({os.path.getsize(out)} bytes, sizes={sizes})")
