#!/usr/bin/env python3
"""Reads a PS2 memory card image (PCSX2's .ps2) and exports Dark Cloud's saves for the port.

    ps2card.py list CARD
    ps2card.py export CARD SAVE_DIR [--first N]

`export` copies each `darkcloudN` file of the card's Dark Cloud save directories (BESCES-50295,
the PAL release, BASCUS-97111, the NTSC one, and BISCPS-15004, the Japanese one) into
SAVE_DIR/saves/<board>/save.dat, the port's layout (docs/PC.md, "Saves and host files"), in card
order from board N (default: after the last board SAVE_DIR holds). It never writes over a board
that exists. The image is read only.

The card file system: pages of 512 bytes, each followed by 16 bytes of ECC in a raw image, two
pages to a cluster; a superblock on page 0, a two-level FAT reached through the superblock's
indirect FAT clusters, and directories of 512-byte entries.
"""

import argparse
import os
import struct
import sys

SAVE_SIZE = 0x136A7
SAVE_DIRS = ("BESCES-50295dkcloud", "BASCUS-97111dkcloud", "BISCPS-15004dkcloud")
VERSION_OFFSET = 0x131C0


class Card:
    def __init__(self, path):
        with open(path, "rb") as file:
            self.raw = file.read()
        sb = self.raw[:512]
        if not sb.startswith(b"Sony PS2 Memory Card Format "):
            raise ValueError(f"{path}: not a PS2 memory card image")
        self.page_len, self.pages_per_cluster = struct.unpack_from("<HH", sb, 0x28)
        self.alloc_offset, self.alloc_end, self.root = struct.unpack_from("<III", sb, 0x34)
        self.indirect = struct.unpack_from("<32I", sb, 0x50)
        # A raw image keeps each page's ECC; an image without it is pages alone.
        self.raw_page = self.page_len + 16 if len(self.raw) % (self.page_len + 16) == 0 else self.page_len
        self.cluster_size = self.page_len * self.pages_per_cluster

    def page(self, number):
        start = number * self.raw_page
        return self.raw[start:start + self.page_len]

    def cluster(self, number):
        first = number * self.pages_per_cluster
        return b"".join(self.page(first + i) for i in range(self.pages_per_cluster))

    def fat_entry(self, number):
        per_cluster = self.cluster_size // 4
        table, rest = divmod(number, per_cluster * per_cluster)
        fat_cluster = struct.unpack_from("<I", self.cluster(self.indirect[table]), (rest // per_cluster) * 4)[0]
        return struct.unpack_from("<I", self.cluster(fat_cluster), (rest % per_cluster) * 4)[0]

    def chain(self, first):
        number = first
        for _ in range(self.alloc_end):
            yield number
            entry = self.fat_entry(number)
            if entry == 0xFFFFFFFF or not entry & 0x80000000 or entry & 0x7FFFFFFF == 0x7FFFFFFF:
                return
            number = entry & 0x7FFFFFFF

    def read(self, first, length):
        data = b"".join(self.cluster(self.alloc_offset + c) for c in self.chain(first))
        return data[:length]

    def entries(self, first, count):
        data = self.read(first, count * 512)
        for i in range(count):
            entry = data[i * 512:(i + 1) * 512]
            mode, length = struct.unpack_from("<HxxI", entry, 0)
            cluster = struct.unpack_from("<I", entry, 0x10)[0]
            name = entry[0x40:0x60].split(b"\0")[0].decode("ascii", "replace")
            if mode & 0x8000 and name not in (".", ".."):
                yield mode, length, cluster, name

    def files(self):
        """(directory, name, length, cluster) of every file, a level below the root."""
        root_count = struct.unpack_from("<I", self.read(self.root, 512), 4)[0]
        for mode, length, cluster, name in self.entries(self.root, root_count):
            if mode & 0x20:
                for _, file_length, file_cluster, file_name in self.entries(cluster, length):
                    yield name, file_name, file_length, file_cluster


def save_number(name):
    return int(name[len("darkcloud"):]) if name.startswith("darkcloud") and name[9:].isdigit() else None


def list_card(card):
    for directory, name, length, _ in card.files():
        print(f"{directory}/{name}  {length}")


def export(card, save_dir, first):
    saves = [(directory, name, length, cluster) for directory, name, length, cluster in card.files()
             if directory in SAVE_DIRS and save_number(name) is not None]
    saves.sort(key=lambda save: (save[0], save_number(save[1])))
    if not saves:
        print("the card holds no Dark Cloud save", file=sys.stderr)
        return 1
    boards = os.path.join(save_dir, "saves")
    os.makedirs(boards, exist_ok=True)
    if first is None:
        taken = [int(entry) for entry in os.listdir(boards) if entry.isdigit()]
        first = max(taken, default=0) + 1
    board = first
    for directory, name, length, cluster in saves:
        data = card.read(cluster, length)
        if len(data) != SAVE_SIZE:
            print(f"{directory}/{name}: {len(data)} bytes, not a save; skipped", file=sys.stderr)
            continue
        version = data[VERSION_OFFSET:VERSION_OFFSET + 0x20].split(b"\0")[0].decode("ascii", "replace")
        target = os.path.join(boards, str(board))
        try:
            os.mkdir(target)
        except FileExistsError:
            print(f"board {board} exists; stopping before {directory}/{name}", file=sys.stderr)
            return 1
        with open(os.path.join(target, "save.dat"), "xb") as file:
            file.write(data)
        print(f"{directory}/{name} ({version}) -> saves/{board}/save.dat")
        board += 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    list_parser = commands.add_parser("list", help="list the card's files")
    list_parser.add_argument("card")
    export_parser = commands.add_parser("export", help="copy the Dark Cloud saves into a save directory")
    export_parser.add_argument("card")
    export_parser.add_argument("save_dir")
    export_parser.add_argument("--first", type=int, help="the first board to fill (default: after the last one)")
    args = parser.parse_args()
    card = Card(args.card)
    if args.command == "list":
        list_card(card)
        return 0
    return export(card, args.save_dir, args.first)


if __name__ == "__main__":
    sys.exit(main())
