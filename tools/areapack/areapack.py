#!/usr/bin/env python3
"""Area data packs: everything the disc holds about one dungeon or town, in one folder.

    python tools/areapack/areapack.py list    <iso-or-data-folder>
    python tools/areapack/areapack.py extract <iso-or-data-folder> <out> [--area d01 e01 ...] [--no-textures]
                                              [--indexed] [--dcdata path/to/dcdata] [--decomp <repo root>]

`<iso-or-data-folder>` is the disc image (unpacked with tools/dcdata) or a folder `dcdata extract` already wrote.
`<out>/<area>/` then holds, for each dungeon (d01..d07), town or event map (e01.., s01..) and shared group (_characters ...):

    raw/        every disc file the area uses, at its disc path
    textures/   every TIM2 picture inside those files, as PNG (rgba; --indexed adds the paletted originals)
    tables/     the game's numbers for the area as JSON: monsters, floor layouts, treasure boxes, clown offers ...
    area.json   what is in the folder: files with sizes and hashes, pictures with their file and offset, tables
    replace/    empty; a place for edited files, laid out like raw/
    README.md   what this pack is and how it is organised

`<out>/pack.json` indexes the packs and lists any disc file that no pack took (there should be none).
Nothing in an output folder is committed to the repository: it is made from the player's own disc.
Needs Python 3.9+ with numpy and Pillow.
"""
import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import catalog  # noqa: E402
import cdata  # noqa: E402
import cfg  # noqa: E402

VERSION = 1
SKIP_TEXTURE_EXT = {".stb", ".snd", ".sne", ".mes", ".cfg", ".ini", ".txt", ".scn", ".bin", ".hd2", ".edt", ".mpd"}


# --------------------------------------------------------------------------------------------- the disc

def find_dcdata(hint, root):
    candidates = [hint] if hint else []
    for base in (root / "build", root / "build" / "pc", root / "port" / "build" / "pc"):
        for name in ("dcdata", "dcdata.exe"):
            candidates += [base / name, base / "tools" / "dcdata" / name]
    exe = shutil.which("dcdata")
    if exe:
        candidates.append(exe)
    for c in candidates:
        if c and Path(c).is_file():
            return str(c)
    sys.exit("areapack: cannot find dcdata. Build it (see docs/PC.md) or pass --dcdata <path>, "
             "or unpack the disc yourself and pass the folder.")


def resolve_data(source, out, dcdata_hint, root):
    """Returns the folder holding dun/, gedit/ ... for an ISO or an already extracted folder."""
    source = Path(source)
    if source.is_dir():
        return source
    if not source.is_file():
        sys.exit(f"areapack: {source} is neither a file nor a folder")
    target = out / "_disc"
    exe = find_dcdata(dcdata_hint, root)
    print(f"unpacking {source.name} with {exe} -> {target}")
    subprocess.run([exe, "extract", str(source), str(target)], check=True)
    return target


def walk(data):
    files = {}
    for dirpath, _, names in os.walk(data):
        for n in names:
            p = Path(dirpath) / n
            rel = p.relative_to(data).as_posix()
            files[rel] = p
    return files


# --------------------------------------------------------------------------------------------- the decomp tables

class Tables:
    """The game tables the decomp keeps as C source, read from a checkout."""

    def __init__(self, root):
        inc = root / "ps2" / "include"
        src = root / "ps2" / "src"
        self.ok = (inc / "monstorunit.hpp").is_file() and (src / "monstorunit.cpp").is_file()
        self.warnings = []
        if not self.ok:
            self.warnings.append(f"{root} has no ps2/ sources; the tables/ folders will be empty. Pass --decomp.")
            return
        self.groups = {}
        self.enums = cdata.read_enums(inc, self.groups)
        self.structs = cdata.read_structs(inc)
        clean = lambda p: cdata.strip_comments(p.read_text(encoding="utf-8", errors="replace"))  # noqa: E731
        raw_monster = (src / "monstorunit.cpp").read_text(encoding="utf-8", errors="replace")
        self.monster_names = {int(m.group(1)): m.group(2).strip()
                              for m in re.finditer(r"//\s*(\d+):\s*(.+)", raw_monster)}
        self.monsters_raw = cdata.read_array(clean(src / "monstorunit.cpp"), "MonstorTable", self.enums) or []
        floors = clean(inc / "monstorunit_floor_data.inc")
        self.layouts = {}
        for i in range(len(catalog.DUNGEONS)):
            for kind, name in (("front", f"BtEnemyLayout{i:02d}"), ("back", f"BtUraEnemyLayout{i:02d}")):
                self.layouts[(i, kind)] = cdata.read_array(floors, name, self.enums) or []
        parts = clean(src / "dungeonparts.cpp")
        self.parts_text = parts
        self.item_names = self._names("Item")
        self.family_names = self._names("MonsterFamily")
        self.kind_names = self._names("MonsterKind")
        self.put_ptrs = cdata.read_pointer_array(parts, "ItemPutListPtr") or []
        self.piero_ptrs = cdata.read_pointer_array(parts, "PieroItemListPtr") or []
        self.rate_ptrs = cdata.read_pointer_array(parts, "ItemSetRateTbl") or []
        self.free_ptrs = cdata.read_pointer_array(parts, "ItemFreeAreaAll") or []
        self.floor_divide = cdata.read_array(parts, "floorNum", self.enums) or []

    def _names(self, enum):
        out = {}
        for name, val in self.groups.get(enum, {}).items():
            if not re.search(r"_(START|END|COUNT|MAX|NUM)$", name):
                out.setdefault(val, name)
        return out

    def item(self, no):
        return self.item_names.get(no, None) if isinstance(no, int) and no >= 0 else None

    def monster(self, no):
        row = cdata.to_record(self.structs, "MONSTOR_MODEL", self.monsters_raw[no])
        row = dict(row)
        row["family_name"] = self.family_names.get(row.get("attachment_kind"))
        row["kind_name"] = self.kind_names.get(row.get("kind"))
        for key in ("steal_item", "rare_item"):
            row[key + "_name"] = self.item(row.get(key))
        out = {"id": no, "name": self.monster_names.get(no), **row}
        return out

    def all_monsters(self):
        return [self.monster(i) for i in range(len(self.monsters_raw))]

    def floors(self, dungeon):
        """Floor layouts of a dungeon: front and back side, slots with monster ids and their chance."""
        out = {}
        for kind in ("front", "back"):
            rows = []
            for n, floor in enumerate(self.layouts.get((dungeon, kind), []), 1):
                rec = cdata.to_record(self.structs, "BT_ENEMY_FLOOR", floor, drop_unknown=False)
                slots = []
                for s in rec.get("monster", []):
                    if s.get("monster_no", -1) < 0:
                        continue
                    no = s["monster_no"]
                    slots.append({"monster_no": no, "name": self.monster_names.get(no), "chance": s.get("unk_08")})
                first = rec.get("monster", [{}])[0] if rec.get("monster") else {}
                rows.append({"floor": n, "monster_count": first.get("unk_00"), "monsters": slots,
                             "unk_6C": rec.get("unk_6C")})
            out[kind] = rows
        return out

    def monster_ids(self, dungeon):
        ids = set()
        for kind in ("front", "back"):
            for floor in self.layouts.get((dungeon, kind), []):
                for slot in (floor[0] if floor and isinstance(floor[0], list) else []):
                    if isinstance(slot, list) and len(slot) >= 2 and isinstance(slot[1], int) and slot[1] >= 0:
                        ids.add(slot[1])
        return sorted(i for i in ids if i < len(self.monsters_raw))

    def _array_by_symbol(self, symbol):
        return cdata.read_array(self.parts_text, symbol, self.enums)

    def treasure_boxes(self, dungeon):
        out = {}
        for side, idx in (("front", dungeon), ("back", dungeon + 7)):
            if idx >= len(self.put_ptrs):
                continue
            sets = self._array_by_symbol(self.put_ptrs[idx]) or []
            rows = []
            for s in sets:
                rec = cdata.to_record(self.structs, "ITEM_PUT_SET", s, drop_unknown=False)
                if rec.get("floor", -1) < 0:
                    continue
                items = [i for i in rec.get("item", []) if i >= 0]
                rows.append({"floor_key": rec["floor"], "item_count": rec.get("item_count"),
                             "items": [{"id": i, "name": self.item(i)} for i in items]})
            out[side] = {"table": self.put_ptrs[idx], "lists": rows}
        out["note"] = ("floor_key is ITEM_PUT_SET::floor. Table 0-6 is the dungeon, 7-13 its back floors "
                       "(see the comments in ps2/include/dungeonparts.hpp).")
        out["floor_divide"] = self.floor_divide[dungeon] if dungeon < len(self.floor_divide) else None
        return out

    def piero(self, dungeon):
        out = {}
        for side, idx in (("front", dungeon), ("back", dungeon + 7)):
            if idx >= len(self.piero_ptrs):
                continue
            sets = self._array_by_symbol(self.piero_ptrs[idx]) or []
            rows = []
            for s in sets:
                rec = cdata.to_record(self.structs, "PIERO_ITEM_SET", s, drop_unknown=False)
                lists = []
                for count, items in zip(rec.get("count", []), rec.get("item", [])):
                    lists.append([{"id": i, "name": self.item(i)} for i in items if i >= 0])
                rows.append({"count": rec.get("count"), "offers": lists})
            out[side] = {"table": self.piero_ptrs[idx], "sets": rows}
        return out

    def item_rates(self, dungeon):
        if dungeon >= len(self.rate_ptrs):
            return None
        rates = self._array_by_symbol(self.rate_ptrs[dungeon]) or []
        return {"table": self.rate_ptrs[dungeon],
                "meaning": "chance out of 100 that the item is turned down when drawn in this dungeon",
                "items": [{"id": i, "name": self.item(i), "turned_down_percent": r}
                          for i, r in enumerate(rates) if self.item(i)]}

    def free_areas(self, dungeon):
        if dungeon >= len(self.free_ptrs):
            return None
        rows = self._array_by_symbol(self.free_ptrs[dungeon]) or []
        out = []
        for r in rows:
            rec = cdata.to_record(self.structs, "ITEM_FREE_AREA", r, drop_unknown=False)
            if rec.get("parts_no", -1) < 0:
                continue
            rects = [x for x in rec.get("rect", [])][:rec.get("rect_num", 0)]
            out.append({"map_part": rec["parts_no"], "direction": rec.get("direction"), "boxes": rects})
        return {"table": self.free_ptrs[dungeon], "areas": out}

    def global_enums(self):
        names = {"items": "Item", "monster_families": "MonsterFamily", "monster_kinds": "MonsterKind",
                 "dungeon_event_kinds": "DungeonEventKind", "treasure_box_kinds": "TreasureBoxKind",
                 "trap_circle_kinds": "TrapCircleKind", "characters": "Character", "item_kinds": "ItemKind",
                 "dungeons": "Dungeon"}
        return {key: dict(sorted(self.groups.get(enum, {}).items(), key=lambda kv: (kv[1], kv[0])))
                for key, enum in names.items()}


# --------------------------------------------------------------------------------------------- packs

class Pack:
    def __init__(self, pid, kind, names, label=""):
        self.id = pid
        self.kind = kind
        self.names = names
        self.label = label
        self.files = []
        self.related = []
        self.tables = {}
        self.match = None
        self.note = ""


def build_packs(files, data, tables):
    packs = []
    for index, tag, en, jp in catalog.DUNGEONS:
        p = Pack(tag, "dungeon", {"en": en, "jp": jp})
        p.index = index
        p.match = catalog.dungeon_matcher(tag)
        packs.append(p)
    towns = sorted({rel.split("/")[1] for rel in files if rel.startswith("gedit/") and rel.count("/") >= 2})
    for t in towns:
        if t == "system":
            continue
        label = ""
        info = files.get(f"gedit/{t}/mapinfo.cfg")
        if info:
            label = catalog.jp_label(info.read_bytes()[:400])
        names = {"jp": label}
        if t in catalog.TOWN_NAMES:
            names["en"] = catalog.TOWN_NAMES[t]
        p = Pack(t, "town" if t.startswith("e") else "map", names, label)
        p.match = catalog.town_matcher(t)
        p.related = catalog.related_dungeons(label)
        packs.append(p)
    for pid, desc, prefixes in [("_town_shared", "Files every town and event map shares", ["gedit/system/", "gedit/interior/"])] \
            + catalog.GLOBAL_PACKS:
        p = Pack(pid, "shared", {"en": desc})
        p.match = (lambda prefixes: lambda path: any(path.startswith(x) for x in prefixes))(tuple(prefixes))
        packs.append(p)
    misc = Pack(catalog.MISC_PACK[0], "shared", {"en": catalog.MISC_PACK[1]})
    return packs, misc


def assign(files, packs, misc, tables):
    """Puts each disc file into every area that uses it; files no area wants go to the first shared pack, else misc."""
    by_id = {p.id: p for p in packs}
    owners = {rel: [] for rel in files}
    areas = [p for p in packs if p.kind in ("dungeon", "town", "map")]
    shared = [p for p in packs if p.kind == "shared"]
    monster_files = {}
    for p in packs:
        if p.kind != "dungeon" or not tables.ok:
            continue
        stems = set()
        for no in tables.monster_ids(p.index):
            row = tables.monsters_raw[no]
            stems.update(s for s in row[0] if s)
            stems.add(row[1])
        monster_files[p.id] = stems
        p.tables["monster_ids"] = tables.monster_ids(p.index)
    for rel in files:
        hit = [p for p in areas if p.match(rel)]
        if rel.startswith("dun/monstor/"):
            stem = rel.rsplit("/", 1)[1].split(".")[0]
            hit += [by_id[t] for t, stems in monster_files.items() if stem in stems and by_id[t] not in hit]
        if not hit:
            hit = [next((p for p in shared if p.match(rel)), misc)]
        owners[rel] = hit
    for rel, ps in owners.items():
        for p in ps:
            p.files.append(rel)
    if misc.files:
        packs.append(misc)
    return owners


# --------------------------------------------------------------------------------------------- textures

def texture_job(args):
    rel, path, dests, indexed = args
    import numpy as np  # noqa: F401
    from PIL import Image
    import tim2
    try:
        data = Path(path).read_bytes()
    except OSError:
        return rel, []
    found = []
    seen = {}
    folder = rel.replace("/", "__")
    for off, name, pics in tim2.scan(data):
        name = name if name and name.isprintable() else Path(rel).stem
        name = "".join(c if c.isalnum() or c in "-_." else "_" for c in name)
        for i, (rgba, idx, pal, info) in enumerate(pics):
            base = name + (f"_{i}" if len(pics) > 1 else "")
            n = seen.get(base, 0)
            seen[base] = n + 1
            if n:
                base = f"{base}~{n}"
            key = tim2.digest(rgba)
            entry = {"source": rel, "offset": off, "name": base, "png": f"textures/{folder}/{base}.png",
                     "md5": key, **info}
            if indexed and idx is not None:
                entry["png_indexed"] = f"textures_indexed/{folder}/{base}.png"
            found.append(entry)
            for dest in dests:
                d = Path(dest) / "textures" / folder
                d.mkdir(parents=True, exist_ok=True)
                Image.fromarray(rgba, "RGBA").save(d / (base + ".png"))
                if indexed and idx is not None:
                    d2 = Path(dest) / "textures_indexed" / folder
                    d2.mkdir(parents=True, exist_ok=True)
                    im = Image.fromarray(idx, "P")
                    im.putpalette(pal.tobytes(), "RGBA")
                    im.save(d2 / (base + ".png"))
    return rel, found


# --------------------------------------------------------------------------------------------- output

def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")


def pack_tables(pack, tables, files):
    out = {}
    configs = {rel: cfg.parse(files[rel].read_bytes()) for rel in sorted(pack.files) if rel.endswith(".cfg")}
    if configs:
        out["configs.json"] = {"note": "Every .cfg of the area, read command by command (lighting, fog, map parts). "
                                       "cp932 comments are decoded.", "files": configs}
    if not tables.ok:
        return out
    if pack.kind == "dungeon":
        ids = tables.monster_ids(pack.index)
        out["monsters.json"] = [tables.monster(i) for i in ids]
        out["floors.json"] = {"dungeon": pack.id, **tables.floors(pack.index),
                              "note": "monsters[].chance is the percentage weight of the slot on that floor. "
                                      "monster_count is the first field of the floor's first slot (unnamed in the "
                                      "decomp; it rises from 3 to 8 down the floors, so it reads as how many "
                                      "monsters the floor holds). unk_6C is unnamed too."}
        out["treasure_boxes.json"] = tables.treasure_boxes(pack.index)
        out["piero_offers.json"] = tables.piero(pack.index)
        out["item_reject_rates.json"] = tables.item_rates(pack.index)
        out["item_free_areas.json"] = tables.free_areas(pack.index)
    return out


README = """# {title}

Area `{id}` ({kind}) made by `tools/areapack` from the disc you own. Nothing here is committed to the repository.

| folder / file | what it holds |
|---|---|
| `raw/` | The {nfiles} disc files this area uses, at their disc paths ({shared} also used by other areas). |
| `textures/` | The {ntex} TIM2 pictures found inside those files, as RGBA PNG. `area.json` lists each picture's source file and byte offset. |
| `tables/` | {tables} |
| `replace/` | Empty. Put edited files here with the same relative path as in `raw/`. |
| `area.json` | Every file (size, SHA-1, other areas that use it), every picture, every table. |

To upscale or redraw this area: work from `textures/`, keep each picture's size ratio, and record the result
against the `source`/`offset`/`name` in `area.json` so it can be put back where the game reads it.
{extra}
"""


def write_pack(pack, out, files, owners, textures, tables, with_raw=True):
    base = out / pack.id
    base.mkdir(parents=True, exist_ok=True)
    entries = []
    for rel in sorted(pack.files):
        src = files[rel]
        others = [p.id for p in owners[rel] if p is not pack]
        if with_raw:
            dst = base / "raw" / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            if not dst.exists() or dst.stat().st_size != src.stat().st_size:
                shutil.copy2(src, dst)
        entry = {"path": rel, "size": src.stat().st_size, "sha1": sha1(src)}
        if others:
            entry["also_in"] = others
        entries.append(entry)
    (base / "replace").mkdir(exist_ok=True)
    (base / "replace" / ".gitkeep").write_text("")
    table_files = pack_tables(pack, tables, files)
    for name, obj in table_files.items():
        write_json(base / "tables" / name, obj)
    pics = [t for rel in pack.files for t in textures.get(rel, [])]
    title = (pack.names.get("en") or pack.names.get("jp") or pack.id)
    doc = {"id": pack.id, "kind": pack.kind, "names": pack.names, "related_dungeons": pack.related,
           "files": entries, "textures": pics, "tables": sorted(table_files), "tool_version": VERSION}
    if pack.kind in ("town", "map") and pack.related:
        doc["related_basis"] = "keywords in the map's Japanese label"
    write_json(base / "area.json", doc)
    extra = ""
    if pack.kind == "dungeon":
        extra = ("\nFloors are built at random from the map parts in `raw/dun/mpd_pack/`; `tables/floors.json` says which "
                 "monsters each floor can hold, `tables/treasure_boxes.json` what its boxes can contain.\n")
    (base / "README.md").write_text(README.format(
        title=title, id=pack.id, kind=pack.kind, nfiles=len(entries), shared=sum(1 for e in entries if "also_in" in e),
        ntex=len(pics), tables=", ".join(f"`{n}`" for n in sorted(table_files)) or "none for this kind of area",
        extra=extra), encoding="utf-8")
    return doc


def cmd_list(args):
    root = Path(args.decomp).resolve()
    data = resolve_data(args.source, Path(args.tmp or "."), args.dcdata, root)
    files = walk(data)
    tables = Tables(root)
    packs, misc = build_packs(files, data, tables)
    owners = assign(files, packs, misc, tables)
    for w in tables.warnings:
        print("warning:", w)
    for p in packs:
        size = sum(files[r].stat().st_size for r in p.files)
        name = p.names.get("en") or p.names.get("jp") or ""
        print(f"{p.id:14} {p.kind:8} {len(p.files):5} files {size / 1048576:9.1f} MB  {name}")
    print(f"{len(files)} disc files, {sum(1 for o in owners.values() if o)} assigned")
    return 0


def cmd_extract(args):
    t0 = time.time()
    root = Path(args.decomp).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    data = resolve_data(args.source, out, args.dcdata, root)
    files = walk(data)
    if not files:
        sys.exit(f"areapack: no files under {data}")
    tables = Tables(root)
    for w in tables.warnings:
        print("warning:", w)
    packs, misc = build_packs(files, data, tables)
    owners = assign(files, packs, misc, tables)
    wanted = [p for p in packs if not args.area or p.id in args.area]
    if args.area:
        missing = set(args.area) - {p.id for p in packs}
        if missing:
            sys.exit("areapack: unknown area(s): " + ", ".join(sorted(missing)))
    print(f"{len(files)} disc files, {len(wanted)} packs to write")

    textures = {}
    if not args.no_textures:
        jobs = []
        selected = {p.id for p in wanted}
        for rel, path in files.items():
            if Path(rel).suffix.lower() in SKIP_TEXTURE_EXT:
                continue
            dests = [str(out / p.id) for p in owners[rel] if p.id in selected]
            if dests:
                jobs.append((rel, str(path), dests, args.indexed))
        workers = args.workers or max(1, (os.cpu_count() or 2) - 1)
        done = 0
        with cf.ProcessPoolExecutor(workers) as pool:
            for rel, found in pool.map(texture_job, jobs, chunksize=8):
                if found:
                    textures[rel] = found
                done += 1
                if done % 500 == 0:
                    print(f"  textures: {done}/{len(jobs)} files scanned")
        print(f"textures: {sum(len(v) for v in textures.values())} pictures in {len(textures)} files")

    index = []
    for p in wanted:
        doc = write_pack(p, out, files, owners, textures, tables)
        index.append({"id": p.id, "kind": p.kind, "names": p.names, "files": len(doc["files"]),
                      "textures": len(doc["textures"]), "tables": doc["tables"],
                      "related_dungeons": p.related})
        print(f"  {p.id:14} {len(doc['files']):5} files {len(doc['textures']):5} pictures")
    if tables.ok:
        write_json(out / "_tables" / "monsters.json", tables.all_monsters())
        write_json(out / "_tables" / "enums.json", tables.global_enums())
    unassigned = sorted(rel for rel, ps in owners.items() if not ps)
    write_json(out / "pack.json", {"tool_version": VERSION, "disc_files": len(files), "packs": index,
                                   "unassigned": unassigned, "warnings": tables.warnings})
    print(f"done in {time.time() - t0:.0f}s -> {out}")
    if unassigned:
        print(f"warning: {len(unassigned)} disc files belong to no pack (see pack.json)")
    return 0


def main(argv=None):
    here = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("list", "extract"):
        sp = sub.add_parser(name)
        sp.add_argument("source", help="the ISO, or a folder `dcdata extract` wrote")
        if name == "extract":
            sp.add_argument("out")
            sp.add_argument("--area", nargs="*", help="only these packs, e.g. d01 e01 s42 _characters")
            sp.add_argument("--no-textures", action="store_true")
            sp.add_argument("--indexed", action="store_true", help="also write the paletted originals")
            sp.add_argument("--workers", type=int)
        else:
            sp.add_argument("--tmp", help="where to unpack an ISO to (default: current folder)")
        sp.add_argument("--dcdata", help="path to the dcdata tool (for an ISO)")
        sp.add_argument("--decomp", default=str(here), help="the repository checkout holding ps2/ (default: this one)")
    args = ap.parse_args(argv)
    return {"list": cmd_list, "extract": cmd_extract}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
