.main
  push "compiler.asm"
  call "read" 1
  set contents

  jmp preprocess


.preprocess
  get contents
  push "\n"
  call "split" 2
  set lines

  push []
  declare output
  set ui64 pos 0


while_loop:
  get lines
  call "len" 1
  set lines_len

  get pos
  get lines_len
  lt
  set cond_while

  get cond_while
  jmpifn pre_proc_end


  get lines
  get pos
  index
  set curr_line

  set str new_line ""
  set i64 line_pos 0
  set bool in_quotes false


char_loop:
  get curr_line
  call "len" 1
  set curr_line_len

  get line_pos
  get curr_line_len
  lt
  set cond_char

  get cond_char
  jmpifn end_line


  get curr_line
  get line_pos
  index
  set curr_char


  ; ────────────── QUOTE TOGGLE ──────────────
  get curr_char
  push "\""
  eq
  jmpif toggle_quotes


  ; ────────────── COMMENT CHECK ──────────────
  get curr_char
  push ";"
  eq
  set is_semicolon

  get in_quotes
  not
  set not_in_quotes

  get is_semicolon
  get not_in_quotes
  and
  set is_comment_cut

  get is_comment_cut
  jmpif end_line


  ; ────────────── NORMAL CHAR ──────────────
  get new_line
  get curr_char
  add
  set new_line

  get line_pos
  push 1
  add
  set line_pos

  jmp char_loop


toggle_quotes:
  get in_quotes
  not
  set in_quotes

  get new_line
  get curr_char
  add
  set new_line

  get line_pos
  push 1
  add
  set line_pos

  jmp char_loop


end_line:
  get new_line
  call "len" 1
  set new_line_len

  get new_line_len
  push 0
  eq
  set is_empty_line

  get is_empty_line
  jmpif next_line

  get output
  get new_line
  add
  set output


next_line:
  get pos
  push 1
  add
  set pos

  jmp while_loop


pre_proc_end:
  get output
  call "print" 1
  push "\n"
  call "print" 1

  jmp prog_end


.prog_end
  halt
