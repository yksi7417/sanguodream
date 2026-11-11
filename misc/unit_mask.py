"""Paints the red faction mask of a unit sheet onto the texture of its 3D model.

TRELLIS.2 makes the texture of a model from the cut picture, but the texture
loses the flat red #D01818 of the faction paint. The model shader then finds
no red, and the unit shows no faction colour. This module puts the red back:

1. It searches the camera that shows the model with the silhouette of the
   cut picture. The search tries each yaw, pitch and mirror, and keeps the
   camera with the largest overlap of the two silhouettes.
2. It finds the 3D point of each texel through the UV map of the model.
3. It projects each texel point to the cut picture. If the camera sees the
   triangle of the texel at that pixel, and the pixel is red paint, the
   texel becomes #D01818.

The camera does not see the back of the model, so the back keeps the colour
of TRELLIS.2.
"""

import math
import struct

import numpy
from PIL import Image, ImageDraw, ImageFilter

import art_import

MASK_RED = (208, 24, 24)
SEARCH_SIZE = 256                # pixels; the side of the silhouettes for the camera search
COARSE_YAW_STEP = 15             # degrees
COARSE_PITCH_STEP = 10           # degrees
FINE_STEP = 3                    # degrees, around the best coarse camera
MAX_PITCH = 70                   # degrees
OPAQUE_ALPHA = 200
RED_GROWTH = 5                   # pixels; the size of the filter that grows the red mask a little
VISIBILITY_SLACK = 1             # pixels; a texel is visible if its triangle is this near its pixel


def read_hxm(path):
    """Returns the vertices (n x 8: position, normal, uv) and the triangles (m x 3) of an HXM1 file."""
    with open(path, "rb") as model_file:
        data = model_file.read()
    if data[:4] != b"HXM1":
        raise ValueError("not an HXM1 file: " + path)
    vertex_count, index_count = struct.unpack_from("<II", data, 4)
    vertices = numpy.frombuffer(data, dtype="<f4", count=vertex_count * 8, offset=12).reshape(vertex_count, 8)
    indices = numpy.frombuffer(data, dtype="<u4", count=index_count, offset=12 + vertex_count * 32)
    return vertices.astype(float), indices.reshape(-1, 3).astype(numpy.int64)


def camera_view(points, camera):
    """Screen x to the right, screen y down, and depth toward the camera, for an orthographic camera."""
    yaw, pitch, mirror = math.radians(camera[0]), math.radians(camera[1]), camera[2]
    turned_x = points[..., 0] * math.cos(yaw) + points[..., 2] * math.sin(yaw)
    turned_z = -points[..., 0] * math.sin(yaw) + points[..., 2] * math.cos(yaw)
    height = points[..., 1]
    screen_up = height * math.cos(pitch) - turned_z * math.sin(pitch)
    depth = turned_z * math.cos(pitch) + height * math.sin(pitch)
    return (-turned_x if mirror else turned_x), -screen_up, depth


def screen_fit(screen_x, screen_y, box):
    """The scale and the offset that put the bounding box of the screen points on the box of the cut."""
    scale_x = (box[2] - box[0]) / max(1e-9, screen_x.max() - screen_x.min())
    scale_y = (box[3] - box[1]) / max(1e-9, screen_y.max() - screen_y.min())
    return (scale_x, box[0] - screen_x.min() * scale_x, scale_y, box[1] - screen_y.min() * scale_y)


def to_pixels(screen_x, screen_y, fit):
    return screen_x * fit[0] + fit[1], screen_y * fit[2] + fit[3]


def triangle_image(pixel_x, pixel_y, triangles, size, order):
    """An image of triangle numbers, drawn in the given order. -1 where no triangle is."""
    image = Image.new("I", (size, size), -1)
    draw = ImageDraw.Draw(image)
    for triangle in order:
        corners = triangles[triangle]
        draw.polygon([(pixel_x[corner], pixel_y[corner]) for corner in corners], fill=int(triangle))
    return numpy.asarray(image).astype(numpy.int64)


def overlap(vertices, triangles, camera, target, box):
    screen_x, screen_y, _ = camera_view(vertices[:, :3], camera)
    pixel_x, pixel_y = to_pixels(screen_x, screen_y, screen_fit(screen_x, screen_y, box))
    silhouette = triangle_image(pixel_x, pixel_y, triangles, SEARCH_SIZE, range(len(triangles))) >= 0
    return (silhouette & target).sum() / max(1, (silhouette | target).sum())


def find_camera(vertices, triangles, cut_alpha):
    """The camera (yaw, pitch, mirror) whose silhouette of the model best matches the cut, and the overlap."""
    small_alpha = Image.fromarray(cut_alpha).resize((SEARCH_SIZE, SEARCH_SIZE), Image.BILINEAR)
    target = numpy.asarray(small_alpha) > 128
    box = Image.fromarray((target * 255).astype("uint8")).getbbox()
    cameras = [(yaw, pitch, mirror) for mirror in (False, True)
               for yaw in range(0, 360, COARSE_YAW_STEP) for pitch in range(0, MAX_PITCH, COARSE_PITCH_STEP)]
    best = max(cameras, key=lambda camera: overlap(vertices, triangles, camera, target, box))
    fine = [(best[0] + yaw_step, max(0, best[1] + pitch_step), best[2])
            for yaw_step in range(-COARSE_YAW_STEP, COARSE_YAW_STEP + 1, FINE_STEP)
            for pitch_step in range(-COARSE_PITCH_STEP, COARSE_PITCH_STEP + 1, FINE_STEP)]
    best = max(fine, key=lambda camera: overlap(vertices, triangles, camera, target, box))
    return best, overlap(vertices, triangles, best, target, box)


def texel_points(vertices, triangles, size):
    """The row, the column, the triangle and the 3D point of each texel that a triangle covers."""
    uv = vertices[:, 6:8] * size
    owner = triangle_image(uv[:, 0], uv[:, 1], triangles, size, range(len(triangles)))
    rows, columns = numpy.nonzero(owner >= 0)
    owners = owner[rows, columns]
    corner_a, corner_b, corner_c = (uv[triangles[owners, corner]] for corner in range(3))
    centre = numpy.stack([columns + 0.5, rows + 0.5], 1)
    edge_b, edge_c, offset = corner_b - corner_a, corner_c - corner_a, centre - corner_a
    dot_bb, dot_bc, dot_cc = (edge_b * edge_b).sum(1), (edge_b * edge_c).sum(1), (edge_c * edge_c).sum(1)
    dot_ob, dot_oc = (offset * edge_b).sum(1), (offset * edge_c).sum(1)
    denominator = dot_bb * dot_cc - dot_bc * dot_bc
    denominator[denominator == 0] = 1e-9
    weight_b = (dot_cc * dot_ob - dot_bc * dot_oc) / denominator
    weight_c = (dot_bb * dot_oc - dot_bc * dot_ob) / denominator
    weight_a = 1.0 - weight_b - weight_c
    positions = vertices[:, :3]
    points = (weight_a[:, None] * positions[triangles[owners, 0]] + weight_b[:, None] * positions[triangles[owners, 1]]
              + weight_c[:, None] * positions[triangles[owners, 2]])
    return rows, columns, owners, points


def red_paint(cut):
    """True where the cut picture shows the flat red of the faction paint, grown a little."""
    alpha = numpy.asarray(cut.split()[3])
    red = (numpy.asarray(art_import.red_mask(cut.convert("RGB"))) > 128) & (alpha > OPAQUE_ALPHA)
    grown = Image.fromarray((red * 255).astype("uint8")).filter(ImageFilter.MaxFilter(RED_GROWTH))
    return numpy.asarray(grown) > 128


def paint_faction_mask(hxm_path, texture_path, cut_path):
    """Writes the red of the cut into the texture. Returns the camera, the overlap and the count of red texels."""
    vertices, triangles = read_hxm(hxm_path)
    cut = Image.open(cut_path).convert("RGBA")
    size = cut.width
    cut_alpha = numpy.asarray(cut.split()[3])
    camera, match = find_camera(vertices, triangles, cut_alpha)
    box = Image.fromarray(((cut_alpha > 128) * 255).astype("uint8")).getbbox()
    screen_x, screen_y, depth = camera_view(vertices[:, :3], camera)
    fit = screen_fit(screen_x, screen_y, box)
    pixel_x, pixel_y = to_pixels(screen_x, screen_y, fit)
    far_first = numpy.argsort(depth[triangles].mean(1))
    seen_triangle = triangle_image(pixel_x, pixel_y, triangles, size, far_first)
    texture = Image.open(texture_path).convert("RGB")
    texels = numpy.asarray(texture).copy()
    rows, columns, owners, points = texel_points(vertices, triangles, texture.width)
    point_x, point_y, _ = camera_view(points, camera)
    point_x, point_y = to_pixels(point_x, point_y, fit)
    column_on_cut = numpy.clip(point_x.astype(int), 0, size - 1)
    row_on_cut = numpy.clip(point_y.astype(int), 0, size - 1)
    visible = numpy.zeros(len(owners), bool)
    for step_x in range(-VISIBILITY_SLACK, VISIBILITY_SLACK + 1):
        for step_y in range(-VISIBILITY_SLACK, VISIBILITY_SLACK + 1):
            near_row = numpy.clip(row_on_cut + step_y, 0, size - 1)
            near_column = numpy.clip(column_on_cut + step_x, 0, size - 1)
            visible |= seen_triangle[near_row, near_column] == owners
    paint = visible & red_paint(cut)[row_on_cut, column_on_cut]
    texels[rows[paint], columns[paint]] = MASK_RED
    Image.fromarray(texels).save(texture_path)
    return camera, match, int(paint.sum())
