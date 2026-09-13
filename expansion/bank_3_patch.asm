
.org $c288

; $c288 held the dispatch table. It has moved to the end of this bank
; (exp_dispatch) because it is the one thing here that grows: every entry added
; to it pushes all 25 routines below it down, and those addresses are the hook
; targets the C side patches in. It is read by label now, so it can grow
; without moving anything.
;
; The 18 bytes it left behind are free for stubs that want to be reachable by a
; short branch from exec_expansion_sub. @stubs_end guards the boundary, because
; a backwards .org overwrites silently rather than failing.

@stubs_end:
.if @stubs_end > $c29a
    .error "the $c288 stubs have overrun the 18 bytes the dispatch table left"
.endif

.org $c29a              ; first byte past the old table. Everything from here
                        ; down sits at its original address, which is the point
                        ; of the whole arrangement - the .error above is what
                        ; keeps it true.

start_dwr_credits:
    ldy #0
    beq exec_expansion_sub

inc_attack_ctr:
    brk
    hex 04 17
    lda #2
    tay
    jsr exec_expansion_sub
    rts

inc_crit_ctr:
    brk
    hex 04 17
    lda #4
    ldy #2
    bne exec_expansion_sub

inc_miss_ctr:
    brk
    hex 04 17
    lda #6
    ldy #2
    bne exec_expansion_sub

inc_dodge_ctr:
    brk
    hex 04 17
    lda #8
    ldy #2
    bne exec_expansion_sub

inc_bonk_ctr:
    brk
    hex 04 17
    lda #10
    ldy #2
    bne exec_expansion_sub

inc_death_ctr:
    brk
    hex 04 17
    lda #0
    ldy #2
    bne exec_expansion_sub

blocked_in_front:
    jsr do_dialog_lo  ; but was blocked in front
    hex f6
    ldy #2
    lda #12
    bne exec_expansion_sub

player_ambushed:
    jsr do_dialog_lo  ; attacked before ready
    hex e4
    lda #14
    ldy #2
;     bne exec_expansion_sub

exec_expansion_sub:
    pha
;     lda #$4c
;     sta $7f00
    lda exp_dispatch,y
    sta $7f01
    lda exp_dispatch+1,y
    sta $7f02
    lda $6004
    sta $7004
    lda #3
    jsr store_and_load_prg_bank
    pla
    jmp ($7f01)

inc_enemy_death_ctr:
    brk
    hex 04 17
    ldy #4
    bne exec_expansion_sub

count_encounter:
    brk
    hex 04 17
    ldy #8
    jsr exec_expansion_sub
    jmp exp_load_last_bank

count_win:
    ldy #6
    jsr exec_expansion_sub
    jmp exp_load_last_bank

count_spell_use:
    ldy #12
    jsr exec_expansion_sub
    jmp $c6f0   ; show window

sort_inventory:
    pha         ; save the item id. b3_e01b shifts $3e left 4 bits when the
                ; item lands in the high nibble of a slot, so the id cannot
                ; be recovered from $3e after the call.
    jsr b3_e01b ; add the new item
    cpx #$04
    bne +
    pla         ; inventory was full and the item was discarded
    rts
+   pla         ; item id back in A
    jsr record_item_time ; item discovery timestamps
    ldy #16
    bne exec_expansion_sub

torch_in_battle:
;     ldy #18
;     bne exec_expansion_sub
    cmp #$04            ; is this a torch?
    bne +++             ; no, go to next check
    lda #$01
    jsr $e04b           ; remove the item from inventory
    jsr $c7c5           ; show the "hurled a torch" message
    hex 29
    lda $e0
    cmp #$10            ; are we fighting a metal slime?
    bne ++              ; no, go to higher damage
-   jsr $c55b           ; run the rng cycle routine
    lda $95             ; load a random number
    and #$01            ; limit to 0-1
    bne +               ; did we hit it?
    jmp $e658           ; A miss!
+   jmp $e694           ; yes, go to damage routine
++  jsr $c55b           ; run the rng cycle routine
    lda $95             ; load a random number
    and #$03            ; limit to 0-3
    clc
    adc #$06            ; add 6
    jmp $e694           ; jump to damage routine
+++ cmp #$05            ; c9 04      ; Fairy Water
    bne +
    lda #$02
    jsr $e04b           ; remove the item from inventory
    jsr $c7c5           ; show the "sprinked fairy water" message
    hex 2a
    lda $e0
    cmp #$10            ; metal slime
    beq - 
    jmp $e744
+   jmp $e6fd           ; 4c fd e6

threes_company_check:
    lda $df   ; Load the status bits
    and #$01  ; Are we carrying Gwaelin?
    beq +     ; No, back to original code
    jsr $c7cb ; Show dialogue
    .db $a1   ; I have some great ideas
    jmp $ccb8 ; Jump to the ending credits

+   jsr $c7cb ; Show dialoge
    .db $a1   ; Now take a long rest
    rts

store_princess_item:
    sta $0e    ; Store the item Gwaelin took
    jmp $e04b  ; Continue on
cursed_princess_check:
    pha        ; Store register
    lda $0e    ; Load the item Gwaelin took
    cmp #$09   ; It is a cursed belt?
    bne +      ; No, keep playing
    jsr $c7cb  ; Yes, Display dialogue
    .db $a0    ; What a lovely corset
    jsr $dffa  ; Play the curse music
    jmp $ccb8  ; Jump to the ending
+   pla        ; Restore register and continue
    jmp $d433

scared_slimes:
    cmp $0100 ; Replace the code from $efba
    bcs +     ; Return, everything is fine
    lda $e0   ; Load the enemy type
    cmp #$10  ; Is it a metal slime?
    beq +
    clc       ; It's not, enemy doesn't run
+   rts

init_save_ram:
    sta $62e8,x
    sta $662c,x
    sta $672c,x
    rts

display_deaths:
    lda $64df
    bne +
    jmp $a8ba
+   lda #05
    sta $64e2
    lda $6630
    sta $1a
    lda $6631
    sta $1b
    lda #00
    sta $1c
    jsr $a753
    jmp $a764

count_frame:
    ldx MAP_INDEX  ; Start the timer once the map is no longer zero
    bne +
    lda $662c      ; continue counting if the timer is greater than zero.
    ora $662d
    ora $662e
    beq ++
+   ldx $662c      ; increment the timer
    inx
    stx $662c
    bne ++
    ldx $662d
    inx
    stx $662d
    bne ++
    inc $662e
++  jmp $fdf4

snapshot_timer:        ; freezes the final stats for the ending screen
    ldx #2
-   lda $662c,x
    sta $6f00,x
    dex
    bpl -
    jmp capture_win_level ; was "jmp do_dialog_hi", which that tail-calls in
                        ; turn. Appended rather than inlined here: this routine
                        ; sits above modify_run_rate, so growing it would move
                        ; modify_run_rate down the bank.

modify_run_rate:
    and #$7f       ; limit the value in reg A to 0-127
    sta $3e        ; save to $3E
    jsr cycle_rng  ; generate a new random number
    lda RANDOM_H   ; load the random number
    and #$3f       ; limit to 0-63
    adc $3e        ; add to the previous number
    jmp $eec7

; *** Item discovery timestamps ***
; Stamps the 24-bit frame counter at $662c - the one the ending's Play Time
; comes from - the first time each tracked item is acquired. Write-once, so
; trade-ins, discards and equipment swaps never disturb a recorded time.
;
; Storage is 10 slots of 3 bytes at ITEM_TIMES ($6760, in bank_0.asm), inside
; the range init_save_ram clears, so 0 safely means "never acquired" - the
; timer only runs once the player is on a map, so nothing lands on frame 0.
;
; The monster tallies below reach $675f, leaving exactly one byte of slack:
; another monster or stat column would overrun into slot 0.

record_item_time:      ; A = item id, captured by the caller BEFORE the add
                        ; (see sort_inventory); called only after a
                        ; successful inventory add
    and #$0f            ; item ids are 4 bits; keeps the index inside the
                        ; 16-byte table no matter what the caller passed
    tax
    lda item_slot_table,x
    cmp #$ff
    beq +
    jsr record_item_time_for_slot
+   rts

record_item_time_for_slot: ; A = slot index (0..ITEM_ROW_COUNT-1).
                            ; Writes only if the slot is still zero.
    sta $3c
    asl a
    clc
    adc $3c
    tax                 ; X = byte offset into ITEM_TIMES (slot*3)
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

grant_erdricks_sword:  ; replaces the original 6-byte equip-write at $e2a6
    lda $be
    ora #$e0
    sta $be
    lda #$00            ; slot 0 = Erdrick's Sword
    jsr record_item_time_for_slot
    rts

grant_erdricks_armor:  ; replaces the original 6-byte equip-write at $e2f6
    lda $be
    ora #$1c
    sta $be
    lda #$01            ; slot 1 = Erdrick's Armor
    jsr record_item_time_for_slot
    rts

; *** Step counter ***
; Replaces the "lda $16 / cmp #$20" pair that begins each of the four
; directional movement handlers (Right/Left/Down/Up Synced). The four call
; sites are patched to "jsr step_counter" + nop, so this runs once per step,
; then performs the original two instructions and returns. JSR/RTS do not
; disturb flags, so the CMP result reaches the caller intact.
step_counter:
    inc STEP_COUNT
    bne +
    inc STEP_COUNT+1
+   lda $16             ; original code from the movement handler
    cmp #$20
    rts

item_slot_table:        ; item id ($0-$f) -> ITEM_TIMES slot, $ff = untracked.
                        ; Ids are from notes/items.txt. Indexed after an
                        ; AND #$0f so it can never be read past its end.
                        ; Slots 0 and 1 are absent here on purpose: the two
                        ; Erdrick equipment pieces have no item id.
    .db $ff, $ff, $ff, $ff, $ff, $06, $09, $04
    .db $ff, $ff, $02, $05, $03, $07, $08, $ff

; *** Spell learn timestamps ***
; Records when each spell was learned, using the same 24-bit frame counter at
; $662c as the item timestamps. Storage is 10 slots of 3 bytes at SPELL_TIMES,
; inside the range init_save_ram clears, so 0 means "no timestamp".
;
; The hook replaces the "jsr $dbf0" in the level-up display loop that
; show_spells_learned() writes at $eae6. That loop already walks newly learned
; spells one at a time and is guarded against ones the player had, and a 3-byte
; JSR for a 3-byte JSR shifts nothing.
;
; A spell learned at level 1 is granted at new-game init without a level-up, so
; its slot stays zero. spell_level_table tells that apart from "not learned".

record_spell_time:     ; A = spell index + 16, exactly as b3_dbf0 expects it
    pha                ; b3_dbf0 needs A intact - it is the name table index
    sec
    sbc #16            ; A = spell index 0..9, in dw_spells order
    sta $3c            ; b3_dbf0 clobbers $3c itself, so it is free scratch here
    asl a
    clc
    adc $3c
    tax                ; X = byte offset into SPELL_TIMES (index * 3)
    lda SPELL_TIMES,x
    ora SPELL_TIMES+1,x
    ora SPELL_TIMES+2,x
    bne +              ; already recorded - keep the first time, not the last
    lda SPELL_STAMP    ; the level-up's time, not the time this message showed
    sta SPELL_TIMES,x
    lda SPELL_STAMP+1
    sta SPELL_TIMES+1,x
    lda SPELL_STAMP+2
    sta SPELL_TIMES+2,x
+   pla
    jmp b3_dbf0        ; tail-call the routine whose JSR we replaced

snapshot_spell_time:   ; Captures the frame counter at the instant of a level-up.
                       ; save_spells (written by show_spells_learned at $bfe0)
                       ; stashes the old spell mask and then jumps to $f050 to
                       ; apply the new level's stats; that jump is retargeted
                       ; here, so this runs once per level-up, before any spell
                       ; message is shown. Ends by continuing to $f050.
    lda $662c
    sta SPELL_STAMP
    lda $662d
    sta SPELL_STAMP+1
    lda $662e
    sta SPELL_STAMP+2
    jmp $f050          ; carry on with the level-up the hook interrupted

; *** Ending stats, tail ends ***

capture_win_level:      ; tail of snapshot_timer. $c7 is the level the game was
                        ; won at, frozen at the same moment as the play time.
                        ; Not read when the stats screen draws: by the time the
                        ; credits start $c7 has been clobbered and reads back as
                        ; $ff. The high byte is cleared so INSTR_SHOW_NUMBER
                        ; reads a clean 16-bit value.
    lda $c7
    sta WIN_LEVEL
    lda #0
    sta WIN_LEVEL+1
    jmp do_dialog_hi

; *** Expansion dispatch table ***
; Indexed by the Y that each stub loads, and read by exec_expansion_sub above.
; It lives here, at the end, rather than at $c288 where it started: it is the
; only part of this bank that grows, and down here it can.

exp_dispatch:
.word exp_start_dwr_credits    ;  0
.word exp_inc_counter          ;  2
.word exp_inc_enemy_death_ctr  ;  4
.word exp_count_win            ;  6
.word exp_count_encounter      ;  8
.word exp_inc_mon_counter      ; 10
.word exp_count_spell_use      ; 12
.word exp_load_last_bank       ; 14
.word exp_sort_inventory       ; 16
; .word exp_torch_in_battle      ; 18

.org $c4f5

.org $c4f5
    ; potentially unused data
.org $c529

.org $c6c9
    ; potentially unused data
.org $c6f0

.org $c7ec
    ; potentially unused data
.org $c9b5

.org $f232
    ; potentially unused data
.org $f35b

