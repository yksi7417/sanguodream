"""Makes the map of a content pack from real geography (docs/MAPS.md).

    python misc/make_world_map.py data/medieval_europe [--offline] [--preview FILE] [--keep-weak-gates]

The tool reads <pack>/map/source/region.tsv and <pack>/map/source/sites.tsv.
It writes <pack>/map/hexes.tsv, <pack>/map/bases.tsv, <pack>/cities.tsv and
<pack>/bases.tsv in the formats of the placeholder pack (formats/map-data.md).

The sources are public domain:
- the height: NOAA ETOPO 2022, 60 arc-second surface elevation, through THREDDS OPeNDAP;
- the land, the lakes, the rivers and the deserts: Natural Earth 1:50m vectors.
The tool keeps the downloads in build/geo_cache. With --offline it uses only that cache.

The grid is the grid of the engine: flat-top hexes in odd-q offset rows.
An odd column is half a hex lower (to the south) than an even column.

The rivers follow the rivers of the placeholder map (formats/map-data.md). That map has
two kinds of river. A 河 river is wide water for ships, with a 岸 bank strip that no unit
enters. No road fords a 河 river: armies cross it by ship from a port. A 川 stream is
narrow water that no unit enters. A road crosses it at a 渡所 ford. These settings of
region.tsv choose the kind and the width of each Natural Earth river:
- river_widths: "Name=W" or "Name=W|W2@LON|W3@LON3", separated by ";". W is the width in
  hexes. W2 is the width west of the longitude LON. The rivers in this list are 河 rivers.
  A width of 0 makes that part of the river a minor river.
- river_minor_width and river_minor_terrain: the width and the terrain (river or stream)
  of the other rivers. The defaults are 1 and river.
- Each river and stream is RIVER_MIN_WIDTH (3) hexes wide or wider (the player, 2026-11-05).
- river_banks: yes puts 岸 hexes along the 河 rivers, except next to a base or a road.
- river_fords: no stops the roads at the 河 rivers. The default yes keeps the first packs.
- river_shallows_every: a 淺瀨 shallows crossing after this many hexes of each 川 stream,
  as in the placeholder map. Land units cross a stream there without a road. 0 = none.
- river_extra: more lines for a river, "Name:LON,LAT LON,LAT ...", separated by ";". Use it
  where Natural Earth stops a river before the sea.

The bases follow these rules:
- A pass guards the city in its owner column against the city in its across column. The
  tool finds the land route between the two cities. It puts the gate at the narrowest gap
  of that route near the position of the pass. Two walls of 崖 cliff go from the gate to
  large barriers: mountains, rivers, the sea or the edge of the map. Thus an army must go
  through the gate or take a long detour. The walls follow the high ground.
- Each gate must have a purpose. After the tool made the map, it closes the centre hex of
  each gate. Then the land route between the two cities of the pass must be 1.5 times
  longer, or 20 hexes longer, or closed. If a gate fails this check, the tool writes
  nothing and stops. With --keep-weak-gates, it writes the map and only reports the gate.
- A port must be on water that leads to other ports: a sea with 2 ports or more, or a lake
  or river with 3 ports or more. The tool moves a port on a closed lake to the nearest good shore.
- A port is PORT_CITY_HEXES (10) or more from the centre of each city. A nearer city on the
  same water launches its ships itself, so the tool removes the port. Else the port moves along
  its water. A removed port keeps its id in bases.tsv with present = no.
- A port stands at the level of its water. On the sea, its model stands at the level of the sea.
- Inland water has a height near the height of the land around it. A lake takes the height of
  its lowest shore. A river falls step by step toward the sea, and it never climbs on its way
  down. The sea has the height 0. The height changes only the picture: the engine divides it
  by 16 for the level of the renderer, and the cost of a hex comes from its terrain.
- A city on the water gets port = yes in cities.tsv: it can build ships, and its units go
  on the water from the city. A city on the water has water, or open land next to water,
  at a distance of 2 hexes from its centre.
"""
import heapq
import json
import math
import os
import sys
import time
import urllib.error
import urllib.request
import warnings

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "build", "geo_cache")
EARTH_KM = 6371.0
SAN11_CITY_SPACING = 22   # median hexes from a city of the placeholder map to the nearest city

ETOPO_URL = ("https://www.ngdc.noaa.gov/thredds/dodsC/global/ETOPO2022/60s/60s_surface_elev_netcdf/"
             "ETOPO_2022_v1_60s_N90W180_surface.nc.ascii")
ETOPO_STRIDE = 3          # 3 arc-minutes, approximately 5 km: sufficient for hexes of 10 km and more
NE_URL = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/%s.geojson"

# The terrain ids of data/placeholder/terrain.tsv.
GRASS, EARTH, SAND, SWAMP, FOREST, STREAM, RIVER, SEA, WASTE, ROAD, FORD, SHALLOWS, SHORE, CLIFF, CITY, PORT, GATE =     0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18
WATER = (RIVER, SEA)
FLOWING = (RIVER, STREAM, FORD, SHALLOWS)   # inland water: its height follows the land (make_water_heights)
NATURAL_LAND = (GRASS, EARTH, SAND, SWAMP, FOREST, WASTE)
# The foot cost of terrain.tsv, for the roads and the regions. 0 = closed.
COST = {GRASS: 5, EARTH: 5, SAND: 5, SWAMP: 8, FOREST: 6, WASTE: 6, ROAD: 4, FORD: 4, SHALLOWS: 5, CITY: 4, PORT: 4, GATE: 4}
ROAD_RIVER_COST = 14      # a road can cross a river: the crossing becomes a ford
# A land unit cannot enter these hexes. The walls of a pass end at a large group of them.
BARRIER = (CLIFF, SEA, RIVER, SHORE)
BIG_BARRIER_HEXES = 40    # a smaller group of barrier hexes is a hill that an army walks around
WALL_KINDS = NATURAL_LAND + (STREAM, SHALLOWS)
WALL_MAX_COST = 90.0      # the longest wall of a pass: 15 hexes of plain or 90 hexes of mountain
WALL_HIGH_METRES = 1000.0 # a wall hex at this height or higher costs 1
WALL_LOW_METRES = 300.0   # a wall hex at this height or lower costs 1 + WALL_LOW_EXTRA
WALL_LOW_EXTRA = 5.0
PASS_SEARCH_HEXES = 12    # the gate moves along the route up to this distance from the position of the pass
PASS_MOVE_COST = 3.0      # the cost of each hex of that move, against the cost of the walls
GATE_CITY_HEXES = 8       # no gate comes nearer to the centre of a city
GATE_GATE_HEXES = 8       # no gate comes nearer to another gate
GATE_DETOUR_TRIES = 40    # the cheapest gaps that the tool tests for the detour
GATE_PURPOSE_FACTOR = 1.5 # the purpose check: with the gate hex closed, the route is this factor longer,
GATE_PURPOSE_EXTRA = 20   # or this many hexes longer, or no route goes from one side to the other
CITY_CLEAR_HEXES = 3      # no wall comes this near to the centre of a city
CITY_OPEN_HEXES = 5       # with relief cliffs, the tool opens the cliffs this near to the centre of a city
VALLEY_CLIFF_COST = 30   # the cost of a cliff hex on the route of a valley: the valley crosses few cliffs
VALLEY_DETOUR = 1.5       # the tool opens a valley if the route around the cliffs is longer than this factor
VALLEY_GROUP_TRIES = 8    # the pairs of cities that the tool tries to join two land groups
PORT_MOVE_HEXES = 30     # the largest move of a port from a closed lake to a good shore
PORT_LAKE_PORTS = 3       # a lake or a river without the sea needs this many ports
PORT_CITY_HEXES = 10      # no port comes nearer to the centre of a city (the player, 2026-11-05)
RIVER_MIN_WIDTH = 3       # each river and stream is this many hexes wide or wider (the player, 2026-11-05)
PORT_HEIGHT = 8           # a port stands this much above the level of its water (sim.h, SIM_MAP_HEIGHT_STEP is 16)
HEIGHT_STEP = 16          # the height of one level of the renderer (sim.h, SIM_MAP_HEIGHT_STEP)
WATER_BELOW_SHORE = 8     # inland water stands this much below the lowest land next to it

# The cube directions of the flat-top grid: 0 = north, then clockwise.
CUBE_DIRECTIONS = ((0, -1), (1, -1), (1, 0), (0, 1), (-1, 1), (-1, 0))


# ---- the files of the pack ----------------------------------------------------------

def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        rows = [line.rstrip("\r\n").split("\t") for line in f if line.strip() and not line.startswith("#")]
    head = rows[0]
    return [dict(zip(head, row)) for row in rows[1:]]


def read_settings(pack):
    settings = {row["key"]: row["value"] for row in read_tsv(os.path.join(pack, "map", "source", "region.tsv"))}
    for key in ("lat_min", "lat_max", "lon_min", "lon_max", "cliff_metres", "mountain_metres"):
        settings[key] = float(settings[key])
    settings.setdefault("max_hexes", "80000")
    settings.setdefault("margin_hexes", "12")
    for key in ("river_max_scalerank", "seed", "max_hexes", "margin_hexes"):
        settings[key] = int(settings[key])
    settings["city_spacing_hexes"] = float(settings.get("city_spacing_hexes", SAN11_CITY_SPACING))
    settings["cliff_relief_share"] = float(settings.get("cliff_relief_share", "0"))
    settings["cliff_relief_hexes"] = int(settings.get("cliff_relief_hexes", "6"))
    settings["cliff_relief_min_metres"] = float(settings.get("cliff_relief_min_metres", "250"))
    km = settings.get("km_per_hex", "-")
    settings["km_per_hex"] = None if km in ("", "-") else float(km)
    settings["river_widths"] = parse_river_widths(settings.get("river_widths", "-"))
    settings["river_minor_width"] = int(settings.get("river_minor_width", "1"))
    minor = settings.get("river_minor_terrain", "river")
    if minor not in ("river", "stream"): sys.exit("region.tsv: river_minor_terrain must be river or stream")
    settings["river_minor_terrain"] = STREAM if minor == "stream" else RIVER
    settings["river_banks"] = settings.get("river_banks", "no") == "yes"
    settings["river_fords"] = settings.get("river_fords", "yes") == "yes"
    settings["river_shallows_every"] = int(settings.get("river_shallows_every", "0"))
    extra_text, settings["river_extra"] = settings.get("river_extra", "-"), []
    for entry in extra_text.split(";"):
        if entry in ("", "-"): continue
        name, points = entry.split(":")
        settings["river_extra"].append((name.strip(), [[float(v) for v in point.split(",")] for point in points.split()]))
    return settings


def parse_river_widths(text):
    """{name: (width, [(lon, width west of lon), ...])} from "Name=W|W2@LON;Name2=W"."""
    widths = {}
    if text in ("", "-"): return widths
    for entry in text.split(";"):
        name, value = entry.split("=")
        parts = value.split("|")
        west = sorted(((float(p.split("@")[1]), int(p.split("@")[0])) for p in parts[1:]), reverse=True)
        widths[name.strip()] = (int(parts[0]), west)
    return widths


def river_width(rule, lon):
    width, west = rule
    for limit, narrow in west:
        if lon < limit: width = narrow
    return width


def read_sites(pack):
    sites = read_tsv(os.path.join(pack, "map", "source", "sites.tsv"))
    for index, site in enumerate(sites):
        if int(site["id"]) != index: sys.exit("sites.tsv: row %d has the id %s" % (index, site["id"]))
        site["id"] = index
        site["lat"], site["lon"] = float(site["lat"]), float(site["lon"])
        # kind none: the map leaves the site out and keeps its id. bases.tsv gets a pass with present = no.
        if site["kind"] == "none": site["removed"] = True
    cities = [s for s in sites if s["kind"] == "city"]
    if len(cities) != 42 or any(s["id"] >= 42 for s in cities):
        sys.exit("sites.tsv: the engine needs 42 cities with the ids 0 to 41")
    if len(sites) != 87: sys.exit("sites.tsv: the engine needs 87 sites (42 cities, then 45 passes and ports)")
    return sites


# ---- the projection -------------------------------------------------------------------

class Projection:
    """Lambert azimuthal equal-area, in km, with y to the north."""

    def __init__(self, lat0, lon0):
        self.phi0, self.lam0 = math.radians(lat0), math.radians(lon0)

    def forward(self, lat, lon):
        phi, lam = np.radians(lat), np.radians(lon) - self.lam0
        k = np.sqrt(2.0 / (1.0 + math.sin(self.phi0) * np.sin(phi) + math.cos(self.phi0) * np.cos(phi) * np.cos(lam)))
        x = EARTH_KM * k * np.cos(phi) * np.sin(lam)
        y = EARTH_KM * k * (math.cos(self.phi0) * np.sin(phi) - math.sin(self.phi0) * np.cos(phi) * np.cos(lam))
        return x, y

    def inverse(self, x, y):
        rho = np.maximum(np.hypot(x, y), 1e-9)
        c = 2.0 * np.arcsin(np.clip(rho / (2.0 * EARTH_KM), -1.0, 1.0))
        phi = np.arcsin(np.cos(c) * math.sin(self.phi0) + y * np.sin(c) * math.cos(self.phi0) / rho)
        lam = self.lam0 + np.arctan2(x * np.sin(c), rho * math.cos(self.phi0) * np.cos(c) - y * math.sin(self.phi0) * np.sin(c))
        return np.degrees(phi), np.degrees(lam)


def great_circle_km(a, b):
    phi1, phi2 = math.radians(a["lat"]), math.radians(b["lat"])
    dphi, dlam = phi2 - phi1, math.radians(b["lon"] - a["lon"])
    h = math.sin(dphi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlam / 2) ** 2
    return 2 * EARTH_KM * math.asin(math.sqrt(h))


# ---- the grid -----------------------------------------------------------------------

class Grid:
    def __init__(self, width, height, spacing_km, x0, y0):
        self.width, self.height = width, height
        self.spacing = spacing_km             # centre to centre
        self.size = spacing_km / math.sqrt(3) # centre to corner
        self.x0, self.y0 = x0, y0             # the km position of hex (0, 0); y0 is the north edge

    def centres(self):
        cols, rows = np.meshgrid(np.arange(self.width), np.arange(self.height))
        x = self.x0 + cols * 1.5 * self.size
        y = self.y0 - (rows + 0.5 * (cols & 1)) * self.spacing
        return x, y   # arrays [row][col]

    def hex_of(self, x, y):
        col = int(round((x - self.x0) / (1.5 * self.size)))
        row = int(round((self.y0 - y) / self.spacing - 0.5 * (col & 1)))
        best, best_d = None, None
        for dc in (-1, 0, 1):
            for dr in (-1, 0, 1):
                c, r = col + dc, row + dr
                cx = self.x0 + c * 1.5 * self.size
                cy = self.y0 - (r + 0.5 * (c & 1)) * self.spacing
                d = (cx - x) ** 2 + (cy - y) ** 2
                if best_d is None or d < best_d: best, best_d = (c, r), d
        return best

    def contains(self, cell):
        return 0 <= cell[0] < self.width and 0 <= cell[1] < self.height


def to_cube(cell):
    col, row = cell
    return col, row - (col - (col & 1)) // 2


def from_cube(q, r):
    return q, r + (q - (q & 1)) // 2


def neighbour(cell, direction):
    q, r = to_cube(cell)
    dq, dr = CUBE_DIRECTIONS[direction % 6]
    return from_cube(q + dq, r + dr)


def neighbours(cell):
    return [neighbour(cell, d) for d in range(6)]


def hex_distance(a, b):
    aq, ar = to_cube(a)
    bq, br = to_cube(b)
    return max(abs(aq - bq), abs(ar - br), abs((-aq - ar) - (-bq - br)))


def hex_line(a, b):
    """The hexes from a to b, each next to the one before."""
    n = hex_distance(a, b)
    if n == 0: return [a]
    aq, ar = to_cube(a)
    bq, br = to_cube(b)
    out = []
    for i in range(n + 1):
        t = i / n
        q, r = aq + (bq - aq) * t + 1e-6, ar + (br - ar) * t + 1e-6
        s = -q - r
        rq, rr, rs = round(q), round(r), round(s)
        dq, dr, ds = abs(rq - q), abs(rr - r), abs(rs - s)
        if dq > dr and dq > ds: rq = -rr - rs
        elif dr > ds: rr = -rq - rs
        out.append(from_cube(int(rq), int(rr)))
    return out


def hex_xy(cell):
    """The centre of a hex on a plane, in units of the hex size, with y to the south."""
    col, row = cell
    return 1.5 * col, math.sqrt(3) * (row + 0.5 * (col & 1))


def disc(centre, radius):
    """The hexes at a distance of radius or less from centre."""
    out = [centre]
    for r in range(1, radius + 1): out += ring(centre, r)
    return out


def thick_cells(cell, width):
    """The hexes of a river of this width around one hex of its centre line. An even width adds the hexes to the south."""
    cells = disc(cell, (width - 1) // 2)
    if width % 2 == 0: cells += [neighbour(c, 3) for c in cells]
    return cells


def ring(centre, radius):
    q, r = to_cube(centre)
    q, r = q + CUBE_DIRECTIONS[4][0] * radius, r + CUBE_DIRECTIONS[4][1] * radius
    out = []
    for direction in range(6):
        for _ in range(radius):
            out.append(from_cube(q, r))
            q, r = q + CUBE_DIRECTIONS[direction][0], r + CUBE_DIRECTIONS[direction][1]
    return out


# ---- the downloads ------------------------------------------------------------------

def fetch(url, path, offline):
    if os.path.exists(path): return path
    if offline: sys.exit("not in the cache (and --offline): %s" % path)
    print("download", url)
    for attempt in range(4):   # the THREDDS server sometimes returns 500 for a request that works later
        try:
            with urllib.request.urlopen(url, timeout=600) as response, open(path + ".part", "wb") as out:
                out.write(response.read())
            break
        except urllib.error.HTTPError:
            if attempt == 3: raise
            time.sleep(5 * (attempt + 1))
    os.replace(path + ".part", path)
    return path


def cached_etopo(i0, i1, j0, j1):
    """A cached ETOPO file that covers the request, as (path, row offset, column offset)."""
    for name in sorted(os.listdir(CACHE)):
        parts = name[:-4].split("_") if name.startswith("etopo2022_") and name.endswith(".npz") else None
        if not parts or len(parts) != 6: continue
        c0, c1, d0, d1, stride = (int(p) for p in parts[1:])
        if stride == ETOPO_STRIDE and c0 <= i0 and c1 >= i1 and d0 <= j0 and d1 >= j1:
            return os.path.join(CACHE, name), (i0 - c0) // stride, (j0 - d0) // stride
    return None


def load_etopo(lat_min, lat_max, lon_min, lon_max, offline):
    """The ETOPO 2022 heights in metres on a regular grid: (lats, lons, z[lat][lon])."""
    first, step = 1 / 120, 1 / 60
    i0, i1 = int((lat_min + 90 - first) / step), int(math.ceil((lat_max + 90 - first) / step))
    j0, j1 = int((lon_min + 180 - first) / step), int(math.ceil((lon_max + 180 - first) / step))
    i1 -= (i1 - i0) % ETOPO_STRIDE
    j1 -= (j1 - j0) % ETOPO_STRIDE
    covered = cached_etopo(i0, i1, j0, j1)
    if covered:
        path, row, col = covered
        data = np.load(path)
        rows, cols = (i1 - i0) // ETOPO_STRIDE + 1, (j1 - j0) // ETOPO_STRIDE + 1
        return data["lats"][row:row + rows], data["lons"][col:col + cols], data["z"][row:row + rows, col:col + cols]
    path = os.path.join(CACHE, "etopo2022_%d_%d_%d_%d_%d.npz" % (i0, i1, j0, j1, ETOPO_STRIDE))
    if not os.path.exists(path):
        rows = []
        chunk = 30 * ETOPO_STRIDE   # the server refuses large requests
        for start in range(i0, i1 + 1, chunk):
            stop = min(start + chunk - ETOPO_STRIDE, i1)
            url = "%s?z%%5B%d:%d:%d%%5D%%5B%d:%d:%d%%5D" % (ETOPO_URL, start, ETOPO_STRIDE, stop, j0, ETOPO_STRIDE, j1)
            text_path = os.path.join(CACHE, "etopo_part.txt")
            if os.path.exists(text_path): os.remove(text_path)
            fetch(url, text_path, offline)
            with open(text_path, encoding="ascii") as f:
                body = f.read().split("z.z[", 1)[1].split("\n\n", 1)[0].splitlines()[1:]
            os.remove(text_path)
            rows.extend([float(v) for v in line.split(",")[1:]] for line in body)
        lats = np.array([-90 + first + step * i for i in range(i0, i1 + 1, ETOPO_STRIDE)])
        lons = np.array([-180 + first + step * j for j in range(j0, j1 + 1, ETOPO_STRIDE)])
        np.savez_compressed(path, lats=lats, lons=lons, z=np.array(rows, dtype=np.float32))
    data = np.load(path)
    return data["lats"], data["lons"], data["z"]


def load_ne(name, offline):
    path = fetch(NE_URL % name, os.path.join(CACHE, name + ".geojson"), offline)
    with open(path, encoding="utf-8") as f:
        return json.load(f)["features"]


# ---- sampling ---------------------------------------------------------------------

class Raster:
    """A lat-lon raster on the ETOPO grid, for the polygon layers."""

    def __init__(self, lats, lons):
        self.lat0, self.lon0 = lats[0], lons[0]
        self.step = lats[1] - lats[0]
        self.nlat, self.nlon = len(lats), len(lons)

    def pixel(self, lon, lat):
        return (lon - self.lon0) / self.step, (self.nlat - 1) - (lat - self.lat0) / self.step

    def polygons(self, features, keep=lambda props: True):
        image = Image.new("L", (self.nlon, self.nlat), 0)
        draw = ImageDraw.Draw(image)
        for feature in features:
            if not keep(feature["properties"]): continue
            geometry = feature["geometry"]
            polys = geometry["coordinates"] if geometry["type"] == "MultiPolygon" else [geometry["coordinates"]]
            for poly in polys:
                for index, ring_points in enumerate(poly):
                    points = [self.pixel(lon, lat) for lon, lat in ring_points]
                    if len(points) > 2: draw.polygon(points, fill=0 if index else 1)
        return np.array(image, dtype=np.uint8)[::-1]   # row 0 = the first (southern) latitude

    def sample(self, values, lat, lon):
        i = np.clip(np.round((lat - self.lat0) / self.step).astype(int), 0, self.nlat - 1)
        j = np.clip(np.round((lon - self.lon0) / self.step).astype(int), 0, self.nlon - 1)
        return values[i, j]


def value_noise(width, height, cell, seed):
    rng = np.random.default_rng(seed)
    coarse = rng.random((height // cell + 2, width // cell + 2))
    ys, xs = np.mgrid[0:height, 0:width] / cell
    y0, x0 = ys.astype(int), xs.astype(int)
    fy, fx = ys - y0, xs - x0
    top = coarse[y0, x0] * (1 - fx) + coarse[y0, x0 + 1] * fx
    bottom = coarse[y0 + 1, x0] * (1 - fx) + coarse[y0 + 1, x0 + 1] * fx
    return top * (1 - fy) + bottom * fy


# ---- the map ----------------------------------------------------------------------

class WorldMap:
    def __init__(self, settings, sites, offline):
        self.settings, self.sites, self.offline = settings, sites, offline
        lat0 = (settings["lat_min"] + settings["lat_max"]) / 2
        lon0 = (settings["lon_min"] + settings["lon_max"]) / 2
        self.proj = Projection(lat0, lon0)
        self.cities = [s for s in sites if s["kind"] == "city"]
        nearest = sorted(min(great_circle_km(a, b) for b in self.cities if b is not a) for a in self.cities)
        self.city_spacing_km = (nearest[20] + nearest[21]) / 2
        self.spacing_km = settings["km_per_hex"] or self.city_spacing_km / settings["city_spacing_hexes"]
        self.make_grid()
        # The terrain mesh needs approximately 1.65 KB of frame memory for each hex (160 MB for each platform).
        # If the map has too many hexes, the hexes become larger and the cities come nearer in hexes.
        while self.grid.width * self.grid.height > settings["max_hexes"]:
            self.spacing_km *= 1.01
            self.make_grid()

    def make_grid(self):
        s = self.settings
        lats = np.concatenate([np.linspace(s["lat_min"], s["lat_max"], 60), np.full(60, s["lat_max"]),
                               np.linspace(s["lat_min"], s["lat_max"], 60), np.full(60, s["lat_min"])])
        lons = np.concatenate([np.full(60, s["lon_min"]), np.linspace(s["lon_min"], s["lon_max"], 60),
                               np.full(60, s["lon_max"]), np.linspace(s["lon_min"], s["lon_max"], 60)])
        x, y = self.proj.forward(lats, lons)
        # The map is the box of the sites plus a margin, inside the box of the settings.
        sx, sy = self.proj.forward(np.array([p["lat"] for p in self.sites]), np.array([p["lon"] for p in self.sites]))
        margin = s["margin_hexes"] * self.spacing_km
        x_min, x_max = max(x.min(), sx.min() - margin), min(x.max(), sx.max() + margin)
        y_min, y_max = max(y.min(), sy.min() - margin), min(y.max(), sy.max() + margin)
        size = self.spacing_km / math.sqrt(3)
        width = int(math.ceil((x_max - x_min) / (1.5 * size))) + 1
        height = int(math.ceil((y_max - y_min) / self.spacing_km)) + 1
        self.grid = Grid(width, height, self.spacing_km, x_min, y_max)

    # -- the layers of real data --

    def sample_layers(self):
        grid = self.grid
        cx, cy = grid.centres()
        # Seven points for each hex: the centre and six points at half the distance to the neighbours.
        offsets = [(0.0, 0.0)] + [(0.5 * grid.spacing * math.cos(a), 0.5 * grid.spacing * math.sin(a))
                                  for a in np.radians(np.arange(0, 360, 60) + 30)]
        points = [self.proj.inverse(cx + dx, cy + dy) for dx, dy in offsets]
        all_lat = np.array([p[0] for p in points])
        all_lon = np.array([p[1] for p in points])
        lats, lons, z = load_etopo(all_lat.min() - 0.2, all_lat.max() + 0.2, all_lon.min() - 0.2, all_lon.max() + 0.2, self.offline)
        raster = Raster(lats, lons)
        land = raster.polygons(load_ne("ne_50m_land", self.offline))
        lakes = raster.polygons(load_ne("ne_50m_lakes", self.offline))
        regions = load_ne("ne_50m_geography_regions_polys", self.offline)
        desert = raster.polygons(regions, lambda p: p.get("FEATURECLA") == "Desert")
        delta = raster.polygons(regions, lambda p: p.get("FEATURECLA") == "Delta")

        heights = np.array([raster.sample(z, la, lo) for la, lo in zip(all_lat, all_lon)])
        lake_points = np.array([raster.sample(lakes, la, lo) for la, lo in zip(all_lat, all_lon)])
        is_land = np.array([raster.sample(land, la, lo) for la, lo in zip(all_lat, all_lon)]) * (1 - lake_points)
        land_count = is_land.sum(axis=0)
        self.land = land_count >= 4
        self.lake = lake_points.sum(axis=0) >= 4
        land_heights = np.where(is_land > 0, heights, np.nan)
        with np.errstate(all="ignore"):
            self.elev_mean = np.where(land_count > 0, np.nanmean(land_heights, axis=0), heights.mean(axis=0))
            self.elev_max = np.where(land_count > 0, np.nanmax(land_heights, axis=0), heights.max(axis=0))
        self.desert = raster.sample(desert, all_lat[0], all_lon[0]) > 0
        self.delta = raster.sample(delta, all_lat[0], all_lon[0]) > 0
        self.lat, self.lon = all_lat[0], all_lon[0]

    def classify(self):
        s, grid = self.settings, self.grid
        noise = value_noise(grid.width, grid.height, 7, s["seed"])
        t = np.full((grid.height, grid.width), SEA, dtype=np.uint8)
        e, lat, land = self.elev_mean, self.lat, self.land
        t[land & (lat < 42)] = EARTH
        t[land & (lat >= 42)] = GRASS
        forest = land & (((lat >= 45) & ((e > 250) | (noise > 0.58))) | ((lat >= 40) & (e > 450) & (noise > 0.35)))
        t[forest] = FOREST
        t[land & (lat < 45) & (e > 500) & (noise < 0.45)] = WASTE
        t[land & self.desert] = SAND
        t[land & (e < 3) & ((lat >= 45) | self.delta)] = SWAMP
        t[land & (e > s["mountain_metres"])] = WASTE
        t[land & ((e > s["cliff_metres"]) | (self.elev_max > s["cliff_metres"] * 1.8))] = CLIFF
        if s["cliff_relief_share"] > 0:
            # The slopes and the ridges become cliffs: the hexes that stand highest above the lowest land near them.
            # The valleys and the high plains stay open, as on the placeholder map.
            relief = self.local_relief(s["cliff_relief_hexes"])
            limit = max(s["cliff_relief_min_metres"], float(np.quantile(relief[land], 1.0 - s["cliff_relief_share"])))
            t[land & (relief >= limit)] = CLIFF
        t[self.lake] = RIVER
        self.terrain = t

    def local_relief(self, radius):
        """For each land hex, its mean height minus the lowest mean height of the land within radius hexes."""
        heights = np.where(self.land, self.elev_mean, np.inf)
        rows, cols = heights.shape
        padded = np.pad(heights, radius, constant_values=np.inf)
        low = heights.copy()
        for dr in range(-radius, radius + 1):
            for dc in range(-radius, radius + 1):
                if dr * dr + dc * dc > radius * radius: continue
                low = np.minimum(low, padded[radius + dr:radius + dr + rows, radius + dc:radius + dc + cols])
        return np.where(self.land & np.isfinite(low), self.elev_mean - low, 0.0)

    def add_rivers(self):
        """The minor rivers first, then the 河 rivers of river_widths over them."""
        s = self.settings
        self.major_river = np.zeros((self.grid.height, self.grid.width), dtype=bool)
        features = []
        for feature in load_ne("ne_50m_rivers_lake_centerlines", self.offline):
            props = feature["properties"]
            name = props.get("name") or ""
            if (props.get("scalerank") or 99) > s["river_max_scalerank"]: continue
            if "Canal" in name or "Csatorna" in name: continue   # modern canals
            features.append((name in s["river_widths"], name, feature["geometry"]))
        for name, points in s["river_extra"]:
            features.append((name in s["river_widths"], name, {"type": "LineString", "coordinates": points}))
        for major, name, geometry in sorted(features, key=lambda f: f[0]):
            lines = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
            for line in lines: self.draw_river(line, name if major else None)

    def draw_river(self, line, major_name):
        s, grid = self.settings, self.grid
        x, y = self.proj.forward(np.array([p[1] for p in line]), np.array([p[0] for p in line]))
        cells = [grid.hex_of(a, b) for a, b in zip(x, y)]
        every, walked = s["river_shallows_every"], 0
        for index, (a, b) in enumerate(zip(cells, cells[1:])):
            width = river_width(s["river_widths"][major_name], (line[index][0] + line[index + 1][0]) / 2.0) if major_name else 0
            major = width > 0
            if not major: width, kind = s["river_minor_width"], s["river_minor_terrain"]
            else: kind = RIVER
            width = max(width, RIVER_MIN_WIDTH)
            for centre in hex_line(a, b):
                walked += 1
                here = kind
                if kind == STREAM and every and walked % every == every // 2: here = SHALLOWS
                for cell in thick_cells(centre, width):
                    if not grid.contains(cell) or self.at(cell) == SEA: continue
                    if self.major_river[cell[1], cell[0]] or self.lake[cell[1], cell[0]]: continue
                    if here == STREAM and self.at(cell) == SHALLOWS: continue
                    self.put(cell, here)
                    if major: self.major_river[cell[1], cell[0]] = True

    def add_banks(self):
        """岸 hexes on the land along the 河 rivers. A hex next to a base, a road or a ford stays open."""
        if not self.settings["river_banks"]: return
        grid, open_kinds = self.grid, (CITY, PORT, GATE, ROAD, FORD)
        banks = []
        for row in range(grid.height):
            for col in range(grid.width):
                cell = (col, row)
                if self.at(cell) not in NATURAL_LAND or cell in self.reserved or cell in self.open_ways: continue
                around = [n for n in neighbours(cell) if grid.contains(n)]
                if not any(self.at(n) == RIVER for n in around): continue
                if any(self.at(n) in open_kinds for n in around): continue
                banks.append(cell)
        for cell in banks: self.put(cell, SHORE)

    # -- the sites --

    def site_cell(self, site):
        x, y = self.proj.forward(np.array(site["lat"]), np.array(site["lon"]))
        return self.grid.hex_of(float(x), float(y))

    def nearest(self, start, accept, radius=8):
        if accept(start): return start
        for r in range(1, radius + 1):
            for cell in ring(start, r):
                if self.grid.contains(cell) and accept(cell): return cell
        return None

    def at(self, cell):
        return self.terrain[cell[1], cell[0]]

    def put(self, cell, kind):
        if not self.grid.contains(cell): return
        self.terrain[cell[1], cell[0]] = kind
        if hasattr(self, "road_costs"): self.road_costs[cell[1]][cell[0]] = self.road_cost_of(cell)

    def place_sites(self):
        self.make_cost_table()
        self.base_cell = {}
        self.reserved = set()
        self.open_ways = set()
        is_land = lambda c: self.grid.contains(c) and self.at(c) not in WATER and self.at(c) != CLIFF
        for city in self.cities:
            centre = self.nearest(self.site_cell(city), is_land)
            if centre is None: sys.exit("no land near the city %s" % city["key"])
            self.base_cell[city["id"]] = centre
            for cell in [centre] + neighbours(centre):
                self.put(cell, CITY)
                self.reserved.add(cell)
        self.city_zone = set()
        for city in self.cities: self.city_zone.update(disc(self.base_cell[city["id"]], CITY_CLEAR_HEXES))
        self.gate_centres, self.pass_report = set(), []
        self.carve_valleys(is_land)
        self.find_big_barriers()
        for site in self.sites:
            if site["kind"] == "pass": self.place_pass(site, is_land)
        self.place_ports()
        for site in self.sites:
            if site["kind"] == "city": continue
            owner = site["owner"]
            if owner in ("", "-"):
                cell = self.base_cell[site["id"]] if site["id"] in self.base_cell else self.site_cell(site)
                owner = min(self.cities, key=lambda c: hex_distance(cell, self.base_cell[c["id"]]))["id"]
            site["owner_city"] = int(owner)
        self.prune_ports()

    def placed_sites(self):
        """The sites on the map: each site except the ports that prune_ports removed."""
        return [s for s in self.sites if not s.get("removed")]

    # -- the passes --

    def carve_valleys(self, is_land):
        """Opens a valley of 荒地 through the cliffs where the cliffs make a long detour between two neighbour cities,
        or between the two cities of a pass. The valley takes the route through the fewest and lowest cliff hexes."""
        if not self.settings["cliff_relief_share"]: return
        self.valley_hexes = 0   # the hexes of the valleys go into open_ways: add_banks keeps them open
        # The land around a city stays open, so that the city has room for its slots (4 to 9 hexes away).
        for city in self.cities:
            for cell in disc(self.base_cell[city["id"]], CITY_OPEN_HEXES):
                if self.grid.contains(cell) and self.at(cell) == CLIFF:
                    self.put(cell, WASTE)
                    self.valley_hexes += 1
        # First the valley of each pass: the historical road goes through the position of the pass.
        for site in self.sites:
            if site["kind"] != "pass": continue
            here = self.nearest(self.site_cell(site), lambda c: self.at(c) not in WATER + (STREAM, SHORE))
            if here is None: continue
            a, b = self.pass_cities(site, here)
            first = self.land_path(self.base_cell[a], here, cliff_cost=VALLEY_CLIFF_COST)
            second = self.land_path(here, self.base_cell[b], cliff_cost=VALLEY_CLIFF_COST)
            if first is None or second is None: continue
            for cell in first + second:
                if self.at(cell) == CLIFF:
                    self.put(cell, WASTE)
                    self.valley_hexes += 1
            self.open_ways.update(first + second)
        # Then a valley between two neighbour cities where the cliffs make a long detour.
        for a, b in self.road_pairs():
            start, goal = self.base_cell[a], self.base_cell[b]
            through = self.land_path(start, goal, cliff_cost=VALLEY_CLIFF_COST)
            # A route far longer than the straight line goes around a river or a sea: no valley helps it.
            if through is None or len(through) > 2.2 * hex_distance(start, goal) + 6: continue
            cliffs =[cell for cell in through if self.at(cell) == CLIFF]
            if not cliffs: continue
            around = self.land_path(start, goal)
            if around is not None and len(around) <= VALLEY_DETOUR * len(through) + 5: continue
            for cell in cliffs: self.put(cell, WASTE)
            self.open_ways.update(through)
            self.valley_hexes += len(cliffs)
        # The rivers without fords and the cliffs can still cut a group of cities from the others.
        # A valley then joins the nearest pair of cities of two groups, around the rivers if necessary.
        for _ in range(len(self.cities)):
            groups = self.city_groups()
            if len(set(groups.values())) == 1: break
            main = groups[self.cities[0]["id"]]
            tries = sorted((hex_distance(self.base_cell[a["id"]], self.base_cell[b["id"]]), a["id"], b["id"])
                           for a in self.cities for b in self.cities if groups[a["id"]] == main and groups[b["id"]] != main)
            for _, a, b in tries[:VALLEY_GROUP_TRIES]:
                through = self.land_path(self.base_cell[a], self.base_cell[b], cliff_cost=VALLEY_CLIFF_COST)
                if through is None: continue
                cliffs = [cell for cell in through if self.at(cell) == CLIFF]
                for cell in cliffs: self.put(cell, WASTE)
                for cell in through:
                    if self.at(cell) == STREAM: self.put(cell, SHALLOWS)   # a land unit wades across here
                self.open_ways.update(through)
                self.valley_hexes += len(cliffs)
                break
            else:
                break

    def city_groups(self):
        """{city id: land group} before the gates: the cities that a land unit joins have the same group."""
        group_of = {}
        for city in self.cities:
            start = self.base_cell[city["id"]]
            if start in group_of: continue
            seen, stack = {start}, [start]
            while stack:
                cell = stack.pop()
                for n in neighbours(cell):
                    if n not in seen and self.grid.contains(n) and COST.get(self.at(n)):
                        seen.add(n)
                        stack.append(n)
            for cell in seen: group_of[cell] = city["id"]
        return {c["id"]: group_of[self.base_cell[c["id"]]] for c in self.cities}

    def find_big_barriers(self):
        """Marks the barrier hexes of a group of BIG_BARRIER_HEXES or more, or of a group at the edge of the map."""
        grid = self.grid
        self.big_barrier = np.zeros((grid.height, grid.width), dtype=bool)
        seen = np.zeros((grid.height, grid.width), dtype=bool)
        for row in range(grid.height):
            for col in range(grid.width):
                if seen[row, col] or self.at((col, row)) not in BARRIER: continue
                group, stack, edge = [], [(col, row)], False
                seen[row, col] = True
                while stack:
                    cell = stack.pop()
                    group.append(cell)
                    for n in neighbours(cell):
                        if not grid.contains(n):
                            edge = True
                        elif not seen[n[1], n[0]] and self.at(n) in BARRIER:
                            seen[n[1], n[0]] = True
                            stack.append(n)
                if edge or len(group) >= BIG_BARRIER_HEXES:
                    for cell in group: self.big_barrier[cell[1], cell[0]] = True

    def land_path(self, start, goal, closed=frozenset(), cliff_cost=0, army=False):
        """The cheapest land route before the roads, as a list of hexes, or None. A gate is open only at its centre.
        With cliff_cost, the route can go through a cliff hex at that cost. With army, the route uses only the hexes
        that a land unit enters: it crosses a river only at a ford or at shallows."""
        frontier, came, best = [(0.0, start)], {start: None}, {start: 0.0}
        while frontier:
            _, cell = heapq.heappop(frontier)
            if cell == goal: break
            for nxt in neighbours(cell):
                if not self.grid.contains(nxt) or nxt in closed: continue
                step = (COST.get(self.at(nxt), 0) if army else self.road_cost(nxt)) or (cliff_cost if self.at(nxt) == CLIFF else 0)
                if not step or (self.at(nxt) == GATE and nxt not in self.gate_centres): continue
                cost = best[cell] + step
                if cost < best.get(nxt, 1e18):
                    best[nxt], came[nxt] = cost, cell
                    heapq.heappush(frontier, (cost + 4 * hex_distance(nxt, goal), nxt))
        if goal not in came: return None
        path, cell = [], goal
        while cell is not None:
            path.append(cell)
            cell = came[cell]
        return path[::-1]

    def pass_cities(self, site, cell):
        """The city that the pass guards and the city on the other side, from the owner and across columns.
        Without a value, the owner is the nearest city, and the other side is the nearest city behind the gate."""
        centre = lambda c: self.base_cell[c["id"]]
        owner = site.get("owner", "-")
        owner = int(owner) if owner not in ("", "-") else min(self.cities, key=lambda c: hex_distance(cell, centre(c)))["id"]
        across = site.get("across", "-")
        if across not in ("", "-"): return owner, int(across)
        gx, gy = hex_xy(cell)
        ox, oy = hex_xy(self.base_cell[owner])
        behind = []
        for city in self.cities:
            if city["id"] == owner: continue
            cx, cy = hex_xy(centre(city))
            if (ox - gx) * (cx - gx) + (oy - gy) * (cy - gy) < 0: behind.append(city)
        pool = behind or [c for c in self.cities if c["id"] != owner]
        return owner, min(pool, key=lambda c: hex_distance(cell, centre(c)))["id"]

    def wall(self, start, gate, road, sign, forbidden):
        """The cheapest line of hexes from the side hex start to a large barrier, on the side sign of the road
        through the gate. Hexes of low ground cost more, so the line follows the ridges. Returns (cost, hexes) or None."""
        gx, gy = hex_xy(gate)
        def side(cell):
            x, y = hex_xy(cell)
            return road[0] * (y - gy) - road[1] * (x - gx)
        frontier, came, best = [(0.0, start)], {start: None}, {start: 0.0}
        while frontier:
            cost, cell = heapq.heappop(frontier)
            if cost > best[cell]: continue
            if any(not self.grid.contains(n) or self.big_barrier[n[1], n[0]] for n in neighbours(cell) if n != gate):
                line = []
                while cell != start:
                    line.append(cell)
                    cell = came[cell]
                return cost, line
            for nxt in neighbours(cell):
                if not self.grid.contains(nxt) or nxt in forbidden or self.at(nxt) not in WALL_KINDS: continue
                if side(nxt) * sign <= 0: continue
                height = float(self.elev_mean[nxt[1], nxt[0]])
                low = min(1.0, max(0.0, (WALL_HIGH_METRES - height) / (WALL_HIGH_METRES - WALL_LOW_METRES)))
                new = cost + 1.0 + WALL_LOW_EXTRA * low
                if new <= WALL_MAX_COST and new < best.get(nxt, 1e18):
                    best[nxt], came[nxt] = new, cell
                    heapq.heappush(frontier, (new, nxt))
        return None

    def choose_gate(self, route, near):
        """The hex of the route with the shortest walls, near the position of the pass. Returns (score, gate, axis, walls) or None."""
        cities = [self.base_cell[c["id"]] for c in self.cities]
        corridor = set(route)
        for cell in route: corridor.update(neighbours(cell))
        found_gates = []
        for index in range(1, len(route) - 1):
            gate = route[index]
            if hex_distance(gate, near) > PASS_SEARCH_HEXES or self.at(gate) not in NATURAL_LAND: continue
            if min(hex_distance(gate, c) for c in cities) < GATE_CITY_HEXES: continue
            if any(hex_distance(gate, other) < GATE_GATE_HEXES for other in self.gate_centres): continue
            ring_cells = neighbours(gate)
            if any(not self.grid.contains(n) or n in self.reserved for n in ring_cells): continue
            (ax, ay), (bx, by) = hex_xy(route[index - 1]), hex_xy(route[index + 1])
            road = (bx - ax, by - ay)
            length = math.hypot(*road)
            gx, gy = hex_xy(gate)
            def across_road(d):
                x, y = hex_xy(neighbour(gate, d))
                return abs((x - gx) * road[0] + (y - gy) * road[1]) / length
            axis = min(range(3), key=across_road)
            forbidden = corridor | self.city_zone | self.reserved | set(ring_cells) | {gate}
            walls, score = [], PASS_MOVE_COST * hex_distance(gate, near)
            for direction in (axis, axis + 3):
                start = neighbour(gate, direction)
                x, y = hex_xy(start)
                sign = 1 if road[0] * (y - gy) - road[1] * (x - gx) > 0 else -1
                found = self.wall(start, gate, road, sign, forbidden)
                if found is None: break
                score += found[0]
                walls.append(found[1])
            if len(walls) == 2: found_gates.append((score, gate, axis, walls))
        # The cheapest gap with a purpose: without the gate, the route is much longer or closed (check_gates).
        # If no gap has a purpose, the gap with the longest detour. The purpose check then reports the gate.
        found_gates.sort(key=lambda g: g[0])
        start, goal = route[0], route[-1]
        best, best_length = None, -1
        for score, gate, axis, walls in found_gates[:GATE_DETOUR_TRIES]:
            closed = frozenset([gate, neighbour(gate, axis), neighbour(gate, axis + 3)] + walls[0] + walls[1])
            detour = self.land_path(start, goal, closed=closed, army=True)
            if detour is None or gate_has_purpose(len(route), len(detour)): return score, gate, axis, walls
            if len(detour) > best_length: best, best_length = (score, gate, axis, walls), len(detour)
        return best

    def place_pass(self, site, is_land):
        """A gate at the narrowest gap of the route between the two cities of the pass, with walls to the barriers."""
        near = self.nearest(self.site_cell(site), lambda c: is_land(c) and c not in self.reserved) or self.site_cell(site)
        owner, across = self.pass_cities(site, near)
        site["guards"] = (owner, across)   # the purpose check uses the same two cities
        start, goal = self.base_cell[owner], self.base_cell[across]
        route = self.land_path(start, goal)
        chosen = self.choose_gate(route, near) if route else None
        if chosen is None:
            self.place_pass_alone(site, near)
            self.pass_report.append("pass %s: no gap with walls between %s and %s; a gate without walls" %
                                    (site["key"], self.sites[owner]["key"], self.sites[across]["key"]))
            return
        _, centre, axis, walls = chosen
        self.base_cell[site["id"]] = centre
        self.gate_centres.add(centre)
        for cell in [centre, neighbour(centre, axis), neighbour(centre, axis + 3)]:
            self.put(cell, GATE)   # the engine makes the two side gate hexes cliffs (F-151)
            self.reserved.add(cell)
            self.big_barrier[cell[1], cell[0]] = cell != centre
        for line in walls:
            for cell in line:
                self.put(cell, CLIFF)
                self.big_barrier[cell[1], cell[0]] = True
        self.open_passage(centre, axis)
        open_route = self.land_path(start, goal)
        detour = self.land_path(start, goal, closed=frozenset([centre]))
        self.pass_report.append("pass %s: guards %s from %s, %d hexes from its position, walls %d + %d hexes, route %d hexes, without the gate %s" %
                                (site["key"], self.sites[owner]["key"], self.sites[across]["key"], hex_distance(centre, near),
                                 len(walls[0]), len(walls[1]), len(open_route) if open_route else 0, "%d hexes" % len(detour) if detour else "no route"))

    def place_pass_alone(self, site, centre):
        """The gate of the first packs: a short wall along the highest pair of opposite neighbours."""
        self.base_cell[site["id"]] = centre
        self.gate_centres.add(centre)
        height = lambda c: self.elev_mean[c[1], c[0]] if self.grid.contains(c) else -1e9
        axis = max(range(3), key=lambda d: height(neighbour(centre, d)) + height(neighbour(centre, d + 3)))
        self.put(centre, GATE)
        self.reserved.add(centre)
        for direction in (axis, axis + 3):
            cell = centre
            for step in range(1, 4):
                cell = neighbour(cell, direction)
                if cell in self.reserved: break
                self.put(cell, GATE if step == 1 else CLIFF)   # the engine makes the two side gate hexes cliffs
                self.reserved.add(cell)
        self.open_passage(centre, axis)

    def open_passage(self, centre, axis):
        """Units go through the four neighbours of the gate that are not on its axis."""
        for direction in range(6):
            if direction % 3 == axis: continue
            cell = neighbour(centre, direction)
            if self.grid.contains(cell) and self.at(cell) in BARRIER and cell not in self.reserved: self.put(cell, WASTE)

    # -- the ports --

    def find_water_bodies(self):
        """The id of the water body of each 河 and 海 hex (-1 for land), and the bodies with sea."""
        grid = self.grid
        self.water_body = np.full((grid.height, grid.width), -1, dtype=np.int32)
        sea_bodies = set()
        count = 0
        for row in range(grid.height):
            for col in range(grid.width):
                if self.water_body[row, col] >= 0 or self.at((col, row)) not in WATER: continue
                stack = [(col, row)]
                self.water_body[row, col] = count
                while stack:
                    cell = stack.pop()
                    if self.at(cell) == SEA: sea_bodies.add(count)
                    for n in neighbours(cell):
                        if grid.contains(n) and self.water_body[n[1], n[0]] < 0 and self.at(n) in WATER:
                            self.water_body[n[1], n[0]] = count
                            stack.append(n)
                count += 1
        return sea_bodies

    def bodies_next_to(self, cell):
        return {int(self.water_body[n[1], n[0]]) for n in neighbours(cell) if self.grid.contains(n) and self.water_body[n[1], n[0]] >= 0}

    def place_ports(self):
        """Each port on the shore nearest to its position. A port on a closed lake moves to water that leads to other ports."""
        ports = [s for s in self.sites if s["kind"] == "port"]
        self.port_ground = {}   # the terrain under each port, for a port that moves or goes
        sea_bodies = self.find_water_bodies()
        def shore(cell, good=None):
            if not self.grid.contains(cell) or cell in self.reserved or self.at(cell) in WATER or self.at(cell) in (CLIFF, GATE): return False
            bodies = self.bodies_next_to(cell)
            return bool(bodies) if good is None else bool(bodies & good)
        first = {}
        for site in ports:
            first[site["id"]] = self.nearest(self.site_cell(site), shore, radius=10)
            if first[site["id"]] is None: sys.exit("no shore near the port %s" % site["key"])
        ports_of = {}
        for cell in first.values():
            for body in self.bodies_next_to(cell): ports_of[body] = ports_of.get(body, 0) + 1
        self.good_bodies = {body for body, count in ports_of.items() if count >= (2 if body in sea_bodies else PORT_LAKE_PORTS)}
        self.port_report = []
        for site in ports:
            cell = first[site["id"]]
            if not (self.bodies_next_to(cell) & self.good_bodies):
                moved = self.nearest(cell, lambda c: shore(c, self.good_bodies), radius=PORT_MOVE_HEXES)
                if moved is None: sys.exit("the port %s is on a closed lake, and no good shore is near. Move it in sites.tsv." % site["key"])
                self.port_report.append("port %s: moved %d hexes from a closed lake to water with other ports" % (site["key"], hex_distance(cell, moved)))
                cell = moved
            if cell in self.reserved:   # another port took the hex first
                cell = self.nearest(cell, lambda c: shore(c, self.good_bodies), radius=PORT_MOVE_HEXES)
                if cell is None: sys.exit("the port %s has no free shore. Move it in sites.tsv." % site["key"])
            self.base_cell[site["id"]] = cell
            self.port_ground[cell] = self.at(cell)
            self.put(cell, PORT)
            self.reserved.add(cell)

    def port_far_from_cities(self, cell):
        return all(hex_distance(cell, self.base_cell[c["id"]]) >= PORT_CITY_HEXES for c in self.cities)

    def prune_ports(self):
        """The player, 2026-11-05: a port must be PORT_CITY_HEXES or more from the centre of each city.
        If a nearer city is on the same water, that city launches its ships itself, so the port goes.
        Else the port moves along the shore, or goes if no far shore is near.
        A port that goes keeps its id in bases.tsv with present = no."""
        def far_shore(c, waters):
            if not self.grid.contains(c) or c in self.reserved or self.at(c) in WATER or self.at(c) in (CLIFF, GATE): return False
            return bool(self.bodies_next_to(c) & waters) and self.port_far_from_cities(c)
        for site in self.sites:
            if site["kind"] != "port": continue
            cell = self.base_cell[site["id"]]
            near = [c for c in self.cities if hex_distance(cell, self.base_cell[c["id"]]) < PORT_CITY_HEXES]
            port_waters = self.bodies_next_to(cell) & self.good_bodies
            on_water = [c for c in near if self.city_waters(c) & port_waters]
            reason = None
            if on_water:
                reason = "%d hexes from %s, a city on the same water" % (hex_distance(cell, self.base_cell[on_water[0]["id"]]), on_water[0]["key"])
            elif near:
                moved = self.nearest(cell, lambda c: far_shore(c, port_waters), radius=PORT_MOVE_HEXES)
                if moved is None: reason = "no shore is %d hexes or more from each city" % PORT_CITY_HEXES
                else:
                    self.port_report.append("port %s: moved %d hexes, away from a city" % (site["key"], hex_distance(cell, moved)))
                    self.free_port_cell(cell)
                    self.base_cell[site["id"]] = moved
                    self.port_ground[moved] = self.at(moved)
                    self.put(moved, PORT)
                    self.reserved.add(moved)
            if reason:
                self.port_report.append("port %s: removed, %s" % (site["key"], reason))
                self.free_port_cell(cell)
                del self.base_cell[site["id"]]
                site["removed"] = True

    def free_port_cell(self, cell):
        self.put(cell, self.port_ground.pop(cell))
        self.reserved.discard(cell)

    def on_water(self, city):
        """True if water of the ports is at a distance of 2 from the centre, or next to open land at that distance."""
        return bool(self.city_waters(city))

    def city_waters(self, city):
        """The bodies of water of the ports that the units of the city enter from the city (on_water)."""
        good = lambda c: self.grid.contains(c) and self.at(c) in WATER and int(self.water_body[c[1], c[0]]) in self.good_bodies
        waters = set()
        for cell in ring(self.base_cell[city["id"]], 2):
            if good(cell): waters.add(int(self.water_body[cell[1], cell[0]]))
            if self.grid.contains(cell) and COST.get(self.at(cell)):
                waters.update(int(self.water_body[n[1], n[0]]) for n in neighbours(cell) if good(n))
        return waters

    # -- the roads --

    def road_cost(self, cell):
        """The cost of a hex for the road searches, from the table of make_cost_table."""
        return self.road_costs[cell[1]][cell[0]]

    def make_cost_table(self):
        """Nested lists of road_cost_of for each hex. A read of a list is much faster than a read of a numpy cell.
        put keeps the table current after this call."""
        self.road_costs = [[self.road_cost_of((col, row)) for col in range(self.grid.width)] for row in range(self.grid.height)]

    def road_cost_of(self, cell):
        kind = self.at(cell)
        if kind == STREAM: return ROAD_RIVER_COST
        if kind == RIVER:
            if self.lake[cell[1], cell[0]] or not self.settings["river_fords"]: return 0
            return ROAD_RIVER_COST
        if kind == ROAD: return 2
        cost = COST.get(kind, 0)
        if cost and kind in NATURAL_LAND: cost += min(6, max(0.0, self.elev_mean[cell[1], cell[0]]) / 250.0)
        return cost

    def find_path(self, start, goal):
        grid = self.grid
        ends = (self.base_of_cell.get(start), self.base_of_cell.get(goal))
        frontier = [(0.0, start)]
        came, cost_so_far = {start: None}, {start: 0.0}
        while frontier:
            _, cell = heapq.heappop(frontier)
            if cell == goal: break
            for nxt in neighbours(cell):
                if not grid.contains(nxt): continue
                step = self.road_cost(nxt)
                if step == 0: continue
                kind = self.at(nxt)
                if kind == GATE and nxt not in self.gate_centres: continue
                if kind in (CITY, PORT) and self.base_of_cell.get(nxt) not in ends: continue
                new_cost = cost_so_far[cell] + step
                if nxt not in cost_so_far or new_cost < cost_so_far[nxt]:
                    cost_so_far[nxt] = new_cost
                    came[nxt] = cell
                    heapq.heappush(frontier, (new_cost + 2 * hex_distance(nxt, goal), nxt))
        if goal not in came: return None
        path, cell = [], goal
        while cell is not None:
            path.append(cell)
            cell = came[cell]
        return path[::-1]

    def lay_road(self, path):
        for cell in path:
            kind = self.at(cell)
            if kind in (RIVER, STREAM): self.put(cell, FORD)
            elif kind in NATURAL_LAND: self.put(cell, ROAD)

    def road_pairs(self):
        """The pairs of neighbour cities that a road can join, the nearest pair first."""
        pairs = set()
        for city in self.cities:
            others = sorted((great_circle_km(city, o), o["id"]) for o in self.cities if o is not city)
            limit = max(others[0][0] * 1.7, others[2][0])
            for km, other in others[:4]:
                if km <= limit: pairs.add(tuple(sorted((city["id"], other))))
        return sorted(pairs, key=lambda p: great_circle_km(self.sites[p[0]], self.sites[p[1]]))

    def add_roads(self):
        self.gate_centres = {self.base_cell[s["id"]] for s in self.sites if s["kind"] == "pass"}
        self.base_of_cell = {}
        for city in self.cities:
            centre = self.base_cell[city["id"]]
            for cell in [centre] + neighbours(centre): self.base_of_cell[cell] = city["id"]
        for site in self.placed_sites():
            if site["kind"] != "city": self.base_of_cell[self.base_cell[site["id"]]] = site["id"]
        self.roads = []
        for a, b in self.road_pairs():
            path = self.find_path(self.base_cell[a], self.base_cell[b])
            # A road that is much longer than the straight line goes around a sea: the two cities trade by ship.
            if path and len(path) <= 2.2 * hex_distance(self.base_cell[a], self.base_cell[b]) + 6:
                self.lay_road(path)
                self.roads.append((a, b))
        for site in self.placed_sites():
            if site["kind"] in ("port", "pass"):
                start, goal = self.base_cell[site["id"]], self.base_cell[site["owner_city"]]
                path = self.find_path(start, goal)
                if path and len(path) <= 3 * hex_distance(start, goal) + 6: self.lay_road(path)

    # -- the regions, the slots and the flags --

    def make_regions(self):
        grid = self.grid
        region = np.full((grid.height, grid.width), 255, dtype=np.uint8)
        frontier = []
        for site in self.placed_sites():
            cell = self.base_cell[site["id"]]
            start = 0.0 if site["kind"] == "city" else 18.0   # a pass or a port has a small region
            for c in [cell] + (neighbours(cell) if site["kind"] == "city" else []): heapq.heappush(frontier, (start, c, site["id"]))
        done = set()
        while frontier:
            cost, cell, owner = heapq.heappop(frontier)
            if cell in done or not grid.contains(cell): continue
            done.add(cell)
            region[cell[1], cell[0]] = owner
            for nxt in neighbours(cell):
                if not grid.contains(nxt) or nxt in done: continue
                kind = self.at(nxt)
                step = COST.get(kind, 0) or (6 if kind == RIVER else 0)
                if step: heapq.heappush(frontier, (cost + step, nxt, owner))
        # The sea belongs to the nearest port. The other hexes (cliffs, islands) take the region of a neighbour.
        frontier = [(0, self.base_cell[s["id"]], s["id"]) for s in self.placed_sites() if s["kind"] == "port"]
        seen = set()
        while frontier:
            cost, cell, owner = heapq.heappop(frontier)
            if cell in seen or not grid.contains(cell): continue
            seen.add(cell)
            if self.at(cell) == SEA: region[cell[1], cell[0]] = owner
            for nxt in neighbours(cell):
                if grid.contains(nxt) and nxt not in seen and self.at(nxt) == SEA: heapq.heappush(frontier, (cost + 1, nxt, owner))
        while True:
            missing = np.argwhere(region == 255)
            if not len(missing): break
            changed = False
            for row, col in missing:
                for n in neighbours((int(col), int(row))):
                    if grid.contains(n) and region[n[1], n[0]] != 255:
                        region[row, col] = region[n[1], n[0]]
                        changed = True
                        break
            if not changed: sys.exit("some hexes have no region")
        # The hexes of a pass or a port are always in its own region. The engine makes the side hexes of a gate cliffs
        # only in the region of the gate (F-151).
        for site in self.placed_sites():
            if site["kind"] == "city": continue
            cell = self.base_cell[site["id"]]
            for c in [cell] + [n for n in neighbours(cell) if self.grid.contains(n) and self.at(n) == GATE]:
                region[c[1], c[0]] = site["id"]
        self.region = region

    def place_slots(self):
        """The slots of a city: clusters of up to seven hexes (a centre and its ring), 4 to 9 hexes from the city."""
        self.slot = np.zeros((self.grid.height, self.grid.width), dtype=np.uint8)
        self.slot_count = {}
        def buildable(c, city):
            return (self.grid.contains(c) and self.at(c) in NATURAL_LAND and self.at(c) != SWAMP
                    and self.region[c[1], c[0]] == city and not self.slot[c[1], c[0]])
        for city in self.cities:
            centre, want, placed, angles = self.base_cell[city["id"]], int(city["slots"]), 0, []
            for distance in range(4, 10):
                for cluster in ring(centre, distance):
                    if placed >= want: break
                    if not buildable(cluster, city["id"]): continue
                    members = [cluster] + [n for n in neighbours(cluster) if buildable(n, city["id"]) and hex_distance(n, centre) >= 3]
                    if len(members) < min(5, want - placed): continue
                    angle = math.atan2(cluster[1] - centre[1], cluster[0] - centre[0])
                    if any(abs(math.remainder(angle - a, 2 * math.pi)) < 1.2 for a in angles): continue
                    angles.append(angle)
                    for cell in members[:want - placed]:
                        self.slot[cell[1], cell[0]] = 1
                        self.put(cell, EARTH)
                        placed += 1
            self.slot_count[city["id"]] = placed

    def place_bandits(self):
        self.bandit = np.zeros((self.grid.height, self.grid.width), dtype=np.uint8)
        for city in self.cities:
            centre, placed = self.base_cell[city["id"]], 0
            for distance in range(9, 16, 3):
                for cell in ring(centre, distance)[::5]:
                    if placed < 3 and self.grid.contains(cell) and self.at(cell) in (FOREST, WASTE) and self.region[cell[1], cell[0]] == city["id"]:
                        self.bandit[cell[1], cell[0]] = 1
                        placed += 1

    def build(self):
        self.sample_layers()
        self.classify()
        self.add_rivers()
        self.place_sites()
        self.add_roads()
        self.add_banks()
        self.make_regions()
        self.place_slots()
        self.place_bandits()
        self.check_gates()

    def check_gates(self):
        """The purpose check of each gate on the finished map, with the roads: the gate hex closed, the route between
        the two cities of the pass must grow by GATE_PURPOSE_FACTOR or GATE_PURPOSE_EXTRA hexes, or close."""
        self.gate_report, self.weak_gates = [], []
        for site in self.sites:
            if site["kind"] != "pass": continue
            centre = self.base_cell[site["id"]]
            owner, across = site["guards"]
            start, goal = self.base_cell[owner], self.base_cell[across]
            route = self.land_path(start, goal, army=True)
            detour = self.land_path(start, goal, closed=frozenset([centre]), army=True)
            if route is None:
                purpose = False
                result = "no route through the gate"
            elif detour is None:
                purpose = True
                result = "route %d hexes, without the gate no route" % len(route)
            else:
                purpose = gate_has_purpose(len(route), len(detour))
                result = "route %d hexes, without the gate %d hexes (x%.2f)" % (len(route), len(detour), len(detour) / len(route))
            self.gate_report.append("gate %s (%s - %s): %s: %s" % (site["key"], self.sites[owner]["key"], self.sites[across]["key"],
                                                                   result, "purpose" if purpose else "NO PURPOSE"))
            if not purpose: self.weak_gates.append(site["key"])

    # -- the output --

    def land_height(self, row, col):
        value = int(round(12 + max(0.0, float(self.elev_mean[row, col])) / 15.0))
        if self.terrain[row, col] == CLIFF: value = max(value, 90)
        return min(255, value)

    def height_value(self, row, col):
        kind = self.terrain[row, col]
        if kind == SEA: return 0
        if kind in FLOWING: return self.water_height[row, col]
        if kind == PORT: return self.port_height(row, col)
        return self.land_height(row, col)

    def shore_height(self, cell):
        """The target height of an inland water hex: a little below the lowest land next to it. None without land."""
        shore = [self.land_height(n[1], n[0]) for n in neighbours(cell)
                 if self.grid.contains(n) and self.at(n) not in FLOWING + (SEA, PORT)]
        return max(0, min(shore) - WATER_BELOW_SHORE) if shore else None

    def make_water_heights(self):
        """The heights of the inland water. A priority flood upstream from the sea gives each water hex the hex below it,
        toward the sea. A closed basin starts at its lowest hex. Then each hex takes the lowest shore height of the water
        that flows into it, with its own hex. Thus the water falls toward the sea, and no water stands above its shores.
        A river in a gorge stays at the height of the basin above the gorge. A lake is flat at its lowest height."""
        grid = self.grid
        self.water_height = np.zeros((grid.height, grid.width), dtype=np.int32)
        flowing = {(col, row) for row in range(grid.height) for col in range(grid.width) if self.at((col, row)) in FLOWING}
        target = {cell: self.shore_height(cell) for cell in flowing}
        lakes, seen = [], set()
        for cell in flowing:
            if cell in seen or not self.lake[cell[1], cell[0]]: continue
            group, stack = [cell], [cell]
            seen.add(cell)
            while stack:
                for n in neighbours(stack.pop()):
                    if n in flowing and n not in seen and self.lake[n[1], n[0]]:
                        seen.add(n)
                        group.append(n)
                        stack.append(n)
            lakes.append(group)
        # The flood: below[cell] is the next hex toward the sea, order is the order from the sea upstream.
        below, order, root = {}, [], (-1, -1)   # root: the parent of a hex next to the sea, or of the start of a basin
        frontier = [(target[c] or 0, c, root) for c in flowing
                    if any(self.grid.contains(n) and self.at(n) == SEA for n in neighbours(c))]
        heapq.heapify(frontier)
        rest, next_rest = sorted(flowing, key=lambda c: (target[c] or 0, c)), 0
        while True:
            while frontier:
                height, cell, parent = heapq.heappop(frontier)
                if cell in below: continue
                below[cell] = parent if parent != root else None
                order.append(cell)
                for n in neighbours(cell):
                    if n in flowing and n not in below: heapq.heappush(frontier, (max(height, target[n] or 0), n, cell))
            while next_rest < len(rest) and rest[next_rest] in below: next_rest += 1
            if next_rest == len(rest): break
            first = rest[next_rest]
            heapq.heappush(frontier, (target[first] or 0, first, root))
        # The lowest shore of the water upstream: from the sources down to the sea.
        unknown = 1 << 30
        height = {cell: unknown if target[cell] is None else target[cell] for cell in flowing}
        for cell in reversed(order):
            parent = below[cell]
            if parent is not None and height[cell] < height[parent]: height[parent] = height[cell]
        # A wide river has hexes without land next to them: they take the height of the hex below them.
        for cell in order:
            if height[cell] == unknown: height[cell] = height[below[cell]] if below[cell] is not None else 0
        for group in lakes:
            level = min(height[c] for c in group)
            for c in group: height[c] = level
        for (col, row), value in height.items(): self.water_height[row, col] = min(255, value)

    def port_height(self, row, col):
        """A port stands PORT_HEIGHT above the level of the lowest water next to it."""
        water = [0 if self.at(n) == SEA else int(self.water_height[n[1], n[0]]) for n in neighbours((col, row))
                 if self.grid.contains(n) and self.at(n) in FLOWING + (SEA,)]
        level = min(water) // HEIGHT_STEP if water else 0
        return level * HEIGHT_STEP + PORT_HEIGHT

    def write(self, pack):
        self.make_water_heights()
        terrain_names = {int(r["id"]): r["name_zh"] for r in read_tsv(os.path.join(ROOT, "data", "placeholder", "terrain.tsv"))}
        grid = self.grid
        out = ["x\ty\tterrain\tterrain_name\tregion\ttrap\twater_dir\tslot\tflag6\tflag7\tflood\tzone\tflag10\theight",
               "# Written by misc/make_world_map.py from %s/map/source. Do not edit. Heights: NOAA ETOPO 2022. Land, lakes, rivers, deserts: Natural Earth." % os.path.basename(pack),
               "# %d x %d hexes, %.1f km from the centre of a hex to the centre of the next hex." % (grid.width, grid.height, grid.spacing)]
        for col in range(grid.width):
            for row in range(grid.height):
                kind = int(self.terrain[row, col])
                out.append("%d\t%d\t%d\t%s\t%d\t0\t0\t%d\t0\t%d\t0\t0\t0\t%d" % (col, row, kind, terrain_names[kind], self.region[row, col],
                                                                             self.slot[row, col], self.bandit[row, col], self.height_value(row, col)))
        os.makedirs(os.path.join(pack, "map"), exist_ok=True)
        write_lines(os.path.join(pack, "map", "hexes.tsv"), out)

        bases = ["id\tname\tkind\tx\ty\thex_count\tcity\tcity_name\tslots"]
        kinds = {"city": "city", "pass": "gate", "port": "port"}
        for site in self.placed_sites():
            cell = self.base_cell[site["id"]]
            owner = site["id"] if site["kind"] == "city" else site["owner_city"]
            hexes = 7 if site["kind"] == "city" else 3 if site["kind"] == "pass" else 1
            bases.append("%d\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%d" % (site["id"], site["name_zh"], kinds[site["kind"]], cell[0], cell[1], hexes, owner,
                                                                self.sites[owner]["name_zh"], self.slot_count.get(site["id"], 0)))
        write_lines(os.path.join(pack, "map", "bases.tsv"), bases)

        port_owners = {s["owner_city"] for s in self.placed_sites() if s["kind"] == "port"}
        port_owners |= {c["id"] for c in self.cities if self.on_water(c)}
        cities = ["id\tkey\tspecialty\tslots\tport\tname_zh\tprice\tname_big5",
                  "# Written by misc/make_world_map.py from map/source/sites.tsv. slots: the slots that the tool placed on the map.",
                  "# port: yes for a city that owns a port or that is on the water. The city can build ships.",
                  "# price: 5 for each city, the middle of the range 3 to 7 (the placeholder pack, Q-155).",
                  "# name_big5: the Big5 bytes of name_zh in hex. The scenario of the pack names the cities with it (docs/MAPS.md, W2).",
                  "# key: the English key of the city. The names of the front end use it (docs/EUROPE.md, E2)."]
        for city in self.cities:
            cities.append("%d\t%s\t%s\t%d\t%s\t%s\t5\t%s" % (city["id"], city["key"], city["specialty"], self.slot_count[city["id"]],
                                                        "yes" if city["id"] in port_owners else "no", city["name_zh"], big5_hex(city["name_zh"], 16)))
        write_lines(os.path.join(pack, "cities.tsv"), cities)

        content_bases = ["id\tkey\tname_zh\tkind\towner_city\tdurability\tstatus\tname_big5\tpresent",
                         "# Written by misc/make_world_map.py from map/source/sites.tsv. durability: 4000 for a pass and 2000 for a port (assumed).",
                         "# name_big5: the Big5 bytes of name_zh in hex, for the names of the scenario (docs/MAPS.md, W2).",
                         "# present: no for a port that the tool left out. Its city is on the water, or no shore is far from the cities."]
        for site in self.sites:
            if site["kind"] == "city": continue
            kind = "pass" if site["kind"] == "none" else site["kind"]
            content_bases.append("%d\t%s\t%s\t%s\t%d\t%d\tassumed\t%s\t%s" % (site["id"], site["key"], site["name_zh"], kind, site["owner_city"],
                                                                            4000 if site["kind"] == "pass" else 2000, big5_hex(site["name_zh"], 25),
                                                                            "no" if site.get("removed") else "yes"))
        write_lines(os.path.join(pack, "bases.tsv"), content_bases)

    def summary(self):
        counts = np.bincount(self.terrain.ravel(), minlength=20)
        city_cells = [self.base_cell[c["id"]] for c in self.cities]
        nearest = sorted(min(hex_distance(a, b) for b in city_cells if b is not a) for a in city_cells)
        return ["grid %d x %d = %d hexes, %.1f km between hex centres" % (self.grid.width, self.grid.height, self.grid.width * self.grid.height, self.grid.spacing),
                "median city spacing %.0f km" % self.city_spacing_km,
                "nearest city: median %.1f hexes (placeholder map: %d)" % ((nearest[20] + nearest[21]) / 2, SAN11_CITY_SPACING),
                "roads between cities: %d" % len(self.roads),
                "slots: %d (asked %d)" % (sum(self.slot_count.values()), sum(int(c["slots"]) for c in self.cities)),
                "terrain: " + ", ".join("%d=%d" % (k, v) for k, v in enumerate(counts) if v),
                "cities on the water: " + " ".join(c["key"] for c in self.cities if self.on_water(c)),
                "cities without a land route to %s: %s" % (self.cities[0]["key"], " ".join(self.cut_off_cities()) or "none")] + self.pass_report + self.gate_report + self.port_report

    def cut_off_cities(self):
        """The keys of the cities that a land unit cannot reach from the first city. The side hexes of a gate are closed."""
        start = self.base_cell[self.cities[0]["id"]]
        seen, stack = {start}, [start]
        while stack:
            cell = stack.pop()
            for n in neighbours(cell):
                if n in seen or not self.grid.contains(n) or not COST.get(self.at(n)): continue
                if self.at(n) == GATE and n not in self.gate_centres: continue
                seen.add(n)
                stack.append(n)
        return [c["key"] for c in self.cities if self.base_cell[c["id"]] not in seen]

    def preview(self, path, pixels=3.0):
        colours = {GRASS: (142, 178, 92), EARTH: (190, 176, 112), SAND: (226, 206, 148), SWAMP: (104, 140, 110), FOREST: (58, 112, 60),
                   RIVER: (70, 130, 200), SEA: (36, 74, 128), WASTE: (150, 128, 96), ROAD: (214, 196, 150), FORD: (120, 170, 210),
                   CLIFF: (96, 84, 76), CITY: (200, 40, 40), PORT: (240, 160, 40), GATE: (150, 40, 160),
                   STREAM: (110, 160, 210), SHORE: (170, 150, 110), SHALLOWS: (150, 190, 215)}
        grid = self.grid
        w = int((grid.width * 1.5 + 0.5) * pixels) + 2
        h = int((grid.height + 0.5) * pixels * math.sqrt(3)) + 2
        image = Image.new("RGB", (w, h), (20, 20, 20))
        draw = ImageDraw.Draw(image)
        corners = [(math.cos(math.radians(60 * k)), math.sin(math.radians(60 * k))) for k in range(6)]
        centre = lambda col, row: (pixels * (1 + 1.5 * col), pixels * math.sqrt(3) * (row + 0.5 + 0.5 * (col & 1)))
        for col in range(grid.width):
            for row in range(grid.height):
                x, y = centre(col, row)
                colour = (230, 220, 60) if self.slot[row, col] else colours.get(int(self.terrain[row, col]), (255, 0, 255))
                draw.polygon([(x + pixels * a, y + pixels * b) for a, b in corners], fill=colour)
        try:
            font = ImageFont.truetype(os.path.join(ROOT, "data", "fonts", "NotoSansCJKtc-Regular.otf"), 13)
        except OSError:
            font = ImageFont.load_default()
        for city in self.cities:
            x, y = centre(*self.base_cell[city["id"]])
            draw.text((x + 6, y - 8), city["name_zh"], fill=(255, 255, 255), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        image.save(path, optimize=True)


def gate_has_purpose(route_hexes, detour_hexes):
    """The purpose rule of a gate: the route without the gate is much longer than the route through it."""
    return detour_hexes >= GATE_PURPOSE_FACTOR * route_hexes or detour_hexes - route_hexes >= GATE_PURPOSE_EXTRA


def big5_hex(text, limit):
    """The Big5 bytes of a name in hex, as the engine reads them. limit: the bytes before the NUL of the record."""
    data = text.encode("big5")
    if len(data) > limit: sys.exit("the name %s has %d Big5 bytes; the record holds %d" % (text, len(data), limit))
    return data.hex().upper()


def write_lines(path, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main():
    args = sys.argv[1:]
    if not args: sys.exit(__doc__)
    pack = os.path.abspath(args[0])
    offline = "--offline" in args
    preview = args[args.index("--preview") + 1] if "--preview" in args else os.path.join(ROOT, "build", "maps", os.path.basename(pack) + ".png")
    os.makedirs(CACHE, exist_ok=True)
    world = WorldMap(read_settings(pack), read_sites(pack), offline)
    print("grid %d x %d, %.1f km between hex centres" % (world.grid.width, world.grid.height, world.spacing_km))
    world.build()
    if world.weak_gates and "--keep-weak-gates" not in args:
        for line in world.gate_report: print(line)
        sys.exit("gates without a purpose: %s. Move them, change their owner and across cities, or leave them out." % " ".join(world.weak_gates))
    world.write(pack)
    world.preview(preview)
    for line in world.summary(): print(line)
    print("preview:", preview)


if __name__ == "__main__": main()
