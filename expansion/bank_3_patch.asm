
.org $c288

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
    lda $c288,y
    sta $7f01
    lda $c289,y
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
    jsr b3_e01b ; add the new item
    cpx #$04
    bne + 
    rts
+   ldy #16
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

snapshot_timer:
    ldx #2
-   lda $662c,x
    sta $6f00,x
    dex
    bpl -
    jmp do_dialog_hi

modify_run_rate:
    and #$7f       ; limit the value in reg A to 0-127
    sta $3e        ; save to $3E
    jsr cycle_rng  ; generate a new random number
    lda RANDOM_H   ; load the random number
    and #$3f       ; limit to 0-63
    adc $3e        ; add to the previous number
    jmp $eec7

.org $c4f5

.org $c4f5
    ; potentially unused data
.org $c529

.org $c6c9
    ; potentially unused data
.org $c6f0

.org $c7ec
    ; potentially unused data

.org $c818             ; clear of the "BEGIN A NEW QUEST" window, which dwr.c
                       ; writes over $c7ec-$c814

; *** Oops All Treasure Guards ***
; The guard table, both lookups and the NPC settle loop live in expansion
; bank 3 - see exp_a7_lookup there. These are the footholds in the bank that is
; always mapped in. Each one saves Y, which the code it replaces never touched,
; switches to expansion bank 3 through a7_call, and carries on where the
; original did. The expansion routines answer in X, because the bank restore
; ends with "lda $7004" and so destroys A and the flags.

a7_spike_start:        ; replaces the check at $cd51, run after each step
    tya
    pha
    ldy #0             ; exp_a7_lookup: X = the enemy to fight, or $ff
    jsr a7_call
    pla
    tay
    cpx #$ff
    beq @done
    txa
    jmp $e4df          ; start_combat, A = enemy
@done:
    jmp $cda2          ; where the original check continues

a7_npc_freeze:         ; replaces "lda $8d / ora #$80 / sta $8d" at $e576, in
                       ; start_combat. It runs for every battle while the flag
                       ; is on, so it only settles when the map has NPCs: the
                       ; map loader sets $8d to $ff when there are none. Why it
                       ; is needed at all is at exp_a7_npc_settle.
    lda $8d
    cmp #$ff
    beq @freeze
    txa                ; start_combat's caller may not expect X or Y to change
    pha
    tya
    pha
    ldy #2             ; exp_a7_npc_settle
    jsr a7_call
    pla
    tay
    pla
    tax
@freeze:
    lda $8d            ; the three instructions this replaced, so A and the
    ora #$80           ; flags come back exactly as start_combat left them
    sta $8d
    rts

a7_spike_ran:          ; replaces the check at $e8d4, after running from a battle
    tya
    pha
    ldy #0             ; exp_a7_lookup: $ff unless an unbeaten guard is here
    jsr a7_call
    pla
    tay
    cpx #$ff
    beq @done
    jsr $c6bb          ; from here to the jmp is spike_rewrite's own code: put
    lda $dc            ; the player back on the tile they came from. It stays
    sta $3a            ; in this bank because $b097 is in the game's own
    sta $8e            ; switchable bank, which is mapped in only here.
    sta $90
    lda $dd
    sta $3b
    sta $8f
    sta $92
    lda #$00
    sta $91
    sta $93
    ldx #$04
-   asl $90
    rol $91
    asl $92
    rol $93
    dex
    bne -
    lda #$02
    sta $602f
    jmp $b097
@done:
    jmp $e940

a7_spike_won:          ; replaces the loop at $e96b, after winning a battle
    tya
    pha
    ldy #1             ; exp_a7_won: marks this tile's guard or encounter beaten
    jsr a7_call
    pla
    tay
    jmp $e98c          ; jsr count_win, then the rest of the victory code

a7_call:               ; Y = 0 lookup, 1 won, 2 NPC settle. The same bank switch
                       ; Progressive Equipment's a8_check does, and for the same
                       ; reason: a slot in the $c288 dispatch table would move
                       ; every routine after it. exp_a7_entry picks the routine
                       ; by Y, and each ends with exp_load_last_bank, which
                       ; switches back and returns to a7_call's caller.
    lda $6004          ; the bank in use, for exp_load_last_bank
    sta $7004
    lda #3
    jsr store_and_load_prg_bank
    jmp exp_a7_entry

; Progressive Equipment starts at $c960 in this same window.
.if $ > $c960
    .error "Oops All Treasure Guards has overrun $c960, where Progressive Equipment starts"
.endif
.org $c9b5

.org $f232
    ; potentially unused data
.org $f35b

