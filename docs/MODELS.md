# Character models on the disc

`dcdata models` lists every character model on the disc, and how each one's skeleton compares with
a playable character's. It is the first step to putting another character's model (Seda's) on a
playable character's motions (Toan's): that works only where the two skeletons share their bones.

It reads the disc image and writes a text report of names and numbers, no game data:

```sh
# dcdata on its own, with any C++23 compiler (g++ 13, clang 18 or later):
c++ -std=c++23 -O2 tools/dcdata/main.cpp -o dcdata
./dcdata models "rom/pal/Dark Cloud (PAL).iso" > models_report.txt
```

`port/build/pc/dcdata`, built with the port, does the same. A third argument names another
reference pack than Toan's `dun/mainchara/c01d.chr`.

The report has:

- **Every model, most like the reference first:** how many of the reference's bone names each model
  has, its bone count, how many motion keys (`KEY` lines) its configuration has, and where it is: the
  file on the disc, then each pack inside it down to the scene (`.mds`).
- **Reference:** the reference's configuration and skeleton.
- **Details:** the configuration, the skeleton and the reference's bones it lacks, for the 40 models
  most like the reference and every model in a pack named `c<number>…` (the story's characters).

A character is a pack (`.chr`, or one nested in an event or map pack) holding a configuration (`.cfg`)
and its scenes. A scene is a 16-byte header whose third word counts the objects, then one 112-byte
record per object: two words, a 32-byte name, the offset of its model data, its parent's index and its
matrix (`ps2/include/mds.hpp`). The objects are the skeleton's bones.
