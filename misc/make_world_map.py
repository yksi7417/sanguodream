"""Makes the map of a content pack from real geography (docs/MAPS.md).

    python misc/make_world_map.py data/medieval_europe [--offline] [--preview FILE]

The tool reads <pack>/map/source/region.tsv and <pack>/map/source/sites.tsv.
It writes <pack>/map/hexes.tsv, <pack>/map/bases.tsv, <pack>/cities.tsv and
<pack>/bases.tsv in the formats of the placeholder pack (formats/map-data.md).

The sources are public domain:
- the height: NOAA ETOPO 2022, 60 arc-second surface elevation, through THREDDS OPeNDAP;
- the land, the lakes, the rivers and the deserts: Natural Earth 1:50m vectors.
The tool keeps the downloads in build/geo_cache. With --offline it uses only that cache.

The grid is the grid of the engine: flat-top hexes in odd-q offset rows.
An odd column is half a hex lower (to the south) than an even column.
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
GRASS, EARTH, SAND, SWAMP, FOREST, RIVER, SEA, WASTE, ROAD, FORD, CLIFF, CITY, PORT, GATE = 0, 1, 2, 3, 5, 7, 8, 9, 10, 12, 15, 16, 17, 18
WATER = (RIVER, SEA)
NATURAL_LAND = (GRASS, EARTH, SAND, SWAMP, FOREST, WASTE)
# The foot cost of terrain.tsv, for the roads and the regions. 0 = closed.
COST = {GRASS: 5, EARTH: 5, SAND: 5, SWAMP: 8, FOREST: 6, WASTE: 6, ROAD: 4, FORD: 4, CITY: 4, PORT: 4, GATE: 4}
ROAD_RIVER_COST = 14      # a road can cross a river: the crossing becomes a ford

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
    km = settings.get("km_per_hex", "-")
    settings["km_per_hex"] = None if km in ("", "-") else float(km)
    return settings


def read_sites(pack):
    sites = read_tsv(os.path.join(pack, "map", "source", "sites.tsv"))
    for index, site in enumerate(sites):
        if int(site["id"]) != index: sys.exit("sites.tsv: row %d has the id %s" % (index, site["id"]))
        site["id"] = index
        site["lat"], site["lon"] = float(site["lat"]), float(site["lon"])
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
        t[self.lake] = RIVER
        self.terrain = t

    def add_rivers(self):
        s, grid = self.settings, self.grid
        for feature in load_ne("ne_50m_rivers_lake_centerlines", self.offline):
            props = feature["properties"]
            name = props.get("name") or ""
            if (props.get("scalerank") or 99) > s["river_max_scalerank"]: continue
            if "Canal" in name or "Csatorna" in name: continue   # modern canals
            geometry = feature["geometry"]
            lines = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
            for line in lines:
                x, y = self.proj.forward(np.array([p[1] for p in line]), np.array([p[0] for p in line]))
                cells = [grid.hex_of(a, b) for a, b in zip(x, y)]
                for a, b in zip(cells, cells[1:]):
                    for cell in hex_line(a, b):
                        if grid.contains(cell) and self.at(cell) != SEA: self.put(cell, RIVER)

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
        if self.grid.contains(cell): self.terrain[cell[1], cell[0]] = kind

    def place_sites(self):
        self.base_cell = {}
        self.reserved = set()
        is_land = lambda c: self.grid.contains(c) and self.at(c) not in WATER and self.at(c) != CLIFF
        for city in self.cities:
            centre = self.nearest(self.site_cell(city), is_land)
            if centre is None: sys.exit("no land near the city %s" % city["key"])
            self.base_cell[city["id"]] = centre
            for cell in [centre] + neighbours(centre):
                self.put(cell, CITY)
                self.reserved.add(cell)
        for site in self.sites:
            if site["kind"] == "pass": self.place_pass(site, is_land)
        for site in self.sites:
            if site["kind"] == "port": self.place_port(site)
        for site in self.sites:
            if site["kind"] == "city": continue
            owner = site["owner"]
            if owner in ("", "-"):
                cell = self.base_cell[site["id"]]
                owner = min(self.cities, key=lambda c: hex_distance(cell, self.base_cell[c["id"]]))["id"]
            site["owner_city"] = int(owner)

    def place_pass(self, site, is_land):
        free = lambda c: is_land(c) and c not in self.reserved
        centre = self.nearest(self.site_cell(site), free) or self.nearest(self.site_cell(site), lambda c: c not in self.reserved)
        self.base_cell[site["id"]] = centre
        # The wall of the pass lies along the highest pair of opposite neighbours. Units go through the other four.
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
        for direction in range(6):
            if direction % 3 == axis: continue
            cell = neighbour(centre, direction)
            if self.grid.contains(cell) and self.at(cell) in (CLIFF, RIVER, SEA) and cell not in self.reserved: self.put(cell, WASTE)

    def place_port(self, site):
        def by_water(cell):
            if not self.grid.contains(cell) or cell in self.reserved or self.at(cell) in WATER or self.at(cell) == CLIFF: return False
            return any(self.grid.contains(n) and self.at(n) in WATER for n in neighbours(cell))
        cell = self.nearest(self.site_cell(site), by_water, radius=10)
        if cell is None: sys.exit("no shore near the port %s" % site["key"])
        self.base_cell[site["id"]] = cell
        self.put(cell, PORT)
        self.reserved.add(cell)

    # -- the roads --

    def road_cost(self, cell):
        kind = self.at(cell)
        if kind == RIVER: return 0 if self.lake[cell[1], cell[0]] else ROAD_RIVER_COST
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
            if kind == RIVER: self.put(cell, FORD)
            elif kind in NATURAL_LAND: self.put(cell, ROAD)

    def add_roads(self):
        self.gate_centres = {self.base_cell[s["id"]] for s in self.sites if s["kind"] == "pass"}
        self.base_of_cell = {}
        for city in self.cities:
            centre = self.base_cell[city["id"]]
            for cell in [centre] + neighbours(centre): self.base_of_cell[cell] = city["id"]
        for site in self.sites:
            if site["kind"] != "city": self.base_of_cell[self.base_cell[site["id"]]] = site["id"]
        pairs = set()
        for city in self.cities:
            others = sorted((great_circle_km(city, o), o["id"]) for o in self.cities if o is not city)
            limit = max(others[0][0] * 1.7, others[2][0])
            for km, other in others[:4]:
                if km <= limit: pairs.add(tuple(sorted((city["id"], other))))
        self.roads = []
        for a, b in sorted(pairs, key=lambda p: great_circle_km(self.sites[p[0]], self.sites[p[1]])):
            path = self.find_path(self.base_cell[a], self.base_cell[b])
            # A road that is much longer than the straight line goes around a sea: the two cities trade by ship.
            if path and len(path) <= 2.2 * hex_distance(self.base_cell[a], self.base_cell[b]) + 6:
                self.lay_road(path)
                self.roads.append((a, b))
        for site in self.sites:
            if site["kind"] in ("port", "pass"):
                start, goal = self.base_cell[site["id"]], self.base_cell[site["owner_city"]]
                path = self.find_path(start, goal)
                if path and len(path) <= 3 * hex_distance(start, goal) + 6: self.lay_road(path)

    # -- the regions, the slots and the flags --

    def make_regions(self):
        grid = self.grid
        region = np.full((grid.height, grid.width), 255, dtype=np.uint8)
        frontier = []
        for site in self.sites:
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
        frontier = [(0, self.base_cell[s["id"]], s["id"]) for s in self.sites if s["kind"] == "port"]
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
        for site in self.sites:
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
        self.make_regions()
        self.place_slots()
        self.place_bandits()

    # -- the output --

    def height_value(self, row, col):
        kind = self.terrain[row, col]
        if kind in WATER: return 0
        value = int(round(12 + max(0.0, float(self.elev_mean[row, col])) / 15.0))
        if kind == CLIFF: value = max(value, 90)
        return min(255, value)

    def write(self, pack):
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
        for site in self.sites:
            cell = self.base_cell[site["id"]]
            owner = site["id"] if site["kind"] == "city" else site["owner_city"]
            hexes = 7 if site["kind"] == "city" else 3 if site["kind"] == "pass" else 1
            bases.append("%d\t%s\t%s\t%d\t%d\t%d\t%d\t%s\t%d" % (site["id"], site["name_zh"], kinds[site["kind"]], cell[0], cell[1], hexes, owner,
                                                                self.sites[owner]["name_zh"], self.slot_count.get(site["id"], 0)))
        write_lines(os.path.join(pack, "map", "bases.tsv"), bases)

        port_owners = {s["owner_city"] for s in self.sites if s["kind"] == "port"}
        cities = ["id\tspecialty\tslots\tport\tname_zh\tprice\tname_big5",
                  "# Written by misc/make_world_map.py from map/source/sites.tsv. slots: the slots that the tool placed on the map.",
                  "# price: 5 for each city, the middle of the range 3 to 7 (the placeholder pack, Q-155).",
                  "# name_big5: the Big5 bytes of name_zh in hex. The scenario of the pack names the cities with it (docs/MAPS.md, W2)."]
        for city in self.cities:
            cities.append("%d\t%s\t%d\t%s\t%s\t5\t%s" % (city["id"], city["specialty"], self.slot_count[city["id"]],
                                                        "yes" if city["id"] in port_owners else "no", city["name_zh"], big5_hex(city["name_zh"], 16)))
        write_lines(os.path.join(pack, "cities.tsv"), cities)

        content_bases = ["id\tkey\tname_zh\tkind\towner_city\tdurability\tstatus\tname_big5",
                         "# Written by misc/make_world_map.py from map/source/sites.tsv. durability: 4000 for a pass and 2000 for a port (assumed).",
                         "# name_big5: the Big5 bytes of name_zh in hex, for the names of the scenario (docs/MAPS.md, W2)."]
        for site in self.sites:
            if site["kind"] == "city": continue
            content_bases.append("%d\t%s\t%s\t%s\t%d\t%d\tassumed\t%s" % (site["id"], site["key"], site["name_zh"], site["kind"], site["owner_city"],
                                                                        4000 if site["kind"] == "pass" else 2000, big5_hex(site["name_zh"], 25)))
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
                "terrain: " + ", ".join("%d=%d" % (k, v) for k, v in enumerate(counts) if v)]

    def preview(self, path, pixels=3.0):
        colours = {GRASS: (142, 178, 92), EARTH: (190, 176, 112), SAND: (226, 206, 148), SWAMP: (104, 140, 110), FOREST: (58, 112, 60),
                   RIVER: (70, 130, 200), SEA: (36, 74, 128), WASTE: (150, 128, 96), ROAD: (214, 196, 150), FORD: (120, 170, 210),
                   CLIFF: (96, 84, 76), CITY: (200, 40, 40), PORT: (240, 160, 40), GATE: (150, 40, 160)}
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
    world.write(pack)
    world.preview(preview)
    for line in world.summary(): print(line)
    print("preview:", preview)


main()
