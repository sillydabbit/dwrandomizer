
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

; *** Tablet Cave Torch ***
; Lights Erdrick's Cave and nothing else. Permanent Torch cannot simply be
; narrowed, because it does not light caves directly: the map-load block sets
; the radius only for maps < 15, so every real dungeon inherits whatever the
; overworld left behind. Gating on the map therefore means setting the radius
; on cave entry instead, and both halves of the effect have to be map-aware.
;
; ERDRICKS_CAVE is 28 and ERDRICKS_CAVE_2 is 29 - the last two map ids - so one
; "bcs" covers both floors without a table or a range check.
;
; The light must not leave with the player. Under Stair Shuffle a Tablet Cave
; staircase can lead straight into another dungeon, whose map load leaves the
; radius alone. Tablet Cave's light is radius 3 with the timer at 0, and a zero
; timer means no countdown at all, so it would light that whole dungeon exactly
; like a vanilla torch - which is also radius 3 with the timer at 0, so the
; game's own state cannot tell the two apart. TABLET_LIT records that the light
; is Tablet Cave's, and the next dungeon map load outside it switches it off.
; Radiant and a torch the player lit themselves are theirs, and carry over
; exactly as they would without this flag - see @left and tablet_torch_used.

tablet_torch_radius:   ; replaces the 13-byte map-load block at $2f12.
                       ; Radius 3 in Tablet Cave, 1 on the overworld and in
                       ; towns. Every other dungeon is left alone, as vanilla,
                       ; unless the player has just come out of Tablet Cave.
    lda $45            ; current map
    cmp #28
    bcs @lit
    cmp #$0f
    bcs @left          ; another dungeon
    lda #$00           ; overworld or town: the radius is reset just below, so
    sta TABLET_LIT     ; there is nothing left to take back
    lda #$01
    bne @set           ; always taken
@left:                 ; Tablet Cave's own light stays behind; Radiant does not.
                       ; A running timer means Radiant is still in effect, and
                       ; it carries over at its current size and fades as it
                       ; always would. Tablet Cave's light never runs the timer
                       ; - entry sets it to 0 and the hold only fires once it
                       ; has already reached 0 - so timer 0 with the flag set is
                       ; that light, and it switches off.
    lda TABLET_LIT
    beq @done          ; not Tablet Cave's light: leave it alone, as vanilla
    lda #$00
    sta TABLET_LIT
    lda $da
    bne @done          ; Radiant is running: carry it over untouched
    lda #$01
    sta $d0            ; back to an unlit dungeon
    rts
@lit:                  ; Raise the radius, never lower it. Radiant grows the
                       ; radius to 7 and fades it back down over time, and
                       ; vanilla skips this whole block for maps >= 15 so caves
                       ; inherit both the radius and the timer across stairs -
                       ; which is the only reason Permanent Torch works.
                       ; Writing unconditionally cut Radiant back to 3 on every
                       ; staircase in here.
    lda $d0
    cmp #$03
    bcs @done          ; already 3 or more - leave it, and leave $da alone
    lda #$03
    sta TABLET_LIT     ; 3 is nonzero: this light is Tablet Cave's
    sta $d0
    lda #$00           ; timer 0, not the 1 the shared set below would leave:
    sta $da            ; a nonzero timer is how @left recognises Radiant
    rts
@set:
    sta $d0            ; light radius
    lsr a
    sta $da            ; radiant timer
@done:
    rts

tablet_torch_hold:     ; replaces "lda $d0 / cmp #$01" at $ca5e, which only
                       ; runs once the radiant timer has counted down to zero.
                       ; Returns Z set when the radius must NOT decay: radius 3
                       ; inside Tablet Cave, or radius 1 anywhere, which is what
                       ; vanilla checked. This half has to be map-aware too.
                       ; Radiant grows the radius to 7 and then fades it by 2
                       ; per timer period, 7 -> 5 -> 3 -> 1, so it passes
                       ; through 3 on the way down in every cave - a blanket
                       ; hold at 3 would stop it there and make Radiant
                       ; permanent everywhere.
    lda $45
    cmp #28
    bcc @vanilla
    lda $d0
    cmp #$03
    bne @vanilla
    sta TABLET_LIT     ; holding at 3 where vanilla would have decayed, so
                       ; Tablet Cave is now the one supplying this light. STA
                       ; leaves Z set from the compare.
    rts
@vanilla:
    lda $d0
    cmp #$01
    rts

tablet_torch_used:     ; replaces "lda #$00 / sta $da" at $dd34, the moment the
                       ; Torch item lights up. The light is now the player's own
                       ; torch, which vanilla keeps for the rest of the dungeon,
                       ; so Tablet Cave no longer gets to take it back. The next
                       ; instruction reloads A, so nothing depends on what this
                       ; leaves behind.
    lda #$00
    sta $da            ; what the original two instructions did
    sta TABLET_LIT
    rts

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

