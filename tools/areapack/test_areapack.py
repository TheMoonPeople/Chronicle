"""Tests for tools/areapack. Run: python -m unittest discover -s tools/areapack
They use small made-up inputs; no disc is needed (the real tables are read from the checkout if it has ps2/)."""
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import areapack  # noqa: E402
import catalog  # noqa: E402
import cdata  # noqa: E402
import cfg  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


class CDataTest(unittest.TestCase):
    def test_enum_values_and_groups(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "a.hpp").write_text("enum Thing { T_A = 3, T_B, T_C = 10, T_D, T_E = T_C + 5 };")
            groups = {}
            values = cdata.read_enums(d, groups)
        self.assertEqual(values["T_B"], 4)
        self.assertEqual(values["T_D"], 11)
        self.assertEqual(values["T_E"], 15)
        self.assertEqual(groups["Thing"]["T_A"], 3)

    def test_initializer_and_names(self):
        body = '{ {"abc", "", 7}, -1, 1.5f, ITEM_X, {{1, 2}, {3}} }'
        got = cdata.parse_initializer(body, {"ITEM_X": 9})
        self.assertEqual(got, [["abc", "", 7], -1, 1.5, 9, [[1, 2], [3]]])

    def test_find_array_skips_comments(self):
        text = cdata.strip_comments("// Foo[2] = {9};\nint Foo[2] = { 1, /* two */ 2 };\n")
        self.assertEqual(cdata.read_array(text, "Foo", {}), [1, 2])

    def test_struct_records(self):
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / "s.hpp").write_text(
                "struct INNER { int a; int b; };\nstruct OUTER { INNER in[2]; char name[16]; int unk_20; };\n")
            structs = cdata.read_structs(d)
        rec = cdata.to_record(structs, "OUTER", [[[1, 2], [3, 4]], "hi", 5], drop_unknown=True)
        self.assertEqual(rec, {"in": [{"a": 1, "b": 2}, {"a": 3, "b": 4}], "name": "hi"})

    def test_pointer_array(self):
        text = "T *Ptrs[3] = { A, B,\n C };"
        self.assertEqual(cdata.read_pointer_array(text, "Ptrs"), ["A", "B", "C"])


class CfgTest(unittest.TestCase):
    def test_commands_and_parts(self):
        text = ("//\n//テスト\n//\n\n\tFOG\t380 ,650\t,0\n\tSET_PATH \"dun/d01/\"\n//no00\n\tDEF_PATS\n"
                "\t PT_BASE \"a.mds\"\t,4\t,0,0,0,\t0\n\t  PT_FIRE\t-3.5,\t2,\t1\n\tDEF_ENDS\n")
        got = cfg.parse(text.encode("cp932"))
        self.assertEqual(got["title"], "テスト")
        self.assertEqual(got["commands"][0], {"cmd": "FOG", "args": [380, 650, 0]})
        self.assertEqual(got["commands"][1]["args"], ["dun/d01/"])
        self.assertEqual(got["parts"][0]["note"], "no00")
        self.assertEqual(got["parts"][0]["entries"][1]["args"], [-3.5, 2, 1])


class CatalogTest(unittest.TestCase):
    def test_dungeon_matcher(self):
        m = catalog.dungeon_matcher("d01")
        self.assertTrue(m("dun/d01/anime/x.img"))
        self.assertTrue(m("dun/d01main_a.cfg"))
        self.assertTrue(m("dun/mpd_pack/d01boss.mpd"))
        self.assertFalse(m("dun/d010/x"))
        self.assertFalse(m("gedit/e01/d01_2.mes"))
        self.assertFalse(m("dun/d02/x.img"))

    def test_related_dungeons(self):
        self.assertEqual(catalog.related_dungeons("神獣の洞窟イベント"), ["d01"])
        self.assertEqual(catalog.related_dungeons("D04踊場"), ["d04"])
        self.assertEqual(catalog.related_dungeons(""), [])

    def test_label(self):
        self.assertEqual(catalog.jp_label("//\n//\n//ゴローの家\n".encode("cp932")), "ゴローの家")


def tim2_32bit(w, h, rgba):
    pix = bytes(rgba) * (w * h)
    pic_hdr = struct.pack("<IIIHH", 0x30 + len(pix), 0, len(pix), 0x30, 0)
    pic_hdr += struct.pack("<BBBBHH", 0, 1, 0, 3, w, h) + bytes(0x30 - 0x18)
    return b"TIM2" + bytes([4, 0]) + struct.pack("<H", 1) + bytes(8) + pic_hdr + pix


class Tim2Test(unittest.TestCase):
    def test_scan_finds_picture_in_container(self):
        import tim2
        blob = b"junk" * 8 + tim2_32bit(4, 2, (10, 20, 30, 64)) + b"tail"
        found = list(tim2.scan(blob))
        self.assertEqual(len(found), 1)
        rgba = found[0][2][0][0]
        self.assertEqual(rgba.shape, (2, 4, 4))
        self.assertEqual(tuple(rgba[0, 0]), (10, 20, 30, 128))  # PS2 alpha 64 -> 128


class PackTest(unittest.TestCase):
    def make_data(self, root):
        for rel, body in {
            "dun/d01/anime/a.img": b"x",
            "dun/d01main_a.cfg": b"\n\n\tFOG 1,2\n",
            "dun/mpd_pack/d01boss.mpd": b"m",
            "dun/monstor/e01a.chr": tim2_32bit(2, 2, (1, 2, 3, 128)),
            "gedit/e01/mapinfo.cfg": "//\n//\n//テスト\n".encode("cp932"),
            "gedit/system/sys.img": b"s",
            "chara/c01d.chr": b"c",
            "sound/bgm.bin": b"b",
            "data.hd2": b"h",
        }.items():
            p = Path(root) / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(body)

    def test_every_file_lands_in_a_pack(self):
        with tempfile.TemporaryDirectory() as d:
            self.make_data(d)
            files = areapack.walk(Path(d))
            tables = areapack.Tables(Path(d))  # no ps2/ here: tables off
            self.assertFalse(tables.ok)
            packs, misc = areapack.build_packs(files, d, tables)
            owners = areapack.assign(files, packs, misc, tables)
            self.assertTrue(all(owners.values()))
            by = {p.id: p for p in packs}
            self.assertIn("dun/d01main_a.cfg", by["d01"].files)
            self.assertIn("dun/mpd_pack/d01boss.mpd", by["d01"].files)
            self.assertIn("gedit/e01/mapinfo.cfg", by["e01"].files)
            self.assertIn("chara/c01d.chr", by["_characters"].files)
            self.assertIn("data.hd2", misc.files)

    def test_extract_writes_pack(self):
        with tempfile.TemporaryDirectory() as d, tempfile.TemporaryDirectory() as o:
            self.make_data(d)
            rc = areapack.main(["extract", d, o, "--decomp", d, "--area", "d01", "e01", "--workers", "1"])
            self.assertEqual(rc, 0)
            doc = json.loads((Path(o) / "d01" / "area.json").read_text(encoding="utf-8"))
            self.assertEqual(sorted(f["path"] for f in doc["files"]),
                             ["dun/d01/anime/a.img", "dun/d01main_a.cfg", "dun/mpd_pack/d01boss.mpd"])
            self.assertTrue((Path(o) / "d01" / "raw" / "dun" / "d01main_a.cfg").is_file())
            self.assertIn("configs.json", doc["tables"])
            self.assertEqual(json.loads((Path(o) / "pack.json").read_text(encoding="utf-8"))["unassigned"], [])

    @unittest.skipUnless((ROOT / "ps2" / "include" / "monstorunit.hpp").is_file(), "needs the decomp sources")
    def test_real_tables(self):
        t = areapack.Tables(ROOT)
        self.assertTrue(t.ok)
        self.assertEqual(len(t.monsters_raw), 167)
        self.assertEqual(t.monster(0)["name"], "Master Jacket")
        self.assertEqual(len(t.floors(0)["front"]), 15)
        self.assertTrue(t.monster_ids(0))
        self.assertTrue(t.treasure_boxes(0)["front"]["lists"])
        self.assertEqual(t.item(145), "ITEM_REGULAR_WATER")


if __name__ == "__main__":
    unittest.main()
