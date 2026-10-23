"""Makes the portrait archive of the pack from the source archives
(docs/ASSETS.md, "Default portraits").

Run: python -X utf8 -E misc\\make_portraits.py OUT.zip SOURCE.zip [SOURCE.zip ...]

Each JPEG member of a source becomes "Three Kingdoms Portraits/<folder>/
<file>" in OUT.zip, fitted to 224 x 280 pixels, as a baseline JPEG at
quality 85 with 4:2:0 chroma. The members are stored, not deflated. The
script skips the folders that start with "Extras". It needs Pillow.
"""
import collections
import io
import sys
import zipfile

from PIL import Image, ImageOps

WIDTH, HEIGHT, QUALITY = 224, 280, 85


def main(out, sources):
    names = collections.Counter()
    count = 0
    total = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as archive:
        for source in sources:
            with zipfile.ZipFile(source) as z:
                for member in sorted(z.infolist(), key=lambda m: m.filename):
                    if member.is_dir() or not member.filename.lower().endswith(".jpg"):
                        continue
                    parts = member.filename.split("/")
                    folder = parts[-2] if len(parts) >= 2 else "Other"
                    file = parts[-1]
                    if folder.startswith("Extras"):
                        continue
                    picture = Image.open(io.BytesIO(z.read(member))).convert("RGB")
                    picture = ImageOps.fit(picture, (WIDTH, HEIGHT), Image.LANCZOS, centering=(0.5, 0.5))
                    data = io.BytesIO()
                    picture.save(data, "JPEG", quality=QUALITY, optimize=True, progressive=False, subsampling=2)
                    info = zipfile.ZipInfo("Three Kingdoms Portraits/%s/%s" % (folder, file), date_time=(2026, 10, 23, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_STORED
                    archive.writestr(info, data.getvalue())
                    count += 1
                    total += len(data.getvalue())
                    names[file.rsplit(".", 1)[0].split(" ")[-1]] += 1
    print("pictures %u, %u bytes, %u bytes each" % (count, total, total // max(count, 1)))
    duplicates = {name: n for name, n in names.items() if n > 1}
    print("Chinese names with two pictures:", duplicates if duplicates else "none")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    main(sys.argv[1], sys.argv[2:])
