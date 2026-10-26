#pragma once

/**
 * Languages the game can run in, as LanguageCode holds them; each loads the
 * matching dun/img/<prefix>/ images.
 */
enum Language {
    LANG_JAPANESE = 0,   /**< Japanese, dun/img/jp/. */
    LANG_ENGLISH_US = 1, /**< American English, dun/img/us/. */
    LANG_ENGLISH_UK = 2, /**< British English, dun/img/us_e/. */
    LANG_FRENCH = 3,     /**< French, dun/img/fr/. */
    LANG_GERMAN = 4,     /**< German, dun/img/gr/. */
    LANG_ITALIAN = 5,    /**< Italian, dun/img/it/. */
    LANG_SPANISH = 6,    /**< Spanish, dun/img/sp/. */
    LANG_COUNT = 7,      /**< Number of languages. */
};
