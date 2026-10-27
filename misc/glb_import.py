"""Imports a 3D model of a base from a binary glTF file (GLB) into the
models folder of a pack.

The renderer draws the model of a base kind in place of the art card of
that kind (docs/ASSETS.md, "Base models"). A card faces the camera, so it
turns with the view. A model stays fixed on the map. This script does
these steps:

1. It reads the scene of the GLB. It applies the transform of each node
   to the primitives of the mesh of that node.
2. It keeps the position, the normal and the first texture coordinate of
   each vertex, and the triangles. If the file has no normals, the script
   makes vertex normals from the faces.
3. It turns the model around Y by the --yaw angle, if you give one.
4. It moves the model so that the centre of its footprint is at x = 0,
   z = 0 and its lowest point is at y = 0. It scales the model so that the
   larger side of the footprint is one unit. The scene scales the model to
   the width of the base kind.
5. It writes data/<pack>/models/<name>.hxm, the flat mesh file of the
   engine: "HXM1", the vertex count, the index count, 8 floats for each
   vertex (position, normal, uv), and the indices as u32.
6. It writes data/<pack>/models/<name>.png, the base colour texture of the
   first material, at TEXTURE_MAX pixels at most. The normal map and the
   metal and roughness map stay out. The renderer uses only the colour.

Run it from the root of the repository:

    python -X utf8 misc\\glb_import.py <file.glb> <name> [data\\placeholder] [--yaw DEGREES]

The name of a base kind is base_city, base_big_city, base_gate or
base_port. The script needs Pillow (pip install pillow). Keep the GLB
source in art-src/models, out of git. Add a row for each model to
data/<pack>/models/provenance.tsv.
"""

import io
import json
import math
import os
import struct
import sys

from PIL import Image

TEXTURE_MAX = 1024      # pixels; the larger side of the written texture
GLB_MAGIC = b"glTF"
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

COMPONENT_FORMATS = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
COMPONENT_NORMALIZE = {5120: 127.0, 5121: 255.0, 5122: 32767.0, 5123: 65535.0}
TYPE_WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class GlbError(Exception):
    pass


def read_glb(path):
    """Returns the JSON document and the binary chunk of a GLB file."""
    with open(path, "rb") as source:
        data = source.read()
    if len(data) < 20 or data[:4] != GLB_MAGIC:
        raise GlbError(f"{path}: the file does not start with the glTF magic")
    version, length = struct.unpack_from("<II", data, 4)
    if version != 2:
        raise GlbError(f"{path}: glTF version {version}, the script reads version 2")
    if length != len(data):
        raise GlbError(f"{path}: the header gives {length} bytes, the file has {len(data)}. "
                       "A text transfer can change the bytes of a binary file. Copy the file again.")
    document, binary = None, b""
    offset = 12
    while offset + 8 <= len(data):
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        chunk = data[offset + 8: offset + 8 + chunk_length]
        if chunk_type == CHUNK_JSON:
            document = json.loads(chunk.decode("utf-8"))
        elif chunk_type == CHUNK_BIN:
            binary = chunk
        offset += 8 + chunk_length
    if document is None:
        raise GlbError(f"{path}: the file has no JSON chunk")
    for extension in document.get("extensionsRequired", []):
        raise GlbError(f"{path}: the file needs the extension {extension}. Export it without compression or quantization.")
    return document, binary


def view_bytes(document, binary, view_index):
    view = document["bufferViews"][view_index]
    if view.get("buffer", 0) != 0:
        raise GlbError("the script reads only the binary chunk of the GLB")
    start = view.get("byteOffset", 0)
    return binary[start: start + view["byteLength"]], view.get("byteStride", 0)


def read_accessor(document, binary, accessor_index):
    """Returns the elements of an accessor as a list of tuples of floats or ints."""
    accessor = document["accessors"][accessor_index]
    width = TYPE_WIDTHS[accessor["type"]]
    component = accessor["componentType"]
    count = accessor["count"]
    if "bufferView" not in accessor:
        return [tuple([0] * width)] * count
    data, stride = view_bytes(document, binary, accessor["bufferView"])
    element = struct.Struct("<" + COMPONENT_FORMATS[component] * width)
    stride = stride or element.size
    base = accessor.get("byteOffset", 0)
    scale = COMPONENT_NORMALIZE.get(component) if accessor.get("normalized") else None
    elements = []
    for index in range(count):
        values = element.unpack_from(data, base + index * stride)
        if scale:
            values = tuple(max(value / scale, -1.0) for value in values)
        elements.append(values)
    return elements


def matrix_multiply(a, b):
    """Column-major 4x4 matrices as lists of 16 floats, as in glTF."""
    return [sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4)) for col in range(4) for row in range(4)]


def node_matrix(node):
    if "matrix" in node:
        return list(node["matrix"])
    tx, ty, tz = node.get("translation", (0.0, 0.0, 0.0))
    qx, qy, qz, qw = node.get("rotation", (0.0, 0.0, 0.0, 1.0))
    sx, sy, sz = node.get("scale", (1.0, 1.0, 1.0))
    rotation = [
        1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy + qz * qw), 2 * (qx * qz - qy * qw), 0,
        2 * (qx * qy - qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz + qx * qw), 0,
        2 * (qx * qz + qy * qw), 2 * (qy * qz - qx * qw), 1 - 2 * (qx * qx + qy * qy), 0,
        0, 0, 0, 1]
    scale = [sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, 0, 0, 0, 1]
    translation = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, tx, ty, tz, 1]
    return matrix_multiply(translation, matrix_multiply(rotation, scale))


def transform_point(matrix, point):
    x, y, z = point
    return tuple(matrix[row] * x + matrix[4 + row] * y + matrix[8 + row] * z + matrix[12 + row] for row in range(3))


def transform_direction(matrix, direction):
    x, y, z = direction
    return normalize(tuple(matrix[row] * x + matrix[4 + row] * y + matrix[8 + row] * z for row in range(3)))


def determinant3(matrix):
    a, b, c = matrix[0], matrix[4], matrix[8]
    d, e, f = matrix[1], matrix[5], matrix[9]
    g, h, k = matrix[2], matrix[6], matrix[10]
    return a * (e * k - f * h) - b * (d * k - f * g) + c * (d * h - e * g)


def normalize(vector):
    length = math.sqrt(sum(value * value for value in vector))
    return tuple(value / length for value in vector) if length > 1e-12 else (0.0, 1.0, 0.0)


def face_normals(positions, indices):
    sums = [[0.0, 0.0, 0.0] for _ in positions]
    for corner in range(0, len(indices) - 2, 3):
        a, b, c = indices[corner], indices[corner + 1], indices[corner + 2]
        pa, pb, pc = positions[a], positions[b], positions[c]
        ab = [pb[axis] - pa[axis] for axis in range(3)]
        ac = [pc[axis] - pa[axis] for axis in range(3)]
        normal = (ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0])
        for vertex in (a, b, c):
            for axis in range(3):
                sums[vertex][axis] += normal[axis]
    return [normalize(total) for total in sums]


def collect_mesh(document, binary):
    """Returns the vertices (position, normal, uv), the indices and the
    material index of the first primitive, in world space."""
    vertices, indices, material = [], [], None
    scene = document["scenes"][document.get("scene", 0)] if document.get("scenes") else {"nodes": list(range(len(document.get("nodes", []))))}
    stack = [(node_index, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]) for node_index in scene["nodes"]]
    while stack:
        node_index, parent = stack.pop()
        node = document["nodes"][node_index]
        world = matrix_multiply(parent, node_matrix(node))
        stack.extend((child, world) for child in node.get("children", []))
        if "mesh" not in node:
            continue
        mirrored = determinant3(world) < 0.0
        for primitive in document["meshes"][node["mesh"]]["primitives"]:
            if primitive.get("mode", 4) != 4:
                continue
            attributes = primitive["attributes"]
            positions = [transform_point(world, p) for p in read_accessor(document, binary, attributes["POSITION"])]
            local = read_accessor(document, binary, primitive["indices"]) if "indices" in primitive else [(i,) for i in range(len(positions))]
            local = [element[0] for element in local]
            if mirrored:
                for corner in range(0, len(local) - 2, 3):
                    local[corner + 1], local[corner + 2] = local[corner + 2], local[corner + 1]
            if "NORMAL" in attributes:
                normals = [transform_direction(world, n) for n in read_accessor(document, binary, attributes["NORMAL"])]
            else:
                normals = face_normals(positions, local)
            uvs = read_accessor(document, binary, attributes["TEXCOORD_0"]) if "TEXCOORD_0" in attributes else [(0.0, 0.0)] * len(positions)
            if material is None:
                material = primitive.get("material")
            base = len(vertices)
            vertices.extend((positions[i], normals[i], uvs[i]) for i in range(len(positions)))
            indices.extend(base + index for index in local)
    if not vertices or not indices:
        raise GlbError("the scene has no triangles")
    return vertices, indices, material


def place_model(vertices, yaw_degrees):
    """Turns the model, puts its footprint on the origin and its base on
    y = 0, and scales the larger side of the footprint to one unit."""
    angle = math.radians(yaw_degrees)
    c, s = math.cos(angle), math.sin(angle)
    turned = []
    for (px, py, pz), (nx, ny, nz), uv in vertices:
        turned.append(((px * c + pz * s, py, -px * s + pz * c), (nx * c + nz * s, ny, -nx * s + nz * c), uv))
    low = [min(v[0][axis] for v in turned) for axis in range(3)]
    high = [max(v[0][axis] for v in turned) for axis in range(3)]
    side = max(high[0] - low[0], high[2] - low[2])
    if side <= 0.0:
        raise GlbError("the model has no footprint")
    centre_x, centre_z = (low[0] + high[0]) * 0.5, (low[2] + high[2]) * 0.5
    placed = []
    for (px, py, pz), normal, uv in turned:
        placed.append((((px - centre_x) / side, (py - low[1]) / side, (pz - centre_z) / side), normal, uv))
    size = ((high[0] - low[0]) / side, (high[1] - low[1]) / side, (high[2] - low[2]) / side)
    return placed, size


def base_colour_image(document, binary, material_index):
    """Returns the base colour texture of the material as a Pillow image,
    or a 4 x 4 image of the base colour factor."""
    material = document["materials"][material_index] if material_index is not None and document.get("materials") else {}
    pbr = material.get("pbrMetallicRoughness", {})
    factor = pbr.get("baseColorFactor", (1.0, 1.0, 1.0, 1.0))
    texture_info = pbr.get("baseColorTexture")
    if texture_info is None:
        colour = tuple(int(round(max(0.0, min(1.0, value)) * 255)) for value in factor[:3])
        return Image.new("RGB", (4, 4), colour)
    image_info = document["images"][document["textures"][texture_info["index"]]["source"]]
    if "bufferView" not in image_info:
        raise GlbError("the base colour image is outside the GLB")
    data, _ = view_bytes(document, binary, image_info["bufferView"])
    image = Image.open(io.BytesIO(data)).convert("RGB")
    if any(abs(value - 1.0) > 1e-3 for value in factor[:3]):
        r, g, b = image.split()
        image = Image.merge("RGB", [channel.point(lambda value, f=f: int(value * f)) for channel, f in zip((r, g, b), factor[:3])])
    return image


def write_hxm(path, vertices, indices):
    with open(path, "wb") as target:
        target.write(b"HXM1")
        target.write(struct.pack("<II", len(vertices), len(indices)))
        packed = bytearray()
        for position, normal, uv in vertices:
            packed += struct.pack("<8f", *position, *normal, uv[0], uv[1])
        target.write(packed)
        target.write(struct.pack(f"<{len(indices)}I", *indices))


def import_model(glb_path, name, pack, yaw_degrees):
    document, binary = read_glb(glb_path)
    vertices, indices, material = collect_mesh(document, binary)
    vertices, size = place_model(vertices, yaw_degrees)
    image = base_colour_image(document, binary, material)
    if max(image.size) > TEXTURE_MAX:
        ratio = TEXTURE_MAX / max(image.size)
        image = image.resize((max(1, round(image.size[0] * ratio)), max(1, round(image.size[1] * ratio))), Image.LANCZOS)
    folder = os.path.join(pack, "models")
    os.makedirs(folder, exist_ok=True)
    write_hxm(os.path.join(folder, name + ".hxm"), vertices, indices)
    image.save(os.path.join(folder, name + ".png"), optimize=True)
    print(f"{name}: {len(vertices)} vertices, {len(indices) // 3} triangles, "
          f"size {size[0]:.3f} x {size[1]:.3f} x {size[2]:.3f}, texture {image.size[0]} x {image.size[1]}")


def main(arguments):
    yaw = 0.0
    if "--yaw" in arguments:
        at = arguments.index("--yaw")
        yaw = float(arguments[at + 1])
        arguments = arguments[:at] + arguments[at + 2:]
    if len(arguments) < 2:
        print(__doc__)
        return 2
    pack = arguments[2] if len(arguments) > 2 else os.path.join("data", "placeholder")
    try:
        import_model(arguments[0], arguments[1], pack, yaw)
    except GlbError as error:
        print(f"error: {error}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
