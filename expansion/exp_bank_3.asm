
.base $8000

exp_start_dwr_credits:
    lda #$a9        ; lda
    sta $200
    lda #%00000010  ; Change banking mode
    sta $201
    ldx #$12
-   lda b3_fe09+3, x
    sta $202,x
    dex
    bpl -

    lda #$a9
    sta $215
    lda #5         ; load bank 5
    sta $216
    ldx #$15
-   lda b3_ff96, x
    sta $217,x
    dex
    bpl -
    lda #>(credit_start-1)
    pha
    lda #<(credit_start-1)
    pha
    jmp $200

exp_inc_enemy_death_ctr:
    lda CURRENT_TILE
    asl
    clc
    adc #160
    jsr exp_inc_mon_counter
    lda #0
    beq exp_inc_counter

exp_count_win:
    lda CURRENT_TILE
    asl
    clc
    adc #80
    bne exp_inc_mon_counter
exp_count_encounter:
    lda CURRENT_TILE
    asl
exp_inc_mon_counter:
    tax
    ldy $6670,x
    iny
    bne +
    inc $6671,x
+   tya
    sta $6670,x
    rts

exp_count_spell_use:
    lda SELECTED_ITEM
    asl
    clc
    adc #$2c
exp_inc_counter:
    tax
    ldy $6630,x
    iny
    bne +
    inc $6631,x
+   tya
    sta $6630,x
exp_load_last_bank:
    lda $7004
    jmp store_and_load_prg_bank

exp_sort_inventory:
    ldx #$03
-   lda ITEMS,x
    and #$0f
    sta $a8,x
    lda ITEMS,x
    lsr a
    lsr a
    lsr a
    lsr a
    sta $ac,x
    lda #$00
    sta ITEMS,x
    dex
    bpl -
    lda #$00
--  ldx #$07
-   cmp $a8,x
    bcs +
    lda $a8,x
    stx $a7
+   dex
    bpl -
    cmp #$00
    beq +
    jsr b3_e01b ; add the next item
    ldx $a7
    lda #$00
    sta $a8,x
    beq --
+   jmp exp_load_last_bank

; *** Progressive Equipment (A8) ***
; In:  X = shop item index * 2. Weapons are items 0-6, armor 7-13, shields
;      14-16 (Erdrick's Sword and Armor are 6 and 13).
; Out: Y = worn tier - required tier, signed; X is kept.
; The equipment byte $be holds each slot as worn item + 1 within its category
; (0 = nothing), so the tier just below item i is a worn value of i minus the
; category's first index. The rule is "at least the tier below": downgrades and
; buying the same piece again are allowed.
A8_ITEM EQU $7f03          ; scratch, next to the $7f01-02 that
                           ; exec_expansion_sub uses
exp_progressive_check:
    txa
    lsr a
    sta A8_ITEM
    lda $be
    cpx #7*2
    bcc @weapon
    cpx #14*2
    bcc @armor
    and #$03               ; shield, worn value + 14
    clc
    adc #14
    bne @diff              ; always taken
@armor:
    lsr a
    lsr a
    and #$07               ; armor, worn value + 7
    clc
    adc #7
    bne @diff              ; always taken
@weapon:
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a                  ; weapon, worn value + 0
@diff:
    sec
    sbc A8_ITEM
    tay
    jmp exp_load_last_bank

; exp_torch_in_battle:
;     cmp #$04            ; is this a torch?
;     bne +++             ; no, go to next check
;     lda #$01
;     jsr $e04b           ; remove the item from inventory
;     jsr $c7c5           ; show the "hurled a torch" message
;     hex 29
;     lda $e0
;     cmp #$10            ; are we fighting a metal slime?
;     bne ++              ; no, go to higher damage
; -   jsr $c55b           ; run the rng cycle routine
;     lda $95             ; load a random number
;     and #$01            ; limit to 0-1
;     bne +               ; did we hit it?
;     jmp $e658           ; A miss!
; +   jmp $e694           ; yes, go to damage routine
; ++  jsr $c55b           ; run the rng cycle routine
;     lda $95             ; load a random number
;     and #$03            ; limit to 0-3
;     clc
;     adc #$06            ; add 6
;     jmp $e694           ; jump to damage routine
; +++ cmp #$05            ; c9 04      ; Fairy Water
;     bne +
;     lda #$02
;     jsr $e04b           ; remove the item from inventory
;     jsr $c7c5           ; show the "sprinked fairy water" message
;     hex 2a
;     lda $e0
;     cmp #$10            ; metal slime
;     beq - 
;     jmp $e744
; +   jmp $e6fd           ; 4c fd e6

; *** Item discovery timestamps ***
; Stamps the 24-bit frame counter at $662c - the one the ending's Play Time
; comes from - the first time each tracked item is acquired. Write-once, so
; trade-ins, discards and equipment swaps never disturb a recorded time.
;
; Storage is 10 slots of 3 bytes at ITEM_TIMES ($6760, in bank_0.asm), inside
; the range init_save_ram clears, so 0 safely means "never acquired" - the
; timer only runs once the player is on a map, so nothing lands on frame 0.
;
; The monster tallies below ITEM_TIMES reach $675f, leaving exactly one byte of
; slack: another monster or stat column would overrun into slot 0.

exp_record_and_sort:    ; dispatch entry 16. sort_inventory only reaches this
                        ; after a successful add, so there is no "did it fit"
                        ; check here. Tail-calls the sort the entry used to
                        ; name, which restores the caller's bank on its way out.
    lda ITEM_ADD_ID
    jsr exp_record_item_time
    jmp exp_sort_inventory

exp_record_item_time:   ; A = item id
    and #$0f            ; item ids are 4 bits; keeps the index inside the
                        ; 16-byte table no matter what the caller passed
    tax
    lda exp_item_slot_table,x
    cmp #$ff
    beq +
    jsr exp_record_item_time_for_slot
+   rts

exp_record_item_time_for_slot: ; A = slot index (0..ITEM_ROW_COUNT-1).
                        ; Writes only if the slot is still zero.
    sta $3c
    asl a
    clc
    adc $3c
    tax                 ; X = byte offset into ITEM_TIMES (slot*3)
exp_record_time_at_x:   ; X = byte offset from ITEM_TIMES. The Magic Key uses
                        ; this entry with KEY_TIME's offset, which lies past
                        ; the ten slots.
    lda ITEM_TIMES,x
    ora ITEM_TIMES+1,x
    ora ITEM_TIMES+2,x
    bne +               ; already recorded, leave it alone
    lda $662c
    sta ITEM_TIMES,x
    lda $662d
    sta ITEM_TIMES+1,x
    lda $662e
    sta ITEM_TIMES+2,x
+   rts

exp_item_slot_table:    ; item id ($0-$f) -> ITEM_TIMES slot, $ff = untracked.
                        ; Ids are from notes/items.txt. Indexed after an
                        ; AND #$0f so it can never be read past its end.
                        ; Slots 0 and 1 are absent here on purpose: the two
                        ; Erdrick equipment pieces have no item id.
    .db $ff, $ff, $ff, $ff, $ff, $06, $09, $04
    .db $ff, $ff, $02, $05, $03, $07, $08, $ff

; *** Magic Key timestamp ***
; One write-once time at KEY_TIME. The throne room's first chest always holds a
; key, so "the first key acquired" would read zero in every seed. Instead:
;   - holding a key at the moment the player first leaves the throne room ->
;     that moment (exp_throne_exit);
;   - otherwise the first key gained after leaving, from a chest, a search spot
;     or a key shop (exp_key_gained). Leaving by the stairs spends a key on the
;     throne room door, so with the one starting key this is the usual case.
; Keys gained inside the throne room before leaving are covered by the first
; rule. The stats page shows this row instead of Fighter's Ring unless No Keys
; is on; it is recorded either way.

KEY_TIME_OFFSET EQU KEY_TIME - ITEM_TIMES

exp_throne_exit:        ; dispatch entry 26, from the map-load hook at $af1f.
                        ; Does what the 12 bytes it replaced did - set $df bit
                        ; 3 when the new map is not the throne room (5) - and
                        ; checks for a key on the one load that first sets it.
    txa                 ; X is kept for the map-load routine (the stub keeps
    pha                 ; Y, which exec_expansion_sub's index overwrites)
    lda $45             ; map being loaded
    cmp #$05
    beq @done           ; the throne room itself: start, continue or death
    lda $df
    and #$08
    bne @done           ; has left before - this is not the first exit
    lda $df
    ora #$08
    sta $df
    lda $bf
    beq @done           ; no key in hand: the first key gained will count
    ldx #KEY_TIME_OFFSET
    jsr exp_record_time_at_x
@done:
    pla
    tax
    jmp exp_load_last_bank

exp_key_gained:         ; dispatch entry 28. A key was just added ($bf already
                        ; incremented) by a chest or search spot (key_from_chest)
                        ; or a key shop (key_from_shop).
    txa                 ; X is kept for the caller; Y is kept by the stubs,
    pha                 ; since exec_expansion_sub's index overwrites it
    lda $df
    and #$08
    beq +               ; still in the throne room: the exit decides instead
    ldx #KEY_TIME_OFFSET
    jsr exp_record_time_at_x   ; write-once, so only the first key counts
+   pla
    tax
    jmp exp_load_last_bank

exp_grant_erdricks_sword:  ; dispatch entry 18. Erdrick's Sword and Armor are
                           ; written straight into the packed equipment byte $be
                           ; and never reach an inventory slot, so they miss the
                           ; sort_inventory path entirely.
    lda $be
    ora #$e0
    sta $be
    lda #$00            ; slot 0 = Erdrick's Sword
    jsr exp_record_item_time_for_slot
    jmp exp_load_last_bank

exp_grant_erdricks_armor:  ; dispatch entry 20
    lda $be
    ora #$1c
    sta $be
    lda #$01            ; slot 1 = Erdrick's Armor
    jsr exp_record_item_time_for_slot
    jmp exp_load_last_bank

; *** Spell learn timestamps ***
; Records when each spell was learned, from the frame counter captured at the
; level-up itself rather than the moment its message is drawn - several spells
; can arrive from one level-up and they should all share that instant.
;
; Storage is 10 slots of 3 bytes at SPELL_TIMES, inside the range
; init_save_ram clears, so 0 means "not learned yet".

exp_record_spell_time:  ; dispatch entry 22. A = spell index + 16.
                        ; A is not handed back - the caller's stub keeps its own
                        ; copy, because the bank restore clobbers the register.
    sec
    sbc #16             ; A = spell index 0..9, in dw_spells order
    sta $3c             ; b3_dbf0 clobbers $3c itself, so it is free scratch here
    asl a
    clc
    adc $3c
    tax                 ; X = byte offset into SPELL_TIMES (index * 3)
    lda SPELL_TIMES,x
    ora SPELL_TIMES+1,x
    ora SPELL_TIMES+2,x
    bne +               ; already recorded - keep the first time, not the last
    lda SPELL_STAMP     ; the level-up's time, not the time this message showed
    sta SPELL_TIMES,x
    lda SPELL_STAMP+1
    sta SPELL_TIMES+1,x
    lda SPELL_STAMP+2
    sta SPELL_TIMES+2,x
+   jmp exp_load_last_bank

exp_snapshot_spell_time: ; dispatch entry 24. Captures the frame counter at the
                        ; instant of a level-up, before any spell message shows.
    lda $662c
    sta SPELL_STAMP
    lda $662d
    sta SPELL_STAMP+1
    lda $662e
    sta SPELL_STAMP+2
    jmp exp_load_last_bank


; *** Oops All Treasure Guards ***
; Treasure Guards keeps its guards in the forced-encounter table at $cd7a,
; which spike_rewrite() sized at 8 entries - 3 of them vanilla - and records a
; beaten guard as one of the 5 spare bits of the quest progress byte $e4. That
; is room for 5 guards. This keeps a second table of A7_GUARDS guards at $bf00,
; filled in by oops_all_treasure_guards(), with one byte per guard at A7_BEATEN.
;
; The three routines spike_rewrite() installed - start, ran away, defeated -
; are replaced when the flag is on, by stubs that call these. Each checks the
; original table first with its original rules, so vanilla encounters behave
; exactly as before, then the guard table. Entries are numbered 0-7 for the original table
; and 8 and up for the guard table, so X alone says which one matched.
;
; Everything below is reached through a7_call in bank_3_patch.asm, from the
; stubs there (a7_spike_start, a7_spike_ran, a7_spike_won, a7_npc_freeze), and
; answers in X: the bank restore ends with "lda $7004", so A and the flags
; do not survive the trip back.

exp_a7_entry:          ; from a7_call: Y = 0 lookup, 1 won, 2 NPC settle
    cpy #1
    bcc exp_a7_lookup
    beq exp_a7_won
    jmp exp_a7_npc_settle

exp_a7_lookup:         ; Y = 0. X = the enemy to fight if the player
                       ; stands on an unbeaten guard or forced encounter, else $ff
    jsr exp_a7_match
    bcc @none
    cpx #$08
    bcs @guard
    lda $cd92,x        ; original table: beaten once its quest bit is set
    and $e4
    bne @none
    lda $cd9a,x
    tax
    jmp exp_load_last_bank
@guard:
    lda A7_BEATEN-8,x
    bne @none
    lda a7_guard_monster-8,x
    tax
    jmp exp_load_last_bank
@none:
    ldx #$ff
    jmp exp_load_last_bank

exp_a7_won:            ; Y = 1. After a won battle, marks the tile
                       ; the player is on as beaten
    jsr exp_a7_match
    bcc @done
    cpx #$08
    bcs @guard
    lda $cd92,x        ; original table: set its quest progress bit, as before
    ora $e4
    sta $e4
    bcc @done          ; carry is still clear from the cpx
@guard:
    lda #$01
    sta A7_BEATEN-8,x
@done:
    jmp exp_load_last_bank

exp_a7_match:          ; C set and X = entry if the player is standing on one
    ldx #$07
@spike:
    lda $45
    cmp $cd7a,x
    bne @spike_next
    lda $3a
    cmp $cd82,x
    bne @spike_next
    lda $3b
    cmp $cd8a,x
    beq @hit
@spike_next:
    dex
    bpl @spike
    ldx #A7_GUARDS+7
@guard:
    lda $45
    cmp a7_guard_map-8,x
    bne @guard_next
    lda $3a
    cmp a7_guard_x-8,x
    bne @guard_next
    lda $3b
    cmp a7_guard_y-8,x
    beq @hit
@guard_next:
    dex
    cpx #$08
    bcs @guard
    clc
    rts
@hit:
    sec
    rts

; Town NPCs desync after a battle: they are drawn in one place while the game
; thinks they are somewhere else. Vanilla never has a battle in a town with
; NPCs, so nothing guards against it.
;
; Each NPC is three bytes from $51: x (low 5 bits), then direction (bits 5-6),
; a "moving" flag (bit 7) and y (low 5 bits), then a pixel offset - x in the
; high nibble, y in the low. Drawn position is tile * 16 + offset. A step takes
; 16 frames, and moving up or left changes the coordinate at the start of the
; step while right or down changes it at the end, so offset and coordinate
; only agree if the step runs to completion in one direction.
;
; Steps stay aligned because the NPCs are timed off the frame counter $4f. NPCs
; are updated two at a time, the pair picked by $8d: a pair chooses a direction
; when the low nibble of $4f is 1, moves one pixel a frame, finishes on 0, and
; $8d moves on to the next pair. Nothing else resynchronises them, so an NPC
; left part-way through a step turns mid-step and never lines up again.
;
; start_combat freezes the NPCs by setting bit 7 of $8d, and the end of battle
; clears $8d to 0 without finishing anyone's step. Vanilla gets away with that
; because its battles never happen in a town. The freeze is the moment that
; matters: frames pass between finding a guard and the battle starting, and
; NPCs keep walking through them (6 frames in the case that was traced, enough
; for a pair to start a step), so settling when the guard is found is too early.
;
; a7_npc_freeze therefore replaces start_combat's own "set bit 7 of $8d" and
; finishes every NPC's step in place first: clear its offset, apply the
; coordinate change a right or down step makes at the end, and clear "moving".
; That is the state a map load leaves them in, and nothing moves them again
; until the battle is over. The visible cost is a jump of up to 15 pixels on
; the NPCs that were mid-step.

exp_a7_npc_settle:     ; Y = 2, reached from a7_npc_freeze
    ldx #$00
@npc:
    lda $52,x
    bpl @next          ; not moving
    and #$60
    beq @y_offset      ; up: y already changed at the start of the step
    cmp #$40
    beq @down
    cmp #$60
    beq @x_offset      ; left: x already changed at the start of the step
    lda $53,x          ; right: x changes when the offset wraps to 0
    and #$f0
    beq @x_offset
    inc $51,x
@x_offset:
    lda $53,x
    and #$0f
    sta $53,x
    jmp @stop
@down:
    lda $53,x          ; down: y changes when the offset wraps to 0
    and #$0f
    beq @y_offset
    inc $52,x
@y_offset:
    lda $53,x
    and #$f0
    sta $53,x
@stop:
    lda $52,x
    and #$7f
    sta $52,x
@next:
    inx
    inx
    inx
    cpx #$3c           ; 20 NPCs, as the map loader clears
    bne @npc
    jmp exp_load_last_bank

.org $bf00             ; pinned so oops_all_treasure_guards() can write the
                       ; table at expansion offset 0x3f00 (A7_TABLES in dwr.c).
                       ; asm6f rejects a backwards .org, so code growing into
                       ; it fails to assemble rather than overlapping.
a7_guard_map:          ; $ff = empty, which no map index matches
    .dsb A7_GUARDS, $ff
a7_guard_x:
    .dsb A7_GUARDS, $ff
a7_guard_y:
    .dsb A7_GUARDS, $ff
a7_guard_monster:
    .dsb A7_GUARDS, $ff

.org $c000
