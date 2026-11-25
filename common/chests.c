
#include <string.h>
#include <stdio.h>

#include "dwr.h"
#include "mt64.h"
#include "patch.h"
#include "dungeon_maps.h"
#include "map.h"

/**
 * Returns a random index of a chest that is not in charlock.
 *
 * @param rom The rom struct.
 *
 * @return The index of a chest accessible outside Charlock.
 */
static inline int non_charlock_chest(dw_rom *rom)
{
    int chest;

    while (TRUE) {
        chest = (int)mt_rand(0, 30);
        if (!(rom->chest_access[chest] & 0x88))
            break;
    }
    return chest;
}

/**
 * Determines whether or not the chest item is a quest item.
 *
 * @param item The item member of a chest
 * @return A boolean indicated whether or not the item is crucial for the quest
 *      to obtain the rainbow drop.
 */
BOOL is_quest_item(uint8_t item)
{
    switch(item) {
        case TOKEN:
        case STONES:
        case HARP:
        case STAFF:
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * Determines whether or not the chest item is a quest item. This function is
 * for use when "No Armor in Charlock" is on.
 *
 * @param item The item member of a chest
 * @return A boolean indicated whether or not the item is crucial for the quest
 *      to obtain the rainbow drop.
 */
static BOOL is_quest_item_incl_armor(uint8_t item)
{
    switch(item) {
        case TOKEN:
        case STONES:
        case HARP:
        case STAFF:
        case ARMOR:
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * Determines if a chest should be left alone when randomizing
 *
 * @param chest The chest to be checked
 *
 * @return a boolean indicating whether the chest should be left alone.
 */
static inline BOOL is_static_chest(dw_chest *chest)
{
    return
        (chest->map == TANTEGEL_THRONE_ROOM && chest->x == 6 && chest->y == 1)
        || chest->item == STAFF;
}


/**
 * Looks for quest items which may be in Charlock due to shuffling and moves
 * them to a chest outside Charlock.
 *
 * @param rom The rom struct.
 */
void check_quest_items(dw_rom *rom)
{
    int i, tmp_index;
    uint8_t tmp_item;
    BOOL (*quest_item_check)(uint8_t item);

    printf("Checking quest item placement...\n");

    quest_item_check = &is_quest_item;

    if (NO_ARMOR_IN_CHARLOCK(rom)) {
        printf("Disallowing Armor in Charlock...\n");
        quest_item_check = &is_quest_item_incl_armor;
    }

    for (i=0; i < CHEST_COUNT; i++) {
        if (rom->chest_access[i] & 0x88) {
            if ((*quest_item_check)(rom->chests[i].item)) {
                do {
                    tmp_index = non_charlock_chest(rom);
                } while ((*quest_item_check)(rom->chests[tmp_index].item) 
                        || is_static_chest(&rom->chests[tmp_index]));
                tmp_item = rom->chests[tmp_index].item;
                rom->chests[tmp_index].item = rom->chests[i].item;
                rom->chests[i].item = tmp_item;
            }
        }
    }
}

/**
 * Determines whether a chest holds gold.
 *
 * The chest byte has five gold values, not one: GOLD (0x12) is the only one the
 * randomizer writes, but a seed with chest shuffle off keeps vanilla's 0x13-0x16
 * as well. 0x17 is Erdrick's tablet and is deliberately outside the range.
 *
 * @param item The item member of a chest
 * @return A boolean indicating whether the chest holds gold
 */
static inline BOOL is_gold_chest(uint8_t item)
{
    return item >= GOLD && item <= 0x16;
}

/**
 * Ranks a chest as a candidate to be turned into the guaranteed gold chest.
 * Lower is better. Quest items are never candidates - moving one is how seeds
 * become unwinnable - and neither are the static chests.
 *
 * @param chest The chest to be ranked
 * @return A rank, or -1 if the chest must be left alone
 */
static int gold_candidate_rank(dw_chest *chest)
{
    if (is_static_chest(chest) || is_quest_item(chest->item))
        return -1;

    switch(chest->item) {
        case HERB:
        case TORCH:
        case FAIRY_WATER:
        case CURSED_BELT:
            return 0;   /* junk: the cheapest thing to overwrite */
        case WINGS:
        case DRAGON_SCALE:
        case KEY:
            return 1;
        default:
            return 2;   /* an equipment or key item: a last resort */
    }
}

static BOOL is_dungeon_tileset(dw_map_index map);

/** Tantegel castle's chest spots, the same eleven move_chests() uses. */
static const uint8_t tantegel_spots[][2] = {
    { 1, 13}, { 2, 13}, { 3, 13}, { 1, 14}, { 2, 14}, { 3, 14},
    { 1, 15}, { 2, 15}, { 3, 15}, {16, 15}, {24, 21},
};

/** Whether a map is part of Charlock: the castle, its throne room or a cave. */
static inline BOOL is_charlock_map(uint8_t map)
{
    return map == CHARLOCK || map == CHARLOCK_THRONE_ROOM
            || (map >= CHARLOCK_CAVE_1 && map <= CHARLOCK_CAVE_6);
}

/**
 * Whether a map is one of the floors Stair Shuffle can reroute: Charlock's two
 * town floors and every cave but Swamp Cave. chest_paths() records, for each
 * chest on these floors, which cave entrances reach it.
 */
static inline BOOL is_stair_floor(uint8_t map)
{
    return map == CHARLOCK || map == CHARLOCK_THRONE_ROOM
            || (map >= CHARLOCK_CAVE_1 && map <= ERDRICKS_CAVE_2
                && map != SWAMP_CAVE);
}

/* chest_access bits set by chest_paths() for a chest reached, with keys, from
 * the Tablet Cave, Mountain Cave and Garin's Grave entrances */
#define TABLET_ACCESS   0x10
#define MOUNTAIN_ACCESS 0x20
#define GRAVE_ACCESS    0x40
#define OVERWORLD_DUNGEON_ACCESS (TABLET_ACCESS | MOUNTAIN_ACCESS | GRAVE_ACCESS)

/**
 * Whether a chest belongs to Charlock's dungeons. Without Stair Shuffle that is
 * the map it is on. With it the stairs decide (user, 2026-11-25: Stair Shuffle
 * splits the floors into the Overworld Dungeons and the Charlock Dungeons): a
 * chest on a shuffled floor is Charlock's only when no other cave entrance
 * reaches it, so a Charlock floor that rolled into an overworld dungeon is not
 * Charlock's, and a Grave or Mountain floor reached only through Charlock is.
 *
 * @param rom The rom struct
 * @param i The chest's index
 * @return TRUE if the chest can only be reached through Charlock
 */
static BOOL charlock_chest(dw_rom *rom, size_t i)
{
    uint8_t map = rom->chests[i].map;

    if (STAIR_SHUFFLE(rom) && is_stair_floor(map))
        return !(rom->chest_access[i] & OVERWORLD_DUNGEON_ACCESS);
    return is_charlock_map(map);
}

/**
 * Ranks a chest elsewhere as the one to move into Tantegel castle when no
 * castle chest can be made gold. Lower is better: a gold chest outside
 * Charlock, then a gold chest in Charlock (taking gold out of Charlock is
 * harmless), then the cheapest junk chest, which is overwritten with gold.
 *
 * @param rom The rom struct
 * @param i The index of the chest to be ranked
 * @return A rank, or -1 if the chest must stay where it is
 */
static int gold_mover_rank(dw_rom *rom, size_t i)
{
    int rank;
    dw_chest *chest = &rom->chests[i];

    if (chest->map == TANTEGEL || chest->map == TANTEGEL_THRONE_ROOM)
        return -1;
    if (is_static_chest(chest) || is_quest_item(chest->item))
        return -1;
    if (is_gold_chest(chest->item))
        return charlock_chest(rom, i) ? 1 : 0;
    rank = gold_candidate_rank(chest);
    return rank < 0 ? -1 : 2 + rank;
}

/**
 * Moves a chest from elsewhere into a free Tantegel castle spot and makes sure
 * it holds gold. The old spot's chest tile is bricked over and the new spot
 * gets one, as move_chests() does. Moving a gold chest keeps the item pool
 * exactly as drawn. The first free spot in the list is used, so no RNG is
 * consumed. Like chest_at(), "free" ignores the map: no two chests anywhere
 * share coordinates.
 *
 * A key is never chosen. This runs after map generation, when need_rimuldar()
 * has already decided where Rimuldar may go based on which caves hold a key
 * (check_keys()), and turning that key into gold would leave the seed with no
 * obtainable key. Gold and junk chests rank first, so a key could only be
 * reached with none of those left anywhere; this makes that impossible rather
 * than unlikely.
 *
 * @param rom The rom struct
 */
static BOOL move_chest_into_tantegel(dw_rom *rom, size_t mover, BOOL make_gold);

static void add_gold_chest_to_tantegel(dw_rom *rom)
{
    size_t i, mover = CHEST_COUNT;
    int rank, best_rank = 99;
    dw_chest *chest = rom->chests;

    for (i=0; i < CHEST_COUNT; i++) {
        if (chest[i].item == KEY)
            continue;
        rank = gold_mover_rank(rom, i);
        if (rank >= 0 && rank < best_rank) {
            best_rank = rank;
            mover = i;
        }
    }
    if (mover >= CHEST_COUNT) {
        printf("  no chest could be moved into Tantegel (should not happen)\n");
        return;
    }
    move_chest_into_tantegel(rom, mover, TRUE);
}

/**
 * Moves one chest into the first free Tantegel castle spot, bricking over its
 * old spot and drawing the new chest tile, as move_chests() does. No RNG.
 *
 * @param rom The rom struct
 * @param mover The index of the chest to move
 * @param make_gold Whether to turn a chest that is not gold into gold
 * @return TRUE if a free spot was found and the chest moved
 */
static BOOL move_chest_into_tantegel(dw_rom *rom, size_t mover, BOOL make_gold)
{
    size_t i, j;
    dw_chest *chest = rom->chests;

    for (i=0; i < sizeof(tantegel_spots) / 2; i++) {
        for (j=0; j < CHEST_COUNT; j++) {
            if (chest[j].x == tantegel_spots[i][0]
                    && chest[j].y == tantegel_spots[i][1])
                break;
        }
        if (j < CHEST_COUNT)
            continue;   /* taken */

        set_dungeon_tile(rom, chest[mover].map, chest[mover].x, chest[mover].y,
                is_dungeon_tileset(chest[mover].map) ? DUNGEON_TILE_BRICK
                                                     : TOWN_TILE_BRICK);
        chest[mover].map = TANTEGEL;
        chest[mover].x = tantegel_spots[i][0];
        chest[mover].y = tantegel_spots[i][1];
        set_dungeon_tile(rom, TANTEGEL, chest[mover].x, chest[mover].y,
                TOWN_TILE_CHEST);
        if (make_gold && !is_gold_chest(chest[mover].item))
            chest[mover].item = GOLD;
        printf("  moved a chest into Tantegel at %d,%d\n",
                chest[mover].x, chest[mover].y);
        return TRUE;
    }
    printf("  no free chest spot in Tantegel\n");
    return FALSE;
}

/* The warps that are real stairs and doors; the table's entries past these
 * are other data (see dwr-map's WARP_COUNT). */
#define GRIND_WARP_COUNT 51

static uint8_t grind_find(uint8_t *parent, uint8_t m)
{
    while (parent[m] != m)
        m = parent[m] = parent[parent[m]];
    return m;
}

/**
 * Whether a chest's item can count towards starting the grind. Erdrick's
 * Sword and Armor never do: with Progressive Equipment they are refused until
 * late, and leaving them out costs nothing when they can be opened.
 */
static inline BOOL grind_item(dw_chest *chest)
{
    return chest->item != SWORD && chest->item != ARMOR;
}

/**
 * Checks whether the gold grind can be started before Charlock. Maps joined
 * by warps that do not touch the overworld form a group: one trip, no
 * overworld load in between. The throne room's group T is always the end of
 * the trip (the death warp lands there). The grind starts if some group C
 * other than Charlock's own has chests(C) + chests(T) - 1 >= 8, or chests(T)
 * - 1 >= 8 when C is T (the -1 is the gold chest, which must come after the
 * list is full). Charlock's chests (charlock_chest()) are never counted, and
 * a group is Charlock's own when its only way in from the overworld is
 * Charlock's entrance - with or without Open Charlock: the grind must never
 * need Charlock.
 *
 * With Stair Shuffle the stairs can join any floors, so a group of maps is no
 * longer one walk: the shuffled floors leave the groups, and each cave
 * entrance is a trip of its own instead - the chests chest_paths() saw it
 * reach, plus the group of the town it starts in (Garin's Grave starts in
 * Garinham).
 *
 * @param rom The rom struct
 * @param charlock_only Set per map: TRUE if it is in a Charlock-only group
 * @return TRUE if the grind can be started before Charlock
 */
static BOOL early_grind_possible(dw_rom *rom, BOOL *charlock_only)
{
    static const struct { dw_warp_index warp; uint8_t access; } caves[] = {
        { WARP_ERDRICKS_CAVE, TABLET_ACCESS },
        { WARP_MOUNTAIN_CAVE, MOUNTAIN_ACCESS },
        { WARP_GARINS_GRAVE,  GRAVE_ACCESS },
    };
    uint8_t parent[ERDRICKS_CAVE_2 + 1], a, b, g, t_group;
    int count[ERDRICKS_CAVE_2 + 1] = {0}, other_entry[ERDRICKS_CAVE_2 + 1] = {0};
    int t, n;
    size_t i, e;
    dw_warp *from = rom->map.warps_from, *to = rom->map.warps_to;
    BOOL ok = FALSE, shuffled = STAIR_SHUFFLE(rom) != 0;

    for (i=0; i <= ERDRICKS_CAVE_2; i++)
        parent[i] = (uint8_t)i;
    for (i=0; i < GRIND_WARP_COUNT; i++) {
        a = from[i].map;
        b = to[i].map;
        if (a > ERDRICKS_CAVE_2 || b > ERDRICKS_CAVE_2)
            continue;
        if (a == NO_MAP || b == NO_MAP || a == OVERWORLD || b == OVERWORLD)
            continue;
        if (shuffled && (is_stair_floor(a) || is_stair_floor(b)))
            continue;
        parent[grind_find(parent, a)] = grind_find(parent, b);
    }
    for (i=0; i < GRIND_WARP_COUNT; i++) {
        a = from[i].map;
        b = to[i].map;
        if (a > ERDRICKS_CAVE_2 || b > ERDRICKS_CAVE_2)
            continue;
        if (i == WARP_CHARLOCK)
            continue;
        if (a == OVERWORLD && b > OVERWORLD)
            other_entry[grind_find(parent, b)] = 1;
        else if (b == OVERWORLD && a > OVERWORLD)
            other_entry[grind_find(parent, a)] = 1;
    }
    for (i=0; i < CHEST_COUNT; i++) {
        a = rom->chests[i].map;
        if (a > ERDRICKS_CAVE_2 || (shuffled && is_stair_floor(a)))
            continue;
        if (grind_item(&rom->chests[i]) && !charlock_chest(rom, i))
            count[grind_find(parent, a)]++;
    }
    t_group = grind_find(parent, TANTEGEL_THRONE_ROOM);
    t = count[t_group];
    for (i=0; i <= ERDRICKS_CAVE_2; i++) {
        g = grind_find(parent, (uint8_t)i);
        charlock_only[i] = !other_entry[g] && g != t_group
                && !(shuffled && is_stair_floor((uint8_t)i));
        if (g != i || charlock_only[i] || !count[g])
            continue;
        if ((g == t_group && t - 1 >= 8) || (g != t_group && count[g] + t - 1 >= 8))
            ok = TRUE;
    }
    if (!shuffled)
        return ok;

    for (e=0; e < sizeof(caves) / sizeof(caves[0]); e++) {
        n = 0;
        for (i=0; i < CHEST_COUNT; i++) {
            if (is_stair_floor(rom->chests[i].map)
                    && grind_item(&rom->chests[i])
                    && (rom->chest_access[i] & caves[e].access))
                n++;
        }
        a = from[caves[e].warp].map;
        if (a != OVERWORLD && a <= ERDRICKS_CAVE_2 && !is_stair_floor(a)) {
            g = grind_find(parent, a);
            if (g != t_group)
                n += count[g];
        }
        if (n && n + t - 1 >= 8)
            ok = TRUE;
    }
    return ok;
}

/**
 * Makes sure the gold grind can be started before Charlock (user, 2026-11-14:
 * the point of a grind is to use it before Charlock). Random Chest Locations
 * can leave every group short except Charlock's, about 1 seed in 100. Each
 * chest moved out of Charlock (or a group reached only through it) into the
 * castle adds one to the throne room's group, which helps every trip, so this
 * moves them one at a time until a trip works: gold first, then junk, never
 * quest items, static chests or Erdrick's items, and contents are left as they
 * are. No RNG, and a seed that already qualifies is untouched.
 *
 * @param rom The rom struct
 */
static void guarantee_early_grind(dw_rom *rom)
{
    BOOL charlock_only[ERDRICKS_CAVE_2 + 1];
    size_t i, mover;
    int rank, best_rank;
    dw_chest *chest = rom->chests;

    while (!early_grind_possible(rom, charlock_only)) {
        mover = CHEST_COUNT;
        best_rank = 99;
        for (i=0; i < CHEST_COUNT; i++) {
            if (chest[i].map > ERDRICKS_CAVE_2)
                continue;
            if (STAIR_SHUFFLE(rom) && is_stair_floor(chest[i].map)) {
                if (!charlock_chest(rom, i))
                    continue;
            } else if (!charlock_only[chest[i].map]
                    && !charlock_chest(rom, i)) {
                continue;
            }
            if (!grind_item(&chest[i]))
                continue;
            rank = gold_mover_rank(rom, i);
            if (rank >= 0 && rank < best_rank) {
                best_rank = rank;
                mover = i;
            }
        }
        if (mover >= CHEST_COUNT) {
            printf("  no Charlock chest could be moved to start the grind earlier\n");
            return;
        }
        printf("  making the grind possible before Charlock:\n");
        if (!move_chest_into_tantegel(rom, mover, FALSE))
            return;
    }
}

/**
 * Guarantees that Tantegel holds a gold chest, so the gold grind is always
 * available.
 *
 * The grind works because the game only remembers 8 taken chests at a time
 * ($e39a records into $601c-$602b and gives up silently once full), and the
 * list is emptied by loading the overworld (bank 0 $2f2a) but not by dying. A
 * player opens chests in one dungeon, dies on purpose, and lands in the throne
 * room with the list still holding them. Entering the throne room (once it has
 * been left at least once) writes its three chests, (4,4), (5,4) and (6,1),
 * into list slots 6-8 (bank 0 $2f3c), which is what stops them respawning; so
 * five chests in slots 1-5 - from the dungeon, plus castle chests opened before
 * the gold one - fill the list. Every further chest is unrecorded and refills.
 *
 * It has to be a chest in the castle itself: the throne room's chests are
 * taken in the first minute and recorded, so gold there is no farm until the
 * list is already full. move_chests() reserves a Tantegel spot when this flag
 * is set, so the castle always has a chest, even with Random Chest Locations.
 *
 * Contents are *swapped* with a gold chest elsewhere where possible, which
 * leaves the item pool exactly as the seed drew it and consumes no RNG, so
 * seeds that already qualify are untouched. If every castle chest holds a
 * quest item, a gold chest is moved into the castle instead.
 *
 * @param rom The rom struct
 */
static void castle_gold_chest(dw_rom *rom)
{
    size_t i, candidate = CHEST_COUNT, donor = CHEST_COUNT;
    int rank, best_rank = 99;
    dw_chest *chest = rom->chests;
    uint8_t tmp;

    for (i=0; i < CHEST_COUNT; i++) {
        if (chest[i].map != TANTEGEL)
            continue;
        if (is_gold_chest(chest[i].item))
            return;  /* nothing to do */
        rank = gold_candidate_rank(&chest[i]);
        if (rank < 0)
            continue;
        if (rank < best_rank) {
            best_rank = rank;
            candidate = i;
        }
    }
    if (candidate >= CHEST_COUNT) {
        /* Every castle chest holds a quest item (seen with Random Chest
         * Locations leaving a single castle chest), so bring a gold chest in
         * instead. */
        add_gold_chest_to_tantegel(rom);
        return;
    }

    for (i=0; i < CHEST_COUNT; i++) {
        if (i == candidate || !is_gold_chest(chest[i].item))
            continue;
        if (is_static_chest(&chest[i]))
            continue;
        /* not the throne room either: those chests are opened in the first
         * minute of the game, so swapping a key item into one would simply
         * hand it to the player at the start */
        if (chest[i].map == TANTEGEL_THRONE_ROOM)
            continue;
        /* never swap into Charlock: a key item moved in there can make the seed
         * unwinnable, which is what check_quest_items() exists to prevent.
         * With Stair Shuffle that means any chest reached only through
         * Charlock, whatever map it is on (charlock_chest()). */
        if (charlock_chest(rom, i))
            continue;
        donor = i;
        break;
    }

    if (donor < CHEST_COUNT) {
        tmp = chest[candidate].item;
        chest[candidate].item = chest[donor].item;
        chest[donor].item = tmp;
    } else {
        chest[candidate].item = GOLD;
    }
}

/**
 * Guarantees a gold chest in Tantegel castle that can be farmed before
 * Charlock: first the chest itself (castle_gold_chest()), then enough chests
 * on one trip to fill the opened-chest list (guarantee_early_grind()).
 *
 * @param rom The rom struct
 */
void guarantee_gold_chest(dw_rom *rom)
{
    if (!GUARANTEED_GOLD(rom))
        return;

    printf("Guaranteeing a gold chest in Tantegel...\n");
    castle_gold_chest(rom);
    guarantee_early_grind(rom);
}

/**
 * Patches the game code to allow Erdrick's Armor to be in a chest and other
 * items to be found on "searchable" spots
 *
 * @param rom The rom struct
 * @param items A pointer to the 3 search items
 */
static void rewrite_search_take_code(dw_rom *rom, uint8_t *items)
{
    /* patch a few branch addresses */
    vpatch(rom, 0x0e231,    1,  0x55);
    vpatch(rom, 0x0e23b,    1,  0x55);
    vpatch(rom, 0x0e26a,    1,  0x55);
    vpatch(rom, 0x0e2ad,    1,  0x55);
    vpatch(rom, 0x0e378,    1,  0x55);

    /* new "SEARCH" code */
    vpatch(rom, 0x0e10b,  43,
        0xa2, 0x02,        /*     ldx #02                                    */
        0xa5, 0x45,        /* -   lda MAP_INDEX                              */
        0xdd, 0x36, 0xe1,  /*     cmp search_map,x                           */
        0xd0, 0x1c,        /*     bne +                                      */
        0xa5, 0x3a,        /*     lda X_POS                                  */
        0xdd, 0x39, 0xe1,  /*     cmp search_x,x                             */
        0xd0, 0x15,        /*     bne +                                      */
        0xa5, 0x3b,        /*     lda Y_POS                                  */
        0xdd, 0x3c, 0xe1,  /*     cmp search_y,x                             */
        0xd0, 0x0e,        /*     bne +                                      */
        0xbd, 0x3f, 0xe1,  /*     lda search_item,x                          */
        0xf0, 0x63,        /*     beq $e18a                                  */
        0x48,              /*     pha                                        */
        0xa9, 0xff,        /*     lda #$ff                                   */
        0x85, 0x0e,        /*     sta $0e                                    */
        0x68,              /*     pla                                        */
        0x4c, 0x24,        /*     jmp $e224                                  */
        0xe2, 0xca,        /* +   dex                                        */
        0x10, 0xda,        /*     bpl -                                      */
        0x4c, 0x8a, 0xe1   /*     jmp $e18a                                  */
    );

    /* Searchable item table */
    vpatch(rom, 0x0e136,  12,
    /* search_map:  */
        OVERWORLD,      KOL, HAUKSNESS,
    /* search_x:    */
             0x53,     0x09,      0x12,
    /* search_y:    */
             0x71,     0x06,      0x0c,
    /* search_item: */
         items[0], items[1],  items[2]
    );
    /* This is freed up space. Mark it so we can find it later. */
    rom_clear(rom, 0xe142, 72);

    /* new "TAKE" code */
    vpatch(rom, 0x0e2ea,  123,
        0xc9,  0x01,        /*     cmp #$01    ; Is it the armor?            */
        0xd0,  0x1b,        /*     bne +                                     */
        0xa5,  0xbe,        /*     lda $be     ; Load the equipment byte     */
        0x29,  0x1c,        /*     and #$1c    ; See if we have the armor    */
        0xc9,  0x1c,        /*     cmp #$1c                                  */
        0xf0,  0x4f,        /*     beq ++      ; The chest is empty          */
        0xa5,  0xbe,        /*     lda $be     ; Load the equipment byte     */
        0x09,  0x1c,        /*     ora #$1c    ; Add the armor               */
        0x85,  0xbe,        /*     sta $be     ; Save it                     */
        0x20,  0x55,  0xe3, /*     jsr +++                                   */
        0xa9,  0x28,        /*     lda #$28                                  */
        0x20,  0xf0,  0xdb, /*     jsr b3_dbf0 ; "You got the armor"         */
        0xa9,  0xd5,        /*     lda #$d5                                  */
        0x4c,  0x42,  0xd2, /*     jmp b3_d242                               */
                            /*                                               */
        0xc9,  0x08,        /* +   cmp #$08    ; If it's not the flute.      */
        0xd0,  0x0b,        /*     bne +       ; Jump to the next check      */
        0xa9,  0x05,        /*     lda #$05    ; Load the flute inv value    */
        0x20,  0x55,  0xe0, /*     jsr $e055   ; Jump to inventory check     */
        0xc9,  0xff,        /*     cmp #$ff    ; Return value is $ff (found) */
        0xd0,  0x2f,        /*     bne ++      ; The chest is empty          */
        0xa9,  0x08,        /*     lda #$08    ; Reset the flute chest value */
        0xc9,  0x0a,        /* +   cmp #$0a    ; If it's not the token       */
        0xd0,  0x14,        /*     bne +       ; Jump to the next check      */
        0xa9,  0x07,        /*     lda #$07    ; Load the token inv value    */
        0x20,  0x55,  0xe0, /*     jsr $e055   ; Jump to inventory check     */
        0xc9,  0xff,        /*     cmp #$ff    ; Return value is $ff (found) */
        0xd0,  0x20,        /*     bne ++      ; The chest is empty          */
        0xa9,  0x0e,        /*     lda #$0e    ; Reset the token chest value */
        0x20,  0x55,  0xe0, /*     jsr $e055                                 */
        0xc9,  0xff,        /*     cmp #$ff                                  */
        0xd0,  0x17,        /*     bne ++                                    */
        0xa9,  0x0a,        /*     lda #$0a                                  */
        0xc9,  0x11,        /* +   cmp #$11    ; If value is >= $11          */
        0xb0,  0x03,        /*     bcs +       ; Jump to the next check      */
        0x4c,  0x69,  0xe2, /*     jmp b3_e269 ; Add the item                */
                            /*                                               */
        0xa9,  0xff,        /* +   lda #$ff                                  */
        0x85,  0x3e,        /*     sta $3e                                   */
        0xa9,  0x30,        /*     lda #$30    ; load base amount for gold   */
        0x85,  0x00,        /*     sta $00                                   */
        0xa9,  0x02,        /*     lda #$02                                  */
        0x85,  0x01,        /*     sta $01                                   */
        0xd0,  0x20,        /*     bne b3_e365                               */
        0xa5,  0x0e,        /* ++  lda $0e                                   */
        0xc9,  0xff,        /*     cmp #$ff                                  */
        0xa9,  0x00,        /*     lda #$00                                  */
        0x85,  0x0e,        /*     sta $0e                                   */
        0xb0,  0x03,        /*     bcs +                                     */
        0x4c,  0xa3,  0xe2, /*     jmp b3_e2a3                               */
        0x4c,  0xc8,  0xe1, /* +   jmp b3_e1c8                               */
                            /*                                               */
                            /*     ; Check the "searched" flag and clear it. */
        0xa5,  0x0e,        /* +++ lda $0e     ; Load the flag               */
        0xc9,  0xff,        /*     cmp #$ff    ; FF indicates search item.   */
        0xa9,  0x00,        /*     lda #$00    ; Clear the flag              */
        0x85,  0x0e,        /*     sta $0e                                   */
        0xb0,  0x66,        /*     bcs b3_e3c5 ; Search: don't play sound    */
        0xf0,  0x39,        /*     beq b3_e39a ; It's a chest.               */ 
        0xff,  0xff,  0xff,  0xff);
}


/**
 * Called when the player chooses not to shuffle items in chests. This sets
 * them to items found in the original game with a few exceptions:
 *   All gold chests aside from 500-755 gold no longer exist, so are replaced
 *     with big gold chests
 *   The tablet no longer exists, so is replaced with a Dragon's Scale
 *
 * @param rom The rom struct
 */
static void no_chest_shuffle(dw_rom *rom)
{
    size_t i;
    dw_chest *chest;
    uint8_t contents[] = {
            /* Tantegel */
            GOLD, GOLD, GOLD, GOLD, GOLD, TORCH,
            /* Rimuldar */
            WINGS,
            /* Garinham */
            GOLD, HERB, TORCH,
            /* Charlock Throne */
            HERB, GOLD, WINGS, KEY, CURSED_BELT, HERB,
            /* Tantegel basement */
            STONES,
            /* Garin's Grave */
            HERB, GOLD, GOLD, CURSED_BELT, HARP,
            /* Charlock dungeon */
            SWORD,
            /* Mountain Cave */
            NECKLACE, TORCH, RING, GOLD, HERB,
            /* Tablet cave (there is no longer a tablet in the game) */
            DRAGON_SCALE
    }, *cont = contents;
    uint8_t  search_items[] = {
           TOKEN, FLUTE, ARMOR
    };
    if (NO_NUMBERS(rom) || THREES_COMPANY(rom)) {
        contents[28] = TOKEN;
        search_items[0] = 0;
    }

    rewrite_search_take_code(rom, search_items);
    chest = rom->chests;
    for (i=0; i < CHEST_COUNT; i++) {
        /* don't move the staff or starting key */
        if (is_static_chest(chest)) {
            chest++;
            continue;
        }
        (chest++)->item = *(cont++);
    }
}

/**
 * Determines whether the given map is using a dungeon tileset.
 *
 * @param map The requested map index
 *
 * @return True if this map uses a dungeon tileset.
 */
static BOOL is_dungeon_tileset(dw_map_index map)
{
    switch(map) {
        case NO_MAP:
        case OVERWORLD:
        case CHARLOCK:
        case HAUKSNESS:
        case TANTEGEL:
        case TANTEGEL_THRONE_ROOM:
        case CHARLOCK_THRONE_ROOM:
        case KOL:
        case BRECCONARY:
        case GARINHAM:
        case CANTLIN:
        case RIMULDAR:
        case TANTEGEL_BASEMENT:
        case NORTHERN_SHRINE:
        case SOUTHERN_SHRINE:
            return FALSE;
        case CHARLOCK_CAVE_1:
        case CHARLOCK_CAVE_2:
        case CHARLOCK_CAVE_3:
        case CHARLOCK_CAVE_4:
        case CHARLOCK_CAVE_5:
        case CHARLOCK_CAVE_6:
        case SWAMP_CAVE:
        case MOUNTAIN_CAVE:
        case MOUNTAIN_CAVE_2:
        case GARINS_GRAVE_1:
        case GARINS_GRAVE_2:
        case GARINS_GRAVE_3:
        case GARINS_GRAVE_4:
        case ERDRICKS_CAVE:
        case ERDRICKS_CAVE_2:
        default:
                return TRUE;
    }
}

static int chest_bin(dw_map_index map) {
    switch(map) {
        case TANTEGEL:
            return 0;
        case GARINS_GRAVE_1:
        case GARINS_GRAVE_2:
        case GARINS_GRAVE_3:
        case GARINS_GRAVE_4:
            return 1;
        case MOUNTAIN_CAVE:
        case MOUNTAIN_CAVE_2:
            return 2;
        case CHARLOCK:
        case CHARLOCK_CAVE_1:
        case CHARLOCK_CAVE_2:
        case CHARLOCK_CAVE_3:
        case CHARLOCK_CAVE_4:
        case CHARLOCK_CAVE_5:
        case CHARLOCK_CAVE_6:
        case CHARLOCK_THRONE_ROOM:
            return 3;
        default:
            return -1;
    }
}

/** Checks to see if a chest already exists at the current x/y coordinates.
 * This will avoid conflicts since 2 chests cannot exist at the same
 * coordinates where the player can travel to both locations without crossing
 * the overwold. With stair shuffle coming, lets just avoid all conflicts.
 *
 * @param rom The rom struct
 * @param x The x coordinate
 * @param y The y coordinate
 * @param until The number of chests to check from the beginning.
 *
 * @return A pointer to the conflicting chest.
 */
static dw_chest *chest_at(dw_rom *rom, uint8_t x, uint8_t y, size_t until) {
    dw_chest *chest;
    dw_chest *stop = rom->chests + until;
    for (chest = rom->chests; chest < stop; chest++) {
        if(chest->x == x && chest->y == y) {
            return chest;
        }
    }
    return NULL;
}

/**
 * Randomizes the placment of chests to a subset of predetermined locations
 *
 * @param rom The rom struct
 */
static void move_chests(dw_rom *rom)
{
    size_t i, j, k;
    int bin, map;
    uint8_t *chest_spot, *new_door;
    uint8_t tantegel_spot[3];
    uint8_t chest_bins[4] = { 3, 0, 0, 0};
    uint8_t chest_spots[][3] = {
//         {TANTEGEL_THRONE_ROOM,  4,  4}, /* vanilla */
//         {TANTEGEL_THRONE_ROOM,  5,  4}, /* vanilla */
//         {TANTEGEL_THRONE_ROOM,  6,  1}, /* vanilla */
        {TANTEGEL,              1, 13}, /* vanilla */
        {TANTEGEL,              2, 13},
        {TANTEGEL,              3, 13}, /* vanilla */
        {TANTEGEL,              1, 14},
        {TANTEGEL,              2, 14}, /* vanilla */
        {TANTEGEL,              3, 14},
        {TANTEGEL,              1, 15}, /* vanilla */
        {TANTEGEL,              2, 15},
        {TANTEGEL,              3, 15}, /* vanilla */
        {TANTEGEL,             16, 15},
        {TANTEGEL,             24, 21},
        {TANTEGEL_BASEMENT,     4,  5}, /* vanilla */
        {TANTEGEL_BASEMENT,     4,  8},
        {TANTEGEL_BASEMENT,     5,  8},
        {BRECCONARY,            3, 21},
        {BRECCONARY,           10, 23},
//         {BRECCONARY,           21,  3},
        {ERDRICKS_CAVE,         9,  0},
        {ERDRICKS_CAVE_2,       1,  9},
        {ERDRICKS_CAVE_2,       9,  3}, /* vanilla */
        {GARINHAM,              4,  5},
        {GARINHAM,              8,  5}, /* vanilla */
        {GARINHAM,              8,  6}, /* vanilla */
        {GARINHAM,              9,  5}, /* vanilla */
        {GARINHAM,             18, 18},
        {GARINS_GRAVE_1,       11,  0}, /* vanilla */
        {GARINS_GRAVE_1,       12,  0}, /* vanilla */
        {GARINS_GRAVE_1,       13,  0}, /* vanilla */
        {GARINS_GRAVE_1,        0, 14},
        {GARINS_GRAVE_1,        7,  5},
        {GARINS_GRAVE_1,        4, 15},
        {GARINS_GRAVE_3,        1,  1}, /* vanilla */
        {GARINS_GRAVE_3,       13,  6}, /* vanilla */
        {GARINS_GRAVE_3,       13, 10},
        {GARINS_GRAVE_3,       17, 17},
        {KOL,                  22,  1},
        {KOL,                   3, 13},
        {SWAMP_CAVE,            5,  0},
        {SWAMP_CAVE,            4, 19},
        {SWAMP_CAVE,            5, 29},
        {RIMULDAR,             24, 23}, /* vanilla */
        {RIMULDAR,             17, 23},
        {RIMULDAR,              3, 20},
        {MOUNTAIN_CAVE,        13,  5}, /* vanilla */
        {MOUNTAIN_CAVE,        13,  9},
        {MOUNTAIN_CAVE_2,       1,  1},
        {MOUNTAIN_CAVE_2,      13,  9},
        {MOUNTAIN_CAVE_2,       0,  3},
        {MOUNTAIN_CAVE_2,       1,  6}, /* vanilla */
        {MOUNTAIN_CAVE_2,       2, 11},
        {MOUNTAIN_CAVE_2,       2,  2}, /* vanilla */
        {MOUNTAIN_CAVE_2,       3,  2}, /* vanilla */
        {MOUNTAIN_CAVE_2,      10,  9}, /* vanilla */
        {HAUKSNESS,             6,  9},
        {CANTLIN,               2, 27},
        {CANTLIN,              12,  2},
        {CHARLOCK_CAVE_1,      19,  9},
        {CHARLOCK_CAVE_1,       2, 17},
        {CHARLOCK_CAVE_1,       8,  6},
        {CHARLOCK_CAVE_1,      11,  6},
        {CHARLOCK_CAVE_1,      19,  6},
        {CHARLOCK_CAVE_2,       5,  5}, /* vanilla */
        {CHARLOCK_CAVE_3,       3,  8},
        {CHARLOCK_CAVE_4,       6,  4},
        {CHARLOCK_CAVE_5,       2,  0},
        {CHARLOCK_CAVE_6,       9,  0},
        {CHARLOCK_THRONE_ROOM,  5,  2},
        {CHARLOCK_THRONE_ROOM,  6, 12},
        {CHARLOCK_THRONE_ROOM,  8, 12},
        {CHARLOCK_THRONE_ROOM, 11, 11}, /* vanilla */
        {CHARLOCK_THRONE_ROOM, 11, 12}, /* vanilla */
        {CHARLOCK_THRONE_ROOM, 11, 13}, /* vanilla */
        {CHARLOCK_THRONE_ROOM, 12, 11},
        {CHARLOCK_THRONE_ROOM, 12, 12}, /* vanilla */
        {CHARLOCK_THRONE_ROOM, 12, 13}, /* vanilla */
        {CHARLOCK_THRONE_ROOM, 13, 11},
        {CHARLOCK_THRONE_ROOM, 13, 12},
        {CHARLOCK_THRONE_ROOM, 13, 13}, /* vanilla */
        {CHARLOCK_THRONE_ROOM, 22, 26},
    };

    uint8_t new_doors[][3] = {
        {BRECCONARY,            8, 23},
        {GARINHAM,             16, 18},
        {KOL,                  20,  2},
        {RIMULDAR,             17, 21},
        {HAUKSNESS,            10,  8},
        {CANTLIN,               3, 24},
        {CANTLIN,              10,  4},
        {CHARLOCK_THRONE_ROOM,  4,  9},
        {CHARLOCK_THRONE_ROOM,  7, 22},
    };

    if (!RANDOM_CHEST_LOCATIONS(rom)) {
        return;
    }

    printf("Removing all chests...\n");
    for (i=0; i < sizeof(chest_spots) / 3; i++) {
        chest_spot = chest_spots[i];
        if (is_dungeon_tileset(chest_spot[0])) {
            set_dungeon_tile(rom, chest_spot[0], chest_spot[1], chest_spot[2],
                    DUNGEON_TILE_BRICK);
        } else {
            set_dungeon_tile(rom, chest_spot[0], chest_spot[1], chest_spot[2],
                    TOWN_TILE_BRICK);
        }
    }
    printf("Adding doors...\n");
    for (i=0; i < sizeof(new_doors) / 3; i++) {
        new_door = new_doors[i];
        if (is_dungeon_tileset(new_door[0])) {
            set_dungeon_tile(rom, new_door[0], new_door[1], new_door[2],
                    DUNGEON_TILE_DOOR);
        } else {
            set_dungeon_tile(rom, new_door[0], new_door[1], new_door[2],
                    TOWN_TILE_DOOR);
        }
    }
    printf("Adding back chests...\n");
chest_placement_retry:
    mt_shuffle(chest_spots, sizeof(chest_spots) / 3, 3);
    /* Guaranteed Gold needs a chest that is still in Tantegel castle once the
     * throne room's have been taken at the start of the game, and a shuffle can
     * otherwise leave the castle with none at all. Move a Tantegel spot to the
     * front so the first chest placed lands there. Swapping two entries of an
     * already shuffled list consumes no RNG, so the rest of the placement is
     * exactly what the seed would have produced anyway. */
    if (GUARANTEED_GOLD(rom)) {
        for (k=0; k < sizeof(chest_spots) / 3; k++) {
            if (chest_spots[k][0] != TANTEGEL)
                continue;
            /* The spot also has to survive the loop's own occupancy test, which
             * compares coordinates without regard to the map (chest_at). Five
             * of the Tantegel spots are vanilla chest coordinates, so a spot
             * reserved blindly is skipped for the same reason a taken one is,
             * and the reservation quietly does nothing. */
            if (chest_at(rom, chest_spots[k][1], chest_spots[k][2], 3))
                continue;
            memcpy(tantegel_spot, chest_spots[0], 3);
            memcpy(chest_spots[0], chest_spots[k], 3);
            memcpy(chest_spots[k], tantegel_spot, 3);
            break;
        }
    }
    for (i=0, j=0; i < CHEST_COUNT; i++, j++) {
        map = rom->chests[i].map;
         /* don't mess with these */
        if (map == TANTEGEL_THRONE_ROOM || map == NORTHERN_SHRINE)
            continue;
        while (chest_at(rom, chest_spots[j][1], chest_spots[j][2], i+3)) {
            j++;
        }
        bin = chest_bin(chest_spots[j][0]);
        while (bin >= 0 && chest_bins[bin] > 8) {
            bin = chest_bin(chest_spots[++j][0]);
        }
        if (j >= sizeof(chest_spots) / 3) {
            /* this should never happen, but just in case */
            printf("Something went wrong, retrying chest placement...\n");
            chest_bins[0] = 3;
            chest_bins[1] = chest_bins[2] = chest_bins[3] = 0;
            goto chest_placement_retry;
        }
        if (bin >= 0) {
            chest_bins[bin]++;
        }
        rom->chests[i].map = chest_spots[j][0];
        rom->chests[i].x = chest_spots[j][1];
        rom->chests[i].y = chest_spots[j][2];
        if (is_dungeon_tileset(rom->chests[i].map)) {
            set_dungeon_tile(rom, rom->chests[i].map, rom->chests[i].x,
                    rom->chests[i].y, DUNGEON_TILE_CHEST);
        } else {
            set_dungeon_tile(rom, rom->chests[i].map, rom->chests[i].x,
                    rom->chests[i].y, TOWN_TILE_CHEST);
        }
    }
}

/**
 * Shuffles the contents of all chests in the game with the exception of the
 * starting throne room key and the staff of rain.
 *
 * @param rom The rom struct
 */
static void shuffle_chests(dw_rom *rom)
{
    size_t i;
    dw_chest *chest;
    uint8_t contents[] = {
            GOLD, GOLD, GOLD, GOLD,
            CURSED_BELT, CURSED_BELT, CURSED_BELT,
            GOLD, GOLD, GOLD, GOLD, GOLD, GOLD,
            WINGS, WINGS,
            KEY, KEY, KEY,
            HERB, HERB, HERB, HERB,
            TORCH, TORCH, TORCH,
            FAIRY_WATER, FAIRY_WATER,
            DRAGON_SCALE, RING
    }, *cont = contents;
    uint8_t  key_items[] = {
            SWORD, ARMOR, HARP, STONES, FLUTE, TOKEN, NECKLACE
    }, *key_item = key_items;
    uint8_t search_items[] = { 0, 0, 0 };

    if (!SHUFFLE_CHESTS(rom))
        return no_chest_shuffle(rom);

     /* if the necklace doesn't have special effects, it's not a key item */
    if (DEATH_NECKLACE(rom)) {
        mt_shuffle(key_items, sizeof(key_items), sizeof(uint8_t));
    } else {
        mt_shuffle(key_items, sizeof(key_items)-1, sizeof(uint8_t));
    }

    for (i=0; i < 3; i++) {
        /* If "No Numbers" is turned on, don't put anything on the overworld */
        if (NO_NUMBERS(rom) && !i) {
            /* If curse princess is on, add an extra cursed belt to make sure 
             * there are three */
            if (CURSED_PRINCESS(rom)) {
                contents[7] = CURSED_BELT;
            }
            continue;
        }

        /* fill in the search spots with a 80% chance unless cursed princess */
        /* is on, then always place to maximize cursed belts. */
        if (mt_rand(0, 4) || CURSED_PRINCESS(rom)) {
            /* If we're putting something on the overworld and cursed princess
             * is on, make sure it's not vital */
            while (!i && THREES_COMPANY(rom) && is_quest_item(key_items[0])) {
                mt_shuffle(key_items, sizeof(key_items), sizeof(uint8_t));
            }
            search_items[i] = *(key_item++);
        }
    }
    rewrite_search_take_code(rom, search_items);

    while (key_item < (key_items + sizeof(key_items))) {
        /* overwite cursed belts with the remaining key items */
        *cont = *key_item++; 
        cont++;
    }

    printf("Shuffling chest contents...\n");
    cont = contents;
    chest = rom->chests;

    /* shuffle the contents and place them in chests */
    mt_shuffle(contents, sizeof(contents), sizeof(uint8_t));
    chest = rom->chests;
    cont = contents;
    for (i=0; i < CHEST_COUNT; i++) {
        /* don't move the staff or starting key */
        if (is_static_chest(chest)) {
            chest++;
            continue;
        }
        (chest++)->item = *(cont++);
    }

}

/**
 * Applies all chest-related flags
 *
 * @param rom The rom struct
 */
void do_chest_flags(dw_rom *rom) {
    move_chests(rom);
    shuffle_chests(rom);
}

