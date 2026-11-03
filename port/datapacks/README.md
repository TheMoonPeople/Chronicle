# Datapacks

Packs the game ships. The build copies each into `assets/datapacks/` beside the executable, and every
package carries them there; each is off until a player names it in `datapacks/datapacks.json`, beside
the game's data folder (docs/ITEM_FRAMEWORK.md, "Datapacks"). Packs are read at start-up.

## Enhanced fishing

`enhanced_fishing/`: Dark Cloud 2's brisk bite and tension fight, and Ruty's three rods with her
quest (docs/FISHING_FRAMEWORK.md, "Enhanced fishing").

To turn it on with the Flatpak, whose data folder is
`~/.var/app/org.themoonpeople.Chronicle/data/chronicle/data`, write:

```sh
packs=~/.var/app/org.themoonpeople.Chronicle/data/chronicle/datapacks
mkdir -p "$packs"
echo '{ "schema_version": 1, "load_order": ["enhanced_fishing"] }' > "$packs/datapacks.json"
```

Elsewhere, `datapacks/` sits beside whichever data folder the game uses (`--data`, `DC_DATA`, or the
folder beside the executable). Taking `enhanced_fishing` out of `load_order` goes back to the base
game's fishing; a save holding Ruty's rods keeps them as unknown items until it is back.

A folder of the same name in `datapacks/` is used instead of the game's, for trying changes to it.
