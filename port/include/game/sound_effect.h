#pragma once

/**
 * Sound effects the game plays by number through SndSePlay.
 *
 * Numbers 100-299 index whichever basic table the loaded sound set names (the
 * town table or the dungeon table), 300-399 the sound set's own table, and
 * 400-499 the voice set of the active character (set 6 is fishing). The same
 * number therefore plays different samples in different places; each name
 * follows where the game plays it. The SE_RUSH_ effects are cued only by the
 * attract movie and are named from their recordings.
 */
enum SoundEffect {
    SE_ATTACHMENT_SET = 5,        /**< Attachment put into a weapon's slot. */
    SE_ATTACHMENT_REMOVE = 6,     /**< Attachment taken out of a weapon's slot. */
    SE_EB_INTRO_TICK = 8,         /**< Button-timing event counts in. */
    SE_EB_HIT = 9,                /**< Button-timing event: prompt hit. */
    SE_EB_HIT_COOL = 10,          /**< Button-timing event: prompt hit perfectly. */
    SE_SAVE_START = 12,           /**< Save screen starts reading the memory card. */
    SE_SPARKLE = 15,              /**< Georama part completed, or a character steps in after one falls. */
    SE_LOCK_ON = 17,              /**< Lock-on target taken or changed. */
    SE_EFFECT_CHIME = 19,         /**< Item takes effect, or a weapon levels up. */
    SE_WARNING = 22,              /**< Warning beep while the character is in danger. */
    SE_WEAPON_EQUIP = 23,         /**< Weapon equipped. */
    SE_WEAPON_REPAIR = 24,        /**< Weapon repaired. */
    SE_AMBIENT_TOWN_NOISE = 52,   /**< Town sound source: a long steady rush of noise. */
    SE_AMBIENT_CAVE_DRIPS = 53,   /**< Divine Beast Cave background: repeated drips. */
    SE_AMBIENT_TOWN_PATTER = 54,  /**< Town sound source: a long run of rapid bright hits. */
    SE_AMBIENT_DUNGEON_HISS = 61, /**< Divine Beast Cave and Demon Shaft background hiss. */
    SE_AMBIENT_SHIPWRECK = 65,    /**< Shipwreck background: slow creaks. */
    SE_AMBIENT_DEEP_DUNGEON = 66, /**< Background from the Sun and Moon Temple onwards: falling wind. */
    SE_THUNDER_RUMBLE = 67,       /**< First of six thunder rumbles picked at random. */
    SE_THUNDER_CLAP = 74,         /**< Thunderclap that starts a storm or a lightning flash. */
    SE_FIRE_GEM = 101,            /**< Fire Gem used. */
    SE_ICE_GEM = 102,             /**< Ice Gem used. */
    SE_THUNDER_GEM = 103,         /**< Thunder Gem used. */
    SE_WIND_GEM = 104,            /**< Wind Gem used. */
    SE_HOLY_GEM = 105,            /**< Holy Gem or Holy Water used. */
    SE_STATUS_AILMENT = 107,      /**< Status ailment inflicted. */
    SE_EXPLOSION = 108,           /**< Bomb or gem explodes. */
    SE_POWER_UP = 111,            /**< Stamina raised, or a monster flies into a rage. */
    SE_THROW = 150,               /**< Character throws an item. */
    SE_DRINK = 151,               /**< Character drinks. */
    SE_BOX_OPEN = 153,            /**< Item box opened. */
    SE_PURCHASE = 154,            /**< Purchase or exchange completed. */
    SE_WORLD_MAP_OPEN = 155,      /**< World map opened from the menu. */
    SE_HIT_NO_DAMAGE = 159,       /**< Monster hit without taking damage. */
    SE_MONSTER_HIT = 160,         /**< Monster hit and damaged. */
    SE_PLAYER_HIT = 161,          /**< Character hit hard enough to shake the screen. */
    SE_HIT_BLOCKED = 162,         /**< Attack blocked or glancing. */
    SE_CHEST_OPEN_BIG = 206,      /**< Large treasure chest opens, or refuses its weapon. */
    SE_CHEST_OPEN_SMALL = 207,    /**< Small treasure chest opens, or refuses its item. */
    SE_RANDOM_ITEM_EMPTY = 221,   /**< Floor item turns out to be empty. */
    SE_RANDOM_ITEM = 222,         /**< Floor item turns out to hold something. */
    SE_ITEM_GET = 223,            /**< Attachment picked up. */
    SE_WEAPON_BREAK = 224,        /**< Weapon breaks. */
    SE_GORO_SMASH = 264,          /**< Goro's charged smash lands. */
    SE_GATE_KEY_GET = 296,        /**< Gate key picked up. */
    SE_RUSH_LOW_HUM = 300,        /**< Attract movie: long low hum. */
    SE_RUSH_LOW_SWELL = 302,      /**< Attract movie: long low swelling tone. */
    SE_RUSH_LOW_TONE = 305,       /**< Attract movie: low tone repeated every half second. */
    SE_RUSH_BRIGHT_SWELL = 345,   /**< Attract movie: long bright swell. */
    SE_RUSH_IMPACT = 346,         /**< Attract movie: short falling impact placed in the world. */
    SE_RUSH_LOW_THUMP = 360,      /**< Attract movie: low thump. */
    SE_RUSH_CLACK = 362,          /**< Attract movie: short clack. */
    SE_RUSH_CLACK_2 = 363,        /**< Attract movie: the same clack again. */
    SE_RUSH_KNOCK = 364,          /**< Attract movie: short dull knock. */
    SE_RUSH_SCRAPE = 365,         /**< Attract movie: short scrape. */
    SE_RUSH_RISING_CHIRP = 366,   /**< Attract movie: short rising chirp. */
    SE_RUSH_RISING_SWELL = 368,   /**< Attract movie: rising swell. */
    SE_RUSH_FALLING_CHIRP = 369,  /**< Attract movie: short falling chirp. */
    SE_RUSH_BRIGHT_HIT = 370,     /**< Attract movie: short bright hit. */
    SE_RUSH_LOW_BUMP = 374,       /**< Attract movie: low bump placed in the world. */
    SE_RUSH_WHOOSH = 395,         /**< Attract movie: long bright whoosh. */
    SE_RUSH_LONG_SWELL = 396,     /**< Attract movie: long swelling rumble. */
    SE_CHARA_ACTION = 400,        /**< Character sound 1, such as Ruby's attack. */
    SE_CHARA_ACTION_2 = 401,      /**< Character sound 2. */
    SE_CHARA_ACTION_3 = 402,      /**< Character sound 3. */
    SE_CHARA_ACTION_4 = 403,      /**< Character sound 4. */
    SE_CHARA_ACTION_5 = 404,      /**< Character sound 5. */
    SE_FISHING_CAST = 400,        /**< Fishing: line cast. */
    SE_FISHING_SPLASH = 401,      /**< Fishing: lure lands in the water. */
    SE_FISHING_SINK = 402,        /**< Fishing: lure sinks below the surface. */
    SE_FISHING_BITE = 403,        /**< Fishing: a fish takes the lure. */
    SE_FISHING_LANDED = 404,      /**< Fishing: the fish is landed. */
    SE_CHARA_SHOUT = 420,         /**< Character attack shout 1. */
    SE_CHARA_SHOUT_2 = 421,       /**< Character attack shout 2. */
    SE_CHARA_SHOUT_3 = 422,       /**< Character attack shout 3. */
    SE_CHARA_SHOUT_4 = 423,       /**< Character attack shout 4. */
    SE_CHARA_HURT_HEAVY = 430,    /**< Character cries out at a heavy hit. */
    SE_CHARA_HURT = 431,          /**< Character cries out at a hit. */
    SE_CHARA_KNOCKED_OUT = 433,   /**< Character falls. */
    SE_CHARA_RECOVER = 440,       /**< Character recovers health. */
    SE_WATER_SPLASH = 547,        /**< Character splashes into water. */
    SE_RUSH_TAP = 606,            /**< Attract movie: short tap. */
    SE_RUSH_TAP_2 = 607,          /**< Attract movie: the same tap again. */
    SE_RUSH_HISS = 610,           /**< Attract movie: short bright hiss. */
    SE_RUSH_RUSTLE = 612,         /**< Attract movie: rustle. */
    SE_RUSH_CRACK = 616,          /**< Attract movie: sharp crack. */
    SE_RUSH_CRACK_2 = 617,        /**< Attract movie: second sharp crack. */
    SE_RUSH_CRACK_3 = 618,        /**< Attract movie: third sharp crack. */
    SE_RUSH_LONG_NOISE = 1727,    /**< Attract movie: long noise. */
    SE_RUSH_LONG_RUMBLE = 1729,   /**< Attract movie: long rumble. */
    SE_DRAN_FIELD_START = 1737,   /**< A Dran field model starts moving; the attract movie cues it too. */
    SE_RUSH_DULL_KNOCK = 1746,    /**< Attract movie: dull knock. */
    SE_RUSH_DULL_KNOCK_2 = 1747,  /**< Attract movie: the same knock again. */
    SE_RUSH_DEEP_TONE = 1749,     /**< Attract movie: long deep tone. */
    SE_RUSH_SWELLING_TONE = 1755, /**< Attract movie: long swelling tone. */
};
