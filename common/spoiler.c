
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwr.h"
#include "base32.h"
#include "chests.h"
#include "map.h"
#include "spoiler.h"

#define SPOILER_SIZE 65536
#define SEARCH_COUNT 3
#define SPIKE_COUNT 8
#define SECTION_WIDTH 78
#define XY_SIZE 12

/**
 * Maps, indexed by dw_map_index. Dungeon floors also carry the dungeon and
 * floor separately, so the enemy zone list can say "Grave B3-B4".
 */
static const struct {
    const char *name;
    const char *dungeon;
    const char *floor;
} maps[] = {
    { "(none)",               NULL,          NULL },
    { "Overworld",            NULL,          NULL },
    { "Charlock Top",         "Charlock",    "Top" },
    { "Hauksness",            "Hauksness",   NULL },
    { "Tantegel Castle",      NULL,          NULL },
    { "Tantegel Throne Room", NULL,          NULL },
    { "Charlock Bottom",      "Charlock",    "Bottom" },
    { "Kol",                  NULL,          NULL },
    { "Brecconary",           NULL,          NULL },
    { "Garinham",             NULL,          NULL },
    { "Cantlin",              NULL,          NULL },
    { "Rimuldar",             NULL,          NULL },
    { "Stones Cave",          NULL,          NULL },
    { "Staff Cave",           NULL,          NULL },
    { "Jerk Cave",            NULL,          NULL },
    { "Charlock B1",          "Charlock",    "B1" },
    { "Charlock B2",          "Charlock",    "B2" },
    { "Charlock B3",          "Charlock",    "B3" },
    { "Charlock B4",          "Charlock",    "B4" },
    { "Charlock B5",          "Charlock",    "B5" },
    { "Charlock B6",          "Charlock",    "B6" },
    { "Swamp Cave",           "Swamp Cave",  NULL },
    { "Mountain B1",          "Mountain",    "B1" },
    { "Mountain B2",          "Mountain",    "B2" },
    { "Grave B1",             "Grave",       "B1" },
    { "Grave B2",             "Grave",       "B2" },
    { "Grave B3",             "Grave",       "B3" },
    { "Grave B4",             "Grave",       "B4" },
    { "Tablet Cave B1",       "Tablet Cave", "B1" },
    { "Tablet Cave B2",       "Tablet Cave", "B2" },
};

/**
 * The order places are listed in: the overworld, then Tantegel and the towns,
 * then the caves, with Charlock last.
 */
static const uint8_t map_order[] = {
    OVERWORLD, TANTEGEL_THRONE_ROOM, TANTEGEL, TANTEGEL_BASEMENT, BRECCONARY,
    GARINHAM, KOL, RIMULDAR, CANTLIN, HAUKSNESS, NORTHERN_SHRINE,
    SOUTHERN_SHRINE, SWAMP_CAVE, MOUNTAIN_CAVE, MOUNTAIN_CAVE_2,
    GARINS_GRAVE_1, GARINS_GRAVE_2, GARINS_GRAVE_3, GARINS_GRAVE_4,
    ERDRICKS_CAVE, ERDRICKS_CAVE_2, CHARLOCK, CHARLOCK_CAVE_1,
    CHARLOCK_CAVE_2, CHARLOCK_CAVE_3, CHARLOCK_CAVE_4, CHARLOCK_CAVE_5,
    CHARLOCK_CAVE_6, CHARLOCK_THRONE_ROOM
};

/**
 * Names for the chest content byte, up to Erdrick's Sword. Everything above
 * that is gold as far as the randomizer's TAKE code is concerned; see
 * read_gold().
 */
static const char *item_names[] = {
    "(nothing)", "Erdrick's Armor", "Herb", "Magic Key", "Torch",
    "Fairy Water", "Wings", "Dragon's Scale", "Fairy Flute", "Fighter's Ring",
    "Erdrick's Token", "Gwaelin's Love", "Cursed Belt", "Silver Harp",
    "Death Necklace", "Stones of Sunlight", "Staff of Rain", "Erdrick's Sword",
};

/** Names for the shop inventory byte, indexed by dw_shop_item. */
static const char *shop_item_names[] = {
    "Bamboo Pole", "Club", "Copper Sword", "Hand Axe", "Broad Sword",
    "Flame Sword", "Erdrick's Sword", "Clothes", "Leather Armor", "Chain Mail",
    "Half Plate", "Full Plate", "Magic Armor", "Erdrick's Armor",
    "Small Shield", "Large Shield", "Silver Shield", "Herb", "Magic Key",
    "Torch", "Fairy Water", "Wings", "Dragon's Scale"
};

/** Names for the enemies, indexed by dw_enemies. */
static const char *enemy_names[] = {
    "Slime", "Red Slime", "Drakee", "Ghost", "Magician", "Magidrakee",
    "Scorpion", "Druin", "Poltergeist", "Droll", "Drakeema", "Skeleton",
    "Warlock", "Metal Scorpion", "Wolf", "Wraith", "Metal Slime", "Specter",
    "Wolflord", "Druinlord", "Drollmagi", "Wyvern", "Rogue Scorpion",
    "Wraith Knight", "Golem", "Goldman", "Knight", "Magiwyvern",
    "Demon Knight", "Werewolf", "Green Dragon", "Starwyvern", "Wizard",
    "Axe Knight", "Blue Dragon", "Stoneman", "Armored Knight", "Red Dragon",
    "Dragonlord 1", "Dragonlord 2"
};

/**
 * The items worth calling out separately, in the order they are listed.
 *
 * The Staff of Rain is left out on purpose: it is a static chest, so it is
 * always in the same place and listing it says nothing about the seed. So is
 * the Rainbow Drop, which is never in a chest at all - the old man hands it
 * over. The Staff still appears under Chests & search spots.
 */
static const uint8_t key_items[] = {
    SWORD, ARMOR, HARP, STONES, TOKEN, NECKLACE, FLUTE, RING
};

/**
 * Locations, as warp indexes. An entrance that is not on the overworld is
 * listed under the place it is in.
 */
static const struct { uint8_t warp; const char *name; } locations[] = {
    { WARP_TANTEGEL,          "Tantegel Castle" },
    { WARP_TANTEGEL_BASEMENT, "Stones Cave" },
    { WARP_BRECCONARY,        "Brecconary" },
    { WARP_GARINHAM,          "Garinham" },
    { WARP_KOL,               "Kol" },
    { WARP_RIMULDAR,          "Rimuldar" },
    { WARP_CANTLIN,           "Cantlin" },
    { WARP_HAUKSNESS,         "Hauksness" },
    { WARP_CHARLOCK,          "Charlock" },
    { WARP_SWAMP_NORTH,       "Swamp North" },
    { WARP_SWAMP_SOUTH,       "Swamp South" },
    { WARP_MOUNTAIN_CAVE,     "Mountain" },
    { WARP_GARINS_GRAVE,      "Grave" },
    { WARP_ERDRICKS_CAVE,     "Tablet Cave" },
    { WARP_STAFF_SHRINE,      "Staff Cave" },
    { WARP_JERK_CAVE,         "Jerk Cave" },
};
#define LOCATION_COUNT (sizeof(locations) / sizeof(locations[0]))

/*
 * Shops. Every shopkeeper is an NPC whose dialog byte picks what it does:
 * 0x00-0x06 are the weapon shops and 0x07-0x0b the item shops (the byte is
 * also the shop's index in the inventory table), 0x0c-0x0e sell keys at the
 * price in KEY_PRICES + (byte - 0x0c), and 0x0f-0x10 sell fairy water.
 * Worked out from the talk handler at $d7ed and the inn prices that follow
 * the key prices, which put the towns in the order Kol, Brecconary,
 * Garinham, Cantlin, Rimuldar. Cantlin's three weapon shops were told apart
 * from the town map: the one at (24,12) and the key shop are only reachable
 * through the door at (26,9).
 */
#define NPC_MOBILE_PTRS 0x1734
#define NPC_STATIC_PTRS 0x174c
#define KEY_PRICES      0x1989
#define SHOP_KEYS       0x0c
#define PRICE_FAIRY     0x14
#define SHOP_MAGIC_KEY  0x12

typedef enum { SELLS_WEAPONS, SELLS_ITEMS, SELLS_KEYS, SELLS_FAIRY } shop_kind;

static const struct {
    uint8_t map;
    uint8_t dialog;
    shop_kind kind;
    const char *label;
} shops[] = {
    { TANTEGEL,   0x0e, SELLS_KEYS,    "" },
    { BRECCONARY, 0x01, SELLS_WEAPONS, "" },
    { BRECCONARY, 0x08, SELLS_ITEMS,   "" },
    { BRECCONARY, 0x0f, SELLS_FAIRY,   "" },
    { GARINHAM,   0x02, SELLS_WEAPONS, "" },
    { GARINHAM,   0x09, SELLS_ITEMS,   "" },
    { KOL,        0x00, SELLS_WEAPONS, "" },
    { KOL,        0x07, SELLS_ITEMS,   "" },
    { RIMULDAR,   0x06, SELLS_WEAPONS, "" },
    { RIMULDAR,   0x0d, SELLS_KEYS,    "" },
    { CANTLIN,    0x03, SELLS_WEAPONS, "(North)" },
    { CANTLIN,    0x05, SELLS_WEAPONS, "(Locked)" },
    { CANTLIN,    0x04, SELLS_WEAPONS, "(South)" },
    { CANTLIN,    0x0a, SELLS_ITEMS,   "" },
    { CANTLIN,    0x0b, SELLS_ITEMS,   "" },
    { CANTLIN,    0x0c, SELLS_KEYS,    "(Locked)" },
    { CANTLIN,    0x10, SELLS_FAIRY,   "" },
};

static const char *shop_kinds[] = {
    "Equipment", "Items", "Keys", "Fairy water"
};

/*
 * The options, in the order and wording the web ui uses (webui/index.js), with
 * the flag byte and bit each one lives in. Most are tri-state: the low bit is
 * "on" and the one above it is "maybe", which update_flags() rolls before any
 * of this runs - so the log reads the flag string the user typed to tell a
 * maybe from an on, and rom->flags for what the maybe rolled.
 *
 * A new flag needs one line, plus a name of our own if it has no web ui
 * option yet. The spoiler flag itself is left out: it is on in every log.
 */
typedef enum {
    TAB_GAMEPLAY, TAB_FEATURES, TAB_MONSTERS, TAB_SHORTCUTS, TAB_CHALLENGE,
    TAB_COSMETIC, TAB_COUNT
} flag_tab;

static const char *tab_names[] = {
    "Gameplay", "Features", "Monsters", "Shortcuts", "Challenge", "Cosmetic"
};

static const struct {
    flag_tab tab;
    uint8_t byte;
    uint8_t bit;
    BOOL tri;
    const char *name;
} options[] = {
    { TAB_GAMEPLAY,   0, 6, TRUE,  "Shuffle Chests & Searches" },
    { TAB_GAMEPLAY,   0, 4, TRUE,  "Random Chest Locations" },
    { TAB_GAMEPLAY,   0, 2, TRUE,  "Random Growth" },
    { TAB_GAMEPLAY,   0, 0, TRUE,  "Random Map" },
    { TAB_GAMEPLAY,   1, 6, TRUE,  "Random Spell Learning" },
    { TAB_GAMEPLAY,   1, 4, TRUE,  "Random Weapon Shops" },
    { TAB_GAMEPLAY,   1, 2, TRUE,  "Random Weapon Prices" },
    { TAB_GAMEPLAY,   1, 0, TRUE,  "Random XP Requirements" },
    { TAB_GAMEPLAY,   2, 6, TRUE,  "Heal/Hurt Before \"More\"" },
    { TAB_GAMEPLAY,   2, 2, TRUE,  "Stair Shuffle" },
    { TAB_FEATURES,   3, 6, TRUE,  "Enable Menu Wrapping" },
    { TAB_FEATURES,   3, 4, TRUE,  "Enable Death Necklace" },
    { TAB_FEATURES,   3, 2, TRUE,  "Enable Torches In Battle" },
    { TAB_FEATURES,   4, 6, TRUE,  "Repel in Dungeons" },
    { TAB_FEATURES,   4, 4, TRUE,  "Permanent Repel" },
    { TAB_FEATURES,   4, 2, TRUE,  "Permanent Torch" },
    { TAB_FEATURES,   4, 0, TRUE,  "Fourth Run Success" },
    { TAB_MONSTERS,   5, 6, TRUE,  "Random Monster Abilities" },
    { TAB_MONSTERS,   5, 4, TRUE,  "Random Monster Zones" },
    { TAB_MONSTERS,   5, 2, TRUE,  "Random Monster Stats" },
    { TAB_MONSTERS,   5, 0, TRUE,  "Random Monster XP & Gold" },
    { TAB_MONSTERS,   6, 6, TRUE,  "Make Random Stats Consistent" },
    { TAB_MONSTERS,   6, 4, TRUE,  "Scared Metal Slimes" },
    { TAB_MONSTERS,   6, 2, TRUE,  "Scaled Metal Slime XP" },
    { TAB_SHORTCUTS,  7, 6, TRUE,  "Fast Text" },
    { TAB_SHORTCUTS,  7, 4, TRUE,  "Speed Hacks" },
    { TAB_SHORTCUTS,  7, 2, TRUE,  "Open Charlock" },
    { TAB_SHORTCUTS,  7, 0, TRUE,  "Short Charlock" },
    { TAB_SHORTCUTS,  8, 6, TRUE,  "Don't Require Magic Keys" },
    { TAB_SHORTCUTS, 13, 2, TRUE,  "Summer Sale" },
    { TAB_SHORTCUTS,  8, 2, TRUE,  "Cursed Princess" },
    { TAB_SHORTCUTS,  8, 0, TRUE,  "Three's Company" },
    { TAB_CHALLENGE,  9, 6, TRUE,  "No Hurtmore" },
    { TAB_CHALLENGE,  9, 4, TRUE,  "No Numbers" },
    { TAB_CHALLENGE,  9, 2, TRUE,  "Invisible Hero" },
    { TAB_CHALLENGE,  9, 0, TRUE,  "Invisible NPCs" },
    { TAB_CHALLENGE, 13, 0, TRUE,  "Treasure Guards" },
    { TAB_CHALLENGE, 10, 0, TRUE,  "Big Swamp" },
    { TAB_CHALLENGE, 10, 2, TRUE,  "Randomly Rotate/Mirror Dungeons" },
    { TAB_CHALLENGE, 10, 4, TRUE,  "No Armor in Charlock" },
    { TAB_CHALLENGE, 10, 6, TRUE,  "Easy Charlock" },
    { TAB_COSMETIC,  11, 6, TRUE,  "Modern Spell Names" },
    { TAB_COSMETIC,  11, 4, TRUE,  "Noir Mode" },
    { TAB_COSMETIC,  14, 7, FALSE, "Shuffle Music" },
    { TAB_COSMETIC,  14, 6, FALSE, "Disable Music" },
    { TAB_COSMETIC,  14, 5, FALSE, "Disable Spell Flashing" },
    { TAB_COSMETIC,  14, 4, FALSE, "Show Death Counter" },
    { TAB_COSMETIC,  14, 3, FALSE, "Allow Custom Spell Names" },
    { TAB_COSMETIC,  14, 2, FALSE, "Skip Original Credits" },
};

/** The drop-downs, whose two bits pick from a list rather than on/off. */
static const struct {
    flag_tab tab;
    uint8_t byte;
    uint8_t bit;
    const char *name;
    const char *choices[4];
} dropdowns[] = {
    { TAB_SHORTCUTS, 13, 4, "Leveling Speed",
            { "Normal", "Fast", "Very Fast", "Very Fast" } },
    { TAB_SHORTCUTS, 13, 6, "Random Map Size",
            { "Normal", "Small", "Very Small", "Random" } },
};

/*
 * Chest gold. The randomizer's TAKE code sends every content byte from 0x11
 * up to one block at $e337, which loads a random mask and a base; the game
 * then adds (random & mask) to the base. The base differs between branches
 * (500 on master, 560 on dev-3.0.4), so it is read from the rom.
 */
#define GOLD_CODE 0xe337

/*
 * Enemy zones. Dungeon floors take their zone from a table indexed by map,
 * starting at Charlock Cave B1; three maps are special-cased in code at
 * $ced8, which loads their zone as an immediate. Maps from the Tablet Cave on
 * have no encounters. Everything is read back out of the rom.
 */
#define DUNGEON_ZONES   0xf542
#define ZONE_CHARLOCK   0xcedd
#define ZONE_HAUKSNESS  0xcee5
#define ZONE_THRONE     0xceed

/*
 * Hero builds. The game sums the first four letters of the name: the low two
 * bits pick which two stats are penalized, and a penalized stat is
 * floor(stat * 9 / 10) plus a bonus. Vanilla takes the bonus from the name
 * too; the randomizer patches it to a constant at $f131, which is why there
 * are exactly four builds. MP of 0 is never penalized.
 */
#define BUILD_BONUS     0xf131
#define SPELL_NAMES     0x7e56
#define SPELL_COUNT     10

enum { HP, MP, STR, AGI };

/** Each build's two full stats, as a mask of 1 << HP etc. */
static const struct { const char *label; int full; } builds[] = {
    { "Build 1 (Str/HP)",  1 << STR | 1 << HP },
    { "Build 2 (Str/Agi)", 1 << STR | 1 << AGI },
    { "Build 3 (HP/MP)",   1 << HP  | 1 << MP },
    { "Build 4 (Agi/MP)",  1 << AGI | 1 << MP },
};

/* every gold chest gives the same range, so the legend states it once */
static char gold_range[64];

static const char *map_name(uint8_t map)
{
    if (map >= sizeof(maps) / sizeof(maps[0]))
        return "(unknown map)";
    return maps[map].name;
}

/**
 * Where a chest is, which is not always the map's name. Tantegel's treasury
 * is the 3 x 3 room behind the locked door at (4,13); the castle's other two
 * chest spots are out in the castle proper. Charlock's chests sit in its
 * treasury rather than the throne room itself.
 */
static const char *chest_place(uint8_t map, uint8_t x, uint8_t y)
{
    if (map == TANTEGEL && x >= 1 && x <= 3 && y >= 13 && y <= 15)
        return "Tantegel Treasury";
    if (map == CHARLOCK_THRONE_ROOM)
        return "Charlock Treasury";
    return map_name(map);
}

static const char *item_name(uint8_t item)
{
    if (item >= sizeof(item_names) / sizeof(char *))
        return "Gold";
    return item_names[item];
}

static const char *shop_item_name(uint8_t item)
{
    if (item >= sizeof(shop_item_names) / sizeof(char *))
        return "(unknown item)";
    return shop_item_names[item];
}

static const char *enemy_name(uint8_t enemy)
{
    if (enemy >= sizeof(enemy_names) / sizeof(char *))
        return "(unknown enemy)";
    return enemy_names[enemy];
}

static inline uint16_t read16(dw_rom *rom, size_t address)
{
    return rom->content[address] | rom->content[address + 1] << 8;
}

/** "(x,y)", with no padding. */
static const char *xy(char *buf, int x, int y)
{
    snprintf(buf, XY_SIZE, "(%d,%d)", x, y);
    return buf;
}

/**
 * Appends to the log, tracking the write position and never running past the
 * end of the buffer.
 */
static void add(char **pos, char *end, const char *fmt, ...)
{
    va_list args;
    int written;

    if (*pos >= end)
        return;
    va_start(args, fmt);
    written = vsnprintf(*pos, end - *pos, fmt, args);
    va_end(args);
    if (written > 0)
        *pos += (written < end - *pos) ? written : end - *pos;
}

/*
 * Tables. Every table in the log has the same shape: a heading row, a rule of
 * '=' under it, the rows with their columns separated by spaces, and a rule
 * of '-' to close it. A heading can also sit over several columns, centred,
 * on the line above ("Resists" over three, a build over its four stats).
 * Rows are cells, blank lines, or a label (a place, a town) that heads the
 * rows under it; labels count towards the first column's width.
 */
#define TABLE_COLS  20
#define TABLE_ROWS  160
#define CELL_SIZE   80
#define TABLE_GAP   4

typedef enum { ROW_CELLS, ROW_BLANK, ROW_LABEL } row_kind;
typedef enum { ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER } cell_align;

typedef struct {
    int cols;
    const char *head[TABLE_COLS];
    cell_align align[TABLE_COLS];
    int gap[TABLE_COLS];            /* spaces before a column; 0 = TABLE_GAP */
    const char *over[TABLE_COLS];   /* a heading over this and the next ones */
    int over_span[TABLE_COLS];
    int rows;
    row_kind kind[TABLE_ROWS];
    char cell[TABLE_ROWS][TABLE_COLS][CELL_SIZE];
    int width[TABLE_COLS];
} table;

static void table_print_halves(char **pos, char *end, table *t);

static table *table_new(int cols)
{
    table *t = calloc(1, sizeof(table));

    if (t)
        t->cols = cols;
    return t;
}

/** Starts a new row and returns its index, or -1 if the table is full. */
static int table_row(table *t, row_kind kind)
{
    if (!t || t->rows >= TABLE_ROWS)
        return -1;
    t->kind[t->rows] = kind;
    return t->rows++;
}

/** Sets a cell of the last row. */
static void table_set(table *t, int col, const char *fmt, ...)
{
    va_list args;

    if (!t || !t->rows || col >= t->cols)
        return;
    va_start(args, fmt);
    vsnprintf(t->cell[t->rows - 1][col], CELL_SIZE, fmt, args);
    va_end(args);
}

/** A label row: a place or a group, heading the rows under it. */
static void table_label(table *t, const char *fmt, ...)
{
    va_list args;

    if (table_row(t, ROW_LABEL) < 0)
        return;
    va_start(args, fmt);
    vsnprintf(t->cell[t->rows - 1][0], CELL_SIZE, fmt, args);
    va_end(args);
}

static int table_gap(table *t, int col)
{
    return col ? (t->gap[col] ? t->gap[col] : TABLE_GAP) : 0;
}

/** Where a column starts, once the widths are known. */
static int table_start(table *t, int col)
{
    int i, pos = 0;

    for (i=0; i < col; i++)
        pos += t->width[i] + table_gap(t, i + 1);
    return pos;
}

static void add_aligned(char **pos, char *end, const char *text, int width,
        cell_align align)
{
    int len = (int)strlen(text), left;

    switch (align) {
        case ALIGN_RIGHT:
            add(pos, end, "%*s", width, text);
            break;
        case ALIGN_CENTER:
            left = MAX(width - len, 0) / 2;
            add(pos, end, "%*s%s%*s", left, "", text,
                    MAX(width - len - left, 0), "");
            break;
        default:
            add(pos, end, "%-*s", width, text);
    }
}

static void add_rule(char **pos, char *end, char c, int width)
{
    while (width-- > 0)
        add(pos, end, "%c", c);
    add(pos, end, "\n");
}

/** Prints a table and frees it. */
static void table_print(char **pos, char *end, table *t)
{
    int r, c, total, span, start, len;
    BOOL any_over = FALSE;

    if (!t)
        return;
    for (c=0; c < t->cols; c++) {
        /* a width set beforehand is a minimum */
        if (t->head[c])
            t->width[c] = MAX(t->width[c], (int)strlen(t->head[c]));
        for (r=0; r < t->rows; r++) {
            if (t->kind[r] == ROW_CELLS || (t->kind[r] == ROW_LABEL && !c))
                t->width[c] = MAX(t->width[c], (int)strlen(t->cell[r][c]));
        }
    }
    for (c=0; c < t->cols; c++)
        any_over |= t->over[c] != NULL;
    total = table_start(t, t->cols - 1) + t->width[t->cols - 1];

    /* a heading over several columns is centred over them; one wider than
     * its columns spills evenly into the gaps either side, which the caller
     * makes wide enough */
    if (any_over) {
        len = 0;
        for (c=0; c < t->cols; c++) {
            if (!t->over[c])
                continue;
            span = table_start(t, c + t->over_span[c] - 1)
                    + t->width[c + t->over_span[c] - 1] - table_start(t, c);
            start = table_start(t, c)
                    + (span - (int)strlen(t->over[c])) / 2;
            add(pos, end, "%*s%s", MAX(start - len, 1), "", t->over[c]);
            len = MAX(start, len + 1) + (int)strlen(t->over[c]);
        }
        add(pos, end, "\n");
    }
    for (c=0; c < t->cols; c++) {
        add(pos, end, "%*s", table_gap(t, c), "");
        add_aligned(pos, end, t->head[c] ? t->head[c] : "", t->width[c],
                t->align[c]);
    }
    add(pos, end, "\n");
    add_rule(pos, end, '=', total);
    for (r=0; r < t->rows; r++) {
        if (t->kind[r] == ROW_LABEL) {
            add(pos, end, "%s", t->cell[r][0]);
        } else if (t->kind[r] == ROW_CELLS) {
            for (c=0; c < t->cols; c++) {
                add(pos, end, "%*s", table_gap(t, c), "");
                add_aligned(pos, end, t->cell[r][c], t->width[c],
                        t->align[c]);
            }
        }
        add(pos, end, "\n");
    }
    add_rule(pos, end, '-', total);
    free(t);
}

/** The sections' names, in order: the contents list and the headings. */
static const char *section_titles[] = {
    "How to read this log", "Flags", "Key items", "Chests & search spots", "Locations",
    "Shops", "Hero stats", "Enemies", "Encounters"
};
#define SECTION_COUNT (sizeof(section_titles) / sizeof(section_titles[0]))

/** A numbered section heading, padded out so the sections stand apart. */
static void section(char **pos, char *end, int number)
{
    const char *title = section_titles[number - 1];
    int width;
    size_t i;

    add(pos, end, "\n\n== %d. ", number);
    for (i=0; title[i]; i++)
        add(pos, end, "%c", toupper((unsigned char)title[i]));
    add(pos, end, " ");
    width = SECTION_WIDTH - 7 - (int)strlen(title) - (number > 9);
    while (width-- > 0)
        add(pos, end, "=");
    add(pos, end, "\n\n");
}

/**
 * Describes the range the rom's gold chests actually give, or says it could
 * not be read if the TAKE code is not the one this was written against.
 */
static void read_gold(dw_rom *rom)
{
    const uint8_t *c = &rom->content[GOLD_CODE];
    int base;

    /* lda #mask / sta $3e / lda #lo / sta $00 / lda #hi / sta $01 */
    if (c[0] == 0xa9 && c[2] == 0x85 && c[3] == 0x3e && c[4] == 0xa9
            && c[6] == 0x85 && c[7] == 0x00 && c[8] == 0xa9 && c[10] == 0x85
            && c[11] == 0x01) {
        base = c[5] | c[9] << 8;
        snprintf(gold_range, sizeof(gold_range),
                "Every gold chest holds %d-%d, at random.", base, base + c[1]);
    } else {
        snprintf(gold_range, sizeof(gold_range),
                "The amount in gold chests could not be read.");
    }
}

/**
 * Reads the spell names back out of the rom, so the log matches the game
 * whether it uses the classic names, the modern ones, or a sprite's own.
 */
static void read_spell_names(dw_rom *rom, char names[SPELL_COUNT][16])
{
    size_t i, len;
    uint8_t *p = &rom->content[SPELL_NAMES];

    for (i=0; i < SPELL_COUNT; i++) {
        for (len=0; p[len] != 0xff && len < 15; len++)
            names[i][len] = p[len];
        dw2ascii((uint8_t *)names[i], len);
        names[i][len] = '\0';
        while (*p != 0xff)
            p++;
        p++;
    }
}

/**
 * Whether a town still has an NPC with the given dialog byte. No Keys
 * removes the key shopkeepers by zeroing their entries.
 */
static BOOL has_shopkeeper(dw_rom *rom, uint8_t map, uint8_t dialog)
{
    size_t table, p;
    const size_t bases[] = { NPC_STATIC_PTRS, NPC_MOBILE_PTRS };

    if (map < TANTEGEL || map > RIMULDAR)
        return FALSE;
    for (table=0; table < 2; table++) {
        p = read16(rom, bases[table] + (map - TANTEGEL) * 2) - 0x8000;
        for (; rom->content[p] != 0xff; p += 3) {
            if (rom->content[p + 2] != dialog)
                continue;
            if (!rom->content[p] && !rom->content[p + 1] && !dialog)
                continue;
            return TRUE;
        }
    }
    return FALSE;
}

/** The encounter table entry on a tile, or -1. */
static int guard_at(dw_rom *rom, uint8_t map, uint8_t x, uint8_t y)
{
    int i;
    dwr_spike_table *spike = rom->spike_table;

    for (i=0; i < SPIKE_COUNT; i++) {
        if (spike->map[i] && spike->map[i] == map && spike->x[i] == x
                && spike->y[i] == y)
            return i;
    }
    return -1;
}

/** The enemy guarding a tile, "(Werewolf)", or "" if there is none. */
static const char *guard_name(dw_rom *rom, uint8_t map, uint8_t x, uint8_t y)
{
    static char name[32];
    int guard = guard_at(rom, map, x, y);

    if (guard < 0)
        return "";
    snprintf(name, sizeof(name), "(%s)",
            enemy_name(rom->spike_table->monster[guard]));
    return name;
}

static int zone_at(dw_rom *rom, int cell)
{
    if (cell & 1)
        return rom->zone_layout[cell / 2] & 0x0f;
    return rom->zone_layout[cell / 2] >> 4;
}

/** The zone a map's encounters come from, or -1 if it has none. */
static int map_zone(dw_rom *rom, uint8_t map)
{
    switch (map) {
        case CHARLOCK:             return rom->content[ZONE_CHARLOCK];
        case HAUKSNESS:            return rom->content[ZONE_HAUKSNESS];
        case CHARLOCK_THRONE_ROOM: return rom->content[ZONE_THRONE];
        default:
            if (map >= CHARLOCK_CAVE_1 && map < ERDRICKS_CAVE)
                return rom->content[DUNGEON_ZONES + map - CHARLOCK_CAVE_1];
            return -1;
    }
}

static void add_header(char **pos, char *end, dw_rom *rom, uint64_t crc)
{
    size_t i;

    add(pos, end, "Dragon Warrior Randomizer %s - spoiler log\n\n", VERSION);
    add(pos, end, "Seed:     %"PRIu64"\n", rom->seed);
    add(pos, end, "Flags:    %s\n", rom->flags_encoded);
    add(pos, end, "Checksum: %016"PRIX64"\n", crc);

    add(pos, end, "\n\nCONTENTS\n\n");
    for (i=0; i < SECTION_COUNT; i++)
        add(pos, end, "  %2d. %s\n", (int)i + 1, section_titles[i]);
    add(pos, end, "\nTo jump to a section, search for its number, e.g. "
            "\"== 6.\"\n");
}

/**
 * Whether the seed has any Treasure Guards: an encounter tile past the game's
 * own three. Key items, chests and search spots get a "Guarded by" column,
 * and the legend an entry for it, only then - Treasure Guards left to the
 * seed can come up with none.
 */
static BOOL show_guards(dw_rom *rom)
{
    int i;

    for (i=3; i < SPIKE_COUNT; i++) {
        if (rom->spike_table->map[i] != NO_MAP)
            return TRUE;
    }
    return FALSE;
}

#define LEGEND_TERM 16

/**
 * One legend entry in two columns: the term, then its text, whose lines are
 * separated by newlines and line up under each other. A blank line follows.
 */
static void legend_row(char **pos, char *end, const char *term,
        const char *text)
{
    const char *line = text, *eol;

    add(pos, end, "%-*s", LEGEND_TERM, term);
    while ((eol = strchr(line, '\n'))) {
        add(pos, end, "%.*s\n%*s", (int)(eol - line), line, LEGEND_TERM, "");
        line = eol + 1;
    }
    add(pos, end, "%s\n\n", line);
}

/**
 * The terms several sections share. Anything only one section uses is
 * explained at the top of that section instead.
 */
static void add_legend(char **pos, char *end, dw_rom *rom)
{
    section(pos, end, 1);
    legend_row(pos, end, "(x,y)",
        "A tile on the map named beside it, or in the heading\n"
        "above it. (0,0) is the top-left corner; x counts right\n"
        "and y counts down.");
    legend_row(pos, end, "Chests", "Opened with TAKE or SEARCH.");
    legend_row(pos, end, "Search spots",
        "Found with SEARCH while standing on the tile.");
    if (show_guards(rom))
        legend_row(pos, end, "(Enemy)",
            "A Treasure Guard. Stepping on that tile starts the\n"
            "fight, until it is beaten.");
    legend_row(pos, end, "Gold", gold_range);
    (*pos)--;   /* the last entry's blank line; the next heading has its own */
}

/**
 * One option's box, "[x]" or "[ ]", and its name. A tristate option the user
 * left to the seed gets an asterisk after its box, which shows how it
 * rolled: the flag string says it was left to the seed, and rom->flags, which
 * update_flags() has already rolled, says how it came out.
 */
static void add_option(char **pos, char *end, dw_rom *rom,
        const uint8_t *typed, size_t i, int width)
{
    BOOL maybe = options[i].tri && typed[options[i].byte] & 2 << options[i].bit;
    BOOL on = rom->flags[options[i].byte] & 1 << options[i].bit;

    /* update_flags() leaves byte 13 alone: Treasure Guards and Summer Sale
     * are never rolled as a whole. Left to the seed, each guard and each
     * price gets its own coin flip, so the box says whether any came up. */
    if (maybe && options[i].byte == 13) {
        if (options[i].bit == 0)
            on = show_guards(rom);
        else
            on = TRUE;  /* about half of 17 prices; none is a 1 in 131072 */
    }

    /* the asterisk follows the box; the other rows keep a space there so
     * every name starts in the same column */
    add(pos, end, "[%c]%c %-*s", on ? 'x' : ' ', maybe ? '*' : ' ', width,
            options[i].name);
}

/**
 * A setting with more than two choices, as "name = value", under the first
 * column: the name lines up with the checkbox labels and the equals signs
 * with each other. A setting left to the seed shows what it rolled, with
 * its asterisk in the same column as the ones after the boxes.
 */
static void add_dropdown(char **pos, char *end, dw_rom *rom,
        const uint8_t *typed, size_t j)
{
    int chosen = typed[dropdowns[j].byte] >> dropdowns[j].bit & 3;
    int rolled = rom->flags[dropdowns[j].byte] >> dropdowns[j].bit & 3;
    int longest = 0;
    size_t k;

    for (k=0; k < sizeof(dropdowns) / sizeof(dropdowns[0]); k++)
        longest = MAX(longest, (int)strlen(dropdowns[k].name));
    add(pos, end, "     %c %-*s = %s\n", chosen != rolled ? '*' : ' ', longest,
            dropdowns[j].name, dropdowns[j].choices[rolled]);
}

/** A tab's options, in the order they are listed; returns how many. */
static size_t tab_options(int tab, size_t *in_tab)
{
    size_t i, count = 0;

    for (i=0; i < sizeof(options) / sizeof(options[0]); i++) {
        if (options[i].tab == (flag_tab)tab)
            in_tab[count++] = i;
    }
    return count;
}

static void add_flags(char **pos, char *end, dw_rom *rom)
{
    size_t i, j, count, half;
    int tab, left = 0;
    size_t in_tab[sizeof(options) / sizeof(options[0])];
    uint8_t typed[16] = { 0 };

    base32_decode(rom->flags_encoded, typed);

    /* the first column is as wide as its longest name, in any tab, and the
     * second starts the usual table gap after it */
    for (tab=0; tab < TAB_COUNT; tab++) {
        count = tab_options(tab, in_tab);
        for (i=0; i < (count + 1) / 2; i++)
            left = MAX(left, (int)strlen(options[in_tab[i]].name));
    }

    section(pos, end, 2);
    add(pos, end, "The options this seed was made with. * marks a tristate "
            "flag or setting left\nto the seed; its box or value shows how it "
            "rolled.\n");
    /* these two roll per guard and per price, not once; see add_option() */
    if (typed[13] & 0x0a)
        add(pos, end, "Treasure Guards and Summer Sale left to the seed roll "
                "per guard and per\nprice instead: [x] means at least one came "
                "up.\n");
    for (tab=0; tab < TAB_COUNT; tab++) {
        count = tab_options(tab, in_tab);
        add(pos, end, "\n%s\n", tab_names[tab]);

        /* two columns, filled down the first and then the second */
        half = (count + 1) / 2;
        for (i=0; i < half; i++) {
            add(pos, end, "  ");
            add_option(pos, end, rom, typed, in_tab[i], left + TABLE_GAP);
            if (i + half < count)
                add_option(pos, end, rom, typed, in_tab[i + half], 0);
            add(pos, end, "\n");
        }
        /* the drop-downs together, under the checkboxes */
        for (j=0; j < sizeof(dropdowns) / sizeof(dropdowns[0]); j++) {
            if (dropdowns[j].tab == (flag_tab)tab)
                add_dropdown(pos, end, rom, typed, j);
        }
    }
}

static void add_key_items(char **pos, char *end, dw_rom *rom)
{
    size_t i, j, k;
    BOOL found;
    char buf[XY_SIZE];
    table *t;
    dwr_search_table *search = rom->search_table;
    struct { uint8_t map, x, y; const char *place; }
            spots[CHEST_COUNT + SEARCH_COUNT];
    const uint8_t *items[CHEST_COUNT + SEARCH_COUNT];

    section(pos, end, 3);

    /* every place a key item can be */
    for (i=0; i < CHEST_COUNT; i++) {
        spots[i].map = rom->chests[i].map;
        spots[i].x = rom->chests[i].x;
        spots[i].y = rom->chests[i].y;
        items[i] = &rom->chests[i].item;
    }
    for (i=0; i < SEARCH_COUNT; i++) {
        spots[CHEST_COUNT + i].map = search->map[i];
        spots[CHEST_COUNT + i].x = search->x[i];
        spots[CHEST_COUNT + i].y = search->y[i];
        items[CHEST_COUNT + i] = &search->item[i];
    }
    /* a chest names the room it is in, a search spot the map */
    for (i=0; i < CHEST_COUNT + SEARCH_COUNT; i++) {
        spots[i].place = i < CHEST_COUNT
                ? chest_place(spots[i].map, spots[i].x, spots[i].y)
                : map_name(spots[i].map);
    }

    t = table_new(show_guards(rom) ? 3 : 2);
    if (!t)
        return;
    t->head[0] = "Item";
    t->head[1] = "Location";
    t->head[2] = "Guarded by";
    for (k=0; k < sizeof(key_items); k++) {
        found = FALSE;
        for (j=0; j < CHEST_COUNT + SEARCH_COUNT; j++) {
            if (*items[j] != key_items[k])
                continue;
            table_row(t, ROW_CELLS);
            table_set(t, 0, "%s", item_name(key_items[k]));
            table_set(t, 1, "%s %s", spots[j].place,
                    xy(buf, spots[j].x, spots[j].y));
            table_set(t, 2, "%s", guard_name(rom, spots[j].map, spots[j].x,
                    spots[j].y));
            found = TRUE;
        }
        if (!found) {
            table_row(t, ROW_CELLS);
            table_set(t, 0, "%s", item_name(key_items[k]));
            table_set(t, 1, "not in this seed");
        }
    }
    table_print(pos, end, t);
}

/** A treasure table's columns: where, what, and the guard if there can be one. */
static table *treasure_table(dw_rom *rom)
{
    table *t = table_new(show_guards(rom) ? 3 : 2);

    if (t) {
        t->head[0] = "Location";
        t->head[1] = "Contents";
        t->head[2] = "Guarded by";
    }
    return t;
}

static void add_treasures(char **pos, char *end, dw_rom *rom)
{
    size_t m, i, j;
    uint8_t map;
    int order[CHEST_COUNT], count, tmp;
    const char *place, *last;
    char buf[XY_SIZE];
    table *t;
    dw_chest *chest;
    dwr_search_table *search = rom->search_table;

    section(pos, end, 4);

    /* chests, grouped under the room they are in */
    add(pos, end, "Chests\n\n");
    t = treasure_table(rom);
    last = NULL;
    for (m=0; m < sizeof(map_order); m++) {
        map = map_order[m];

        /* this map's chests, top to bottom, then left to right */
        /* the Staff of Rain's chest is the same in every seed */
        count = 0;
        for (i=0; i < CHEST_COUNT; i++) {
            if (rom->chests[i].map == map && rom->chests[i].item != STAFF)
                order[count++] = (int)i;
        }
        for (i=1; i < (size_t)count; i++) {
            for (j=i; j > 0; j--) {
                dw_chest *a = &rom->chests[order[j - 1]];
                dw_chest *b = &rom->chests[order[j]];
                if (a->y < b->y || (a->y == b->y && a->x <= b->x))
                    break;
                tmp = order[j]; order[j] = order[j - 1]; order[j - 1] = tmp;
            }
        }
        for (i=0; i < (size_t)count; i++) {
            chest = &rom->chests[order[i]];
            place = chest_place(map, chest->x, chest->y);
            if (place != last) {
                if (last)
                    table_row(t, ROW_BLANK);
                table_label(t, "[%s]", place);
                last = place;
            }
            table_row(t, ROW_CELLS);
            table_set(t, 0, "  %s", xy(buf, chest->x, chest->y));
            table_set(t, 1, "%s", item_name(chest->item));
            table_set(t, 2, "%s", guard_name(rom, map, chest->x, chest->y));
        }
    }
    table_print_halves(pos, end, t);

    /* Only three, all in different places, so one row each. Shuffle Chests
     * & Searches leaves each one empty a fifth of the time; when the
     * overworld one is, the game stops pointing at its tile - Cantlin's hint
     * becomes "Thou must go fight!" - so the log gives no tile either. */
    add(pos, end, "\nSearch spots\n\n");
    t = treasure_table(rom);
    for (m=0; m < sizeof(map_order); m++) {
        for (i=0; i < SEARCH_COUNT; i++) {
            if (search->map[i] != map_order[m])
                continue;
            table_row(t, ROW_CELLS);
            if (search->map[i] == OVERWORLD && !search->item[i])
                table_set(t, 0, "%s", map_name(search->map[i]));
            else
                table_set(t, 0, "%s %s", map_name(search->map[i]),
                        xy(buf, search->x[i], search->y[i]));
            table_set(t, 1, "%s", search->item[i]
                    ? item_name(search->item[i]) : "nothing");
            table_set(t, 2, "%s", guard_name(rom, search->map[i],
                    search->x[i], search->y[i]));
        }
    }
    table_print(pos, end, t);
}

/**
 * The location an entrance sits inside, as an index into locations[], or -1
 * if it is not inside any of them.
 */
static int location_containing(dw_rom *rom, uint8_t map)
{
    size_t i;

    for (i=0; i < LOCATION_COUNT; i++) {
        if (rom->map.warps_to[locations[i].warp].map == map)
            return (int)i;
    }
    return -1;
}

static void add_locations(char **pos, char *end, dw_rom *rom)
{
    size_t i, j, k;
    int parent;
    BOOL listed[LOCATION_COUNT] = { FALSE };
    char buf[XY_SIZE];
    table *t;

    section(pos, end, 5);
    add(pos, end, "Where each entrance is on the overworld. An arrow (->) "
            "marks where the\nbasement stairs in Tantegel Castle and Garinham "
            "lead.\n\n");

    t = table_new(2);
    if (!t)
        return;
    t->head[0] = "Place";
    t->head[1] = "Overworld";
    for (i=0; i < LOCATION_COUNT; i++) {
        dw_warp *from = &rom->map.warps_from[locations[i].warp];
        if (from->map != OVERWORLD)
            continue;
        table_row(t, ROW_CELLS);
        table_set(t, 0, "%s", locations[i].name);
        table_set(t, 1, "%s", xy(buf, from->x, from->y));
        listed[i] = TRUE;
        for (j=0; j < LOCATION_COUNT; j++) {
            if (listed[j])
                continue;
            parent = location_containing(rom,
                    rom->map.warps_from[locations[j].warp].map);
            if (parent == (int)i) {
                table_row(t, ROW_CELLS);
                table_set(t, 0, "  -> %s", locations[j].name);
                listed[j] = TRUE;
            }
        }
    }

    /* anything inside a place that is not itself a location, by map */
    for (i=0; i < LOCATION_COUNT; i++) {
        uint8_t map = rom->map.warps_from[locations[i].warp].map;
        if (listed[i])
            continue;
        table_label(t, "%s", map_name(map));
        for (k=i; k < LOCATION_COUNT; k++) {
            if (!listed[k]
                    && rom->map.warps_from[locations[k].warp].map == map) {
                table_row(t, ROW_CELLS);
                table_set(t, 0, "  -> %s", locations[k].name);
                listed[k] = TRUE;
            }
        }
    }
    table_print_halves(pos, end, t);
}

#define HALVES_SIZE 32768
#define HALVES_GAP  8

/**
 * Whether two group labels name one place: the same first word, as in
 * Mountain B1 and Mountain B2, or Tantegel Throne Room and Tantegel Treasury.
 */
static BOOL same_place(const char *a, const char *b)
{
    size_t len = strcspn(a, " ");

    return len == strcspn(b, " ") && !strncmp(a, b, len);
}

/**
 * Prints a table as two halves side by side, so it is half as long. The rows
 * are split between groups - each label starts one - so no group is cut in
 * two, at the point that leaves the halves closest in length with the right
 * one never the longer. Frees the table.
 */
static void table_print_halves(char **pos, char *end, table *t)
{
    char *buf, *p, *lines[TABLE_ROWS + 4];
    int n = 0, i, s, pass, width, left, right, best = -1, best_height = 0;
    int starts[TABLE_ROWS], groups = 0;

    buf = malloc(HALVES_SIZE);
    if (!buf) {
        table_print(pos, end, t);
        return;
    }
    p = buf;
    *p = '\0';
    table_print(&p, buf + HALVES_SIZE, t);

    /* heading, '=' rule, the rows, '-' rule */
    for (p=buf; *p && n < TABLE_ROWS + 4; ) {
        lines[n++] = p;
        p = strchr(p, '\n');
        if (!p)
            break;
        *p++ = '\0';
    }
    for (i=2; i < n - 1; i++) {
        if (lines[i][0] && lines[i][0] != ' ')
            starts[groups++] = i;
    }
    width = (int)strlen(lines[1]);

    /* the split that leaves the two halves closest in length with the right
     * one no longer than the left, and never between two groups of one place
     * - unless there is nowhere else to split */
    for (pass=0; pass < 2 && best < 0; pass++) {
        for (s=1; s < groups; s++) {
            if (!pass && same_place(lines[starts[s - 1]], lines[starts[s]]))
                continue;
            for (left=starts[s] - 1; left > 2 && !lines[left][0]; left--)
                ;
            left -= 1;
            right = n - 1 - starts[s];
            if (!pass && right > left)
                continue;
            if (best < 0 || MAX(left, right) < best_height) {
                best = s;
                best_height = MAX(left, right);
            }
        }
    }
    if (best < 0 || n < 4) {
        for (i=0; i < n; i++)
            add(pos, end, "%s\n", lines[i]);
        free(buf);
        return;
    }

    for (left=starts[best] - 1; left > 2 && !lines[left][0]; left--)
        ;
    left -= 1;
    right = n - 1 - starts[best];
    add(pos, end, "%-*s%*s%s\n", width, lines[0], HALVES_GAP, "", lines[0]);
    add_rule(pos, end, '=', width * 2 + HALVES_GAP);
    for (i=0; i < MAX(left, right); i++) {
        add(pos, end, "%-*s%*s%s\n", width, i < left ? lines[2 + i] : "",
                HALVES_GAP, "", i < right ? lines[starts[best] + i] : "");
    }
    add_rule(pos, end, '-', width * 2 + HALVES_GAP);
    free(buf);
}

static void add_shops(char **pos, char *end, dw_rom *rom)
{
    size_t i;
    uint8_t *item;
    int index;
    uint8_t town = NO_MAP;
    BOOL first;
    int first_row;
    table *t;

    section(pos, end, 6);
    add(pos, end, "(Locked) shops are behind Cantlin's locked door.\n\n");
    t = table_new(3);
    if (!t)
        return;
    t->head[0] = "Shop";
    t->head[1] = "Item";
    t->head[2] = "Price";
    t->align[2] = ALIGN_RIGHT;
    for (i=0; i < sizeof(shops) / sizeof(shops[0]); i++) {
        /* a blank line between shops, two between towns */
        if (shops[i].map != town) {
            if (town != NO_MAP) {
                table_row(t, ROW_BLANK);
                table_row(t, ROW_BLANK);
            }
            town = shops[i].map;
            table_label(t, "[%s]", map_name(town));
        } else {
            table_row(t, ROW_BLANK);
        }
        /* the shop is named on its first row only, and which of a town's
         * shops it is, "(North)", on its second */
        first_row = table_row(t, ROW_CELLS);
        table_set(t, 0, "  [%s]", shop_kinds[shops[i].kind]);
        if (!has_shopkeeper(rom, town, shops[i].dialog)) {
            /* No Keys removes the key shopkeepers */
            table_set(t, 1, "none in this seed");
        } else switch (shops[i].kind) {
            case SELLS_KEYS:
                table_set(t, 1, "%s", shop_item_name(SHOP_MAGIC_KEY));
                table_set(t, 2, "%d",
                        rom->content[KEY_PRICES + shops[i].dialog - SHOP_KEYS]);
                break;
            case SELLS_FAIRY:
                table_set(t, 1, "%s", shop_item_name(PRICE_FAIRY));
                table_set(t, 2, "%d", rom->weapon_prices[PRICE_FAIRY]);
                break;
            default:
                /* the inventory table is one list per shop, each ended by
                 * SHOP_END, in dialog byte order */
                item = rom->weapon_shops;
                for (index=0; index < shops[i].dialog; item++) {
                    if (*item == SHOP_END)
                        index++;
                }
                for (first=TRUE; *item != SHOP_END; item++, first=FALSE) {
                    if (!first)
                        table_row(t, ROW_CELLS);
                    table_set(t, 1, "%s", shop_item_name(*item));
                    table_set(t, 2, "%d", rom->weapon_prices[*item]);
                }
        }
        if (*shops[i].label && first_row >= 0) {
            if (t->rows == first_row + 1)
                table_row(t, ROW_CELLS);
            snprintf(t->cell[first_row + 1][0], CELL_SIZE, "  %s",
                    shops[i].label);
        }
    }
    table_print_halves(pos, end, t);
}

/**
 * The place an encounter tile is in. The game's own third tile sits on the
 * Dragonlord's floor, the only way through to him, and is called plain
 * Charlock rather than the room it is in. A Treasure Guard is named the way
 * its chest is under Chests & search spots - Charlock Treasury, Tantegel
 * Treasury - so the two lists agree.
 */
static const char *encounter_place(dw_rom *rom, int i)
{
    dwr_spike_table *spike = rom->spike_table;

    if (i == 2 && spike->map[i] == CHARLOCK_THRONE_ROOM)
        return "Charlock";
    return chest_place(spike->map[i], spike->x[i], spike->y[i]);
}

static void add_encounters(char **pos, char *end, dw_rom *rom)
{
    int i, count = 0;
    char buf[XY_SIZE];
    table *t;
    dwr_spike_table *spike = rom->spike_table;

    add(pos, end, "\nEncounter tiles\n\n");
    for (i=0; i < SPIKE_COUNT; i++)
        count += spike->map[i] != NO_MAP;
    if (!count) {
        add(pos, end, "None in this seed.\n");
        return;
    }
    /* the game's own three have no "beaten" bit, so they never go away */
    add(pos, end, "Stepping on one of these tiles starts a fight. The first "
            "three are the game's\nown and fight every time; Treasure Guards "
            "fight until beaten.\n\n");
    /* What each tile guards is not repeated here: a guarded chest or search
     * spot says so under Chests & search spots, and the two that guard a
     * route rather than a treasure are the game's own, in the same place
     * every seed. */
    t = table_new(2);
    if (!t)
        return;
    t->head[0] = "Tile";
    t->head[1] = "Enemy";
    for (i=0; i < SPIKE_COUNT; i++) {
        if (spike->map[i] == NO_MAP)
            continue;
        table_row(t, ROW_CELLS);
        table_set(t, 0, "%s %s", encounter_place(rom, i),
                xy(buf, spike->x[i], spike->y[i]));
        table_set(t, 1, "%s", enemy_name(spike->monster[i]));
    }
    table_print(pos, end, t);
}

static void add_stats(char **pos, char *end, dw_rom *rom,
        char spells[SPELL_COUNT][16])
{
    int level, b, s, j, v, bonus, col;
    uint16_t learned, before = 0;
    dw_stats *st;
    char list[64];
    table *t;
    const char *stat_names[] = { "HP", "MP", "Str", "Agi" };

    bonus = rom->content[BUILD_BONUS] == 0x69
            ? rom->content[BUILD_BONUS + 1] : 3;

    section(pos, end, 7);

    /* the spells first: a short list, and it answers a different question
     * than the table below it */
    add(pos, end, "Spells learned\n\n");
    t = table_new(2);
    if (!t)
        return;
    t->head[0] = "Level";
    t->head[1] = "Spells";
    t->align[0] = ALIGN_RIGHT;
    for (level=0; level < 30; level++) {
        st = &rom->stats[level];
        learned = st->spells & ~before;
        before = st->spells;
        if (!learned)
            continue;
        list[0] = '\0';
        for (j=0; j < SPELL_COUNT; j++) {
            if (learned & 1 << (j + 8) % 16)
                snprintf(list + strlen(list), sizeof(list) - strlen(list),
                        "%s%s", list[0] ? ", " : "", spells[j]);
        }
        table_row(t, ROW_CELLS);
        table_set(t, 0, "%d", level + 1);
        table_set(t, 1, "%s", list);
    }
    if (!t->rows) {
        table_row(t, ROW_CELLS);
        table_set(t, 1, "None");
    }
    table_print(pos, end, t);

    /* the four builds side by side, each heading its four stats */
    add(pos, end, "\nStats by build\n\n");
    add(pos, end, "Exp is the total experience needed to reach each level. "
            "The hero's name\npicks one of four builds: its two named stats "
            "grow in full, and the other\ntwo are 90%% of the table, plus "
            "%d.\n\n", bonus);
    t = table_new(2 + 16);
    if (!t)
        return;
    t->head[0] = "Level";
    t->head[1] = "Exp";
    t->align[0] = t->align[1] = ALIGN_RIGHT;
    for (b=0; b < 4; b++) {
        t->over[2 + b * 4] = builds[b].label;
        t->over_span[2 + b * 4] = 4;
        for (s=HP; s <= AGI; s++) {
            col = 2 + b * 4 + s;
            t->head[col] = stat_names[s];
            t->align[col] = ALIGN_RIGHT;
            t->gap[col] = s == HP ? 4 : 2;
            t->width[col] = 3;
        }
    }
    for (level=0; level < 30; level++) {
        st = &rom->stats[level];
        table_row(t, ROW_CELLS);
        table_set(t, 0, "%d", level + 1);
        table_set(t, 1, "%d", (int)rom->xp_reqs[level]);
        for (b=0; b < 4; b++) {
            for (s=HP; s <= AGI; s++) {
                v = s == HP ? st->hp : s == MP ? st->mp
                        : s == STR ? st->str : st->agi;
                if (!(builds[b].full & 1 << s) && (s != MP || v))
                    v = v * 9 / 10 + bonus;
                table_set(t, 2 + b * 4 + s, "%3d", v);
            }
        }
    }
    table_print(pos, end, t);
}

/** One enemy ability, in capitals, and how often it is used, or nothing. */
static void set_ability(table *t, int col, const char *name, int chance)
{
    char *c;

    if (!chance)
        return;
    table_set(t, col, "%s %d%%", name, chance * 25);
    for (c=t->cell[t->rows - 1][col]; *c; c++)
        *c = toupper((unsigned char)*c);
}

static void add_enemies(char **pos, char *end, dw_rom *rom,
        char spells[SPELL_COUNT][16])
{
    int i, move, c;
    dw_enemy *e;
    table *t;
    const char *support[] = {
        spells[SLEEP], spells[STOPSPELL], spells[HEAL], spells[HEALMORE]
    };
    const char *combat[] = {
        spells[HURT], spells[HURTMORE], "Fire breath", "DL2 breath"
    };
    const char *heads[] = {
        "Enemy", "Str", "Def", "HP", "Exp", "Gold", "Dodge", "Sleep", "Stop",
        "Hurt", "Ability 1", "Ability 2"
    };

    section(pos, end, 8);
    add(pos, end, "Str is the enemy's attack and Def its defense. Dodge is "
            "out of 64, resistances\nout of 16. Ability 1 is a support spell "
            "and Ability 2 an attack; the\npercentage is how often the enemy "
            "uses it.\n\n");
    t = table_new(12);
    if (!t)
        return;
    /* the numbers sit close together; the names and abilities stand apart */
    for (c=0; c < 12; c++) {
        t->head[c] = heads[c];
        t->align[c] = c == 0 || c >= 10 ? ALIGN_LEFT : ALIGN_RIGHT;
        t->gap[c] = c >= 2 && c <= 9 ? 2 : 0;
    }
    t->over[7] = "Resists";
    t->over_span[7] = 3;
    for (i=SLIME; i <= DRAGONLORD_2; i++) {
        e = &rom->enemies[i];
        table_row(t, ROW_CELLS);
        table_set(t, 0, "%s", enemy_name(i));
        table_set(t, 1, "%d", e->str);
        table_set(t, 2, "%d", e->agi);
        table_set(t, 3, "%d", e->hp);
        table_set(t, 4, "%d", (int)e->xp);
        table_set(t, 5, "%d", (int)e->gold);
        table_set(t, 6, "%d", e->hr_dodge & 0x0f);
        table_set(t, 7, "%d", e->s_ss_resist >> 4);
        table_set(t, 8, "%d", e->s_ss_resist & 0x0f);
        table_set(t, 9, "%d", e->hr_dodge >> 4);
        move = e->pattern;
        set_ability(t, 10, support[move >> 6], (move >> 4) & 3);
        set_ability(t, 11, combat[(move >> 2) & 3], move & 3);
    }
    table_print(pos, end, t);
}

/**
 * Where a zone's group is used below ground, shortest form first:
 * "Grave B3-B4", "Charlock B5-B6/Bottom", "Swamp Cave/Mountain B1".
 */
static void add_zone_places(char **pos, char *end, dw_rom *rom, int zone)
{
    size_t m, run;
    uint8_t map, last;
    const char *prev = NULL;

    for (m=0; m < sizeof(map_order); m++) {
        map = map_order[m];
        if (map == OVERWORLD || map_zone(rom, map) != zone)
            continue;

        /* a run of floors of one dungeon, adjacent in map_order */
        run = m;
        while (maps[map].floor && map != CHARLOCK_THRONE_ROOM
                && run + 1 < sizeof(map_order)) {
            uint8_t next = map_order[run + 1];
            if (map_zone(rom, next) != zone || !maps[next].dungeon
                    || strcmp(maps[next].dungeon, maps[map].dungeon)
                    || !maps[next].floor || next == CHARLOCK_THRONE_ROOM)
                break;
            run++;
        }
        last = map_order[run];

        add(pos, end, "%s", prev ? "/" : "");
        if (!maps[map].floor) {
            add(pos, end, "%s", maps[map].name);
        } else {
            if (!prev || strcmp(prev, maps[map].dungeon))
                add(pos, end, "%s ", maps[map].dungeon);
            add(pos, end, "%s", maps[map].floor);
            if (last != map)
                add(pos, end, "-%s", maps[last].floor);
        }
        prev = maps[map].dungeon ? maps[map].dungeon : maps[map].name;
        m = run;
    }
}

static void add_zones(char **pos, char *end, dw_rom *rom)
{
    int row, col, zone, i;
    char *p;
    table *t;

    section(pos, end, 9);
    add(pos, end, "Random fights\n\n");
    add(pos, end, "The overworld is split into an 8 x 8 grid of zones, each 15 "
            "x 15 tiles. Each\ncell shows which enemy group attacks there. "
            "Tantegel Castle is always in\nzone 0, shown as [0], and zones 1 "
            "and 2 border it.\n\n");
    for (row=0; row < 8; row++) {
        for (col=0; col < 8; col++) {
            zone = zone_at(rom, row * 8 + col);
            if (zone == 0)
                add(pos, end, " [0]");
            else
                add(pos, end, "%4d", zone);
        }
        add(pos, end, "\n");
    }

    add(pos, end, "\nEnemy groups: each fight picks one of the five.\n\n");
    t = table_new(7);
    if (!t)
        return;
    t->head[0] = "Zone";
    t->head[1] = "Enemies";
    t->head[6] = "Dungeon Zones";
    t->align[0] = ALIGN_RIGHT;
    for (zone=0; zone < 20; zone++) {
        table_row(t, ROW_CELLS);
        table_set(t, 0, "%d", zone);
        for (i=0; i < 5; i++)
            table_set(t, 1 + i, "%s", enemy_name(rom->zones[zone * 5 + i]));
        p = t->cell[t->rows - 1][6];
        add_zone_places(&p, p + CELL_SIZE, rom, zone);
    }
    table_print(pos, end, t);
}

/**
 * Removes the spaces left at the ends of lines by fixed-width columns that
 * have nothing in them, in place.
 */
static void strip_trailing_spaces(char *text)
{
    char *in, *out, *spaces = NULL;

    for (in=out=text; *in; in++) {
        if (*in == ' ') {
            if (!spaces)
                spaces = out;
        } else if (*in == '\n' && spaces) {
            out = spaces;
            spaces = NULL;
        } else {
            spaces = NULL;
        }
        *out++ = *in;
    }
    if (spaces)
        out = spaces;
    *out = '\0';
}

char *dwr_spoiler(dw_rom *rom, uint64_t crc)
{
    char *buf, *pos, *end;
    char spells[SPELL_COUNT][16];
    const char truncated[] = "\n(log truncated)\n";

    buf = malloc(SPOILER_SIZE);
    if (!buf)
        return NULL;
    pos = buf;
    end = buf + SPOILER_SIZE;
    read_spell_names(rom, spells);
    read_gold(rom);

    add_header(&pos, end, rom, crc);
    add_legend(&pos, end, rom);
    add_flags(&pos, end, rom);
    add_key_items(&pos, end, rom);
    add_treasures(&pos, end, rom);
    add_locations(&pos, end, rom);
    add_shops(&pos, end, rom);
    add_stats(&pos, end, rom, spells);
    add_enemies(&pos, end, rom, spells);
    add_zones(&pos, end, rom);
    add_encounters(&pos, end, rom);

    if (pos >= end - 1)
        strcpy(end - sizeof(truncated), truncated);
    strip_trailing_spaces(buf);
    return buf;
}

BOOL dwr_write_spoiler(dw_rom *rom, uint64_t crc, const char *path)
{
    FILE *out;
    char *text = dwr_spoiler(rom, crc);

    if (!text)
        return FALSE;
    out = fopen(path, "w");
    if (!out) {
        free(text);
        return FALSE;
    }
    fputs(text, out);
    fclose(out);
    free(text);
    return TRUE;
}
