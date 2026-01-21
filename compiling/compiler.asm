; First iteration of Minis
.main
  push "compiler.asm"    ; file name
  call "read" 1          ; read file
  set contents           ; set the contents of file

.preprocess              ; remove comments (like these), then empty lines
  get contents           ; get contents variable
  push "\n"              ; split by \n
  call "split" 2         ; call split
  set lines              ; set the split list

  set list output []     ; all the lines :D

  set str new_line ""

  set ui64 pos 0         ; set pointer position

  while_loop:
    get pos              ; get loop starter
    get lines            ; get lines
    call "len" 1         ; get length of lines
    lt                   ; check if i < len of lines
    jmpifn pre_proc_end  ; go to pre_proc_end if true


    ; Loop body - process line
    get lines            ; get all lines
    get pos              ; get pos in lines
    call "index" 2       ; get line at pos
    push ""              ; set what to split by
    call "split" 2       ; split every char
    set curr_line        ; set list to curr_line

    set i32 line_pos 0

    while_in_comment:
      ; Increment line, then jump to while_in_comment :)

      ; check for end of line :3
      get line_pos
      get curr_line
      call "len" 1
      lt
      jmpif increment_line

      ; Set the current character
      get curr_line
      get line_pos
      call "index" 2
      set curr_char

      get curr_char
      push ";"
      eq
      jmpif increment_line

      get new_line
      get curr_char
      add
      set new_line

      get line_pos
      push 1
      add
      set line_pos


    increment_line:
      set line_pos 0
      get curr_line
      push 1
      add
      set curr_line

      get output
      get new_line
      add
      set contents

      push ""
      set new_line

      jmp while_in_comment

  pre_proc_end:
    push "\n"
    get output
    call "print" 2
    jmp prog_end

prog_end:
  ; Free up al variables :)
  ;unset contents
  ;unset lines
  ;unset pos
  ;unset curr_line
  ;unset line_pos
  ;unset output
  halt
