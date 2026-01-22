.main
  push "Running :)\n"
  call "print" 1

  push "compiler.asm"
  call "read" 1
  set contents

  ;push "set contents :D\n"
  ;call "print" 1

  jmp preprocess


.preprocess
  push "in preprocess!\n"
  call "print" 1

  get contents
  push "\n"
  call "split" 2
  set lines

  push "lines ["
  call "print" 1
  get lines
  call "print" 1
  push "] lines\n"
  call "print" 1

  ;push "set lines to:\n"
  ;get lines
  ;call "print" 2

  push []
  declare output
  set ui64 pos 0
  jmp while_loop

while_loop:
  push "while_loop\n"
  call "print" 1

  get lines
  call "len" 1
  set lines_len

  push "set lines_len, "
  get lines_len
  push "\n"
  call "print" 3

  get pos
  get lines_len
  lt
  ;not
  set cond_while

  push "pos = "
  get pos

  push "\nset cond_while, "
  get cond_while

  push "\n\n\n"
  push true
  push "\n"
  push false
  push "\n\n\n"
  call "print" 9

  get cond_while
  jmpifn pre_proc_end


  get lines
  get pos
  index
  set curr_line

  push "set curr_line: "
  get curr_line
  push "\n"
  call "print" 3

  set str new_line "a"
  set i64 line_pos 0
  set bool in_quotes false


char_loop:
  push "char_loop"
  call "print" 1
  get curr_line
  call "len" 1
  set curr_line_len

  get line_pos
  get curr_line_len
  lt
  set cond_char

  get cond_char
  jmpifn end_line


  push "curr_char = "
  get curr_char
  push "\n"
  call "print" 3


  get curr_line
  get line_pos
  index
  set curr_char


  ; QUOTE TOGGLE
  get curr_char
  push "\""
  eq
  jmpif toggle_quotes


  ; COMMENT CHECK
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


  ; NORMAL CHAR
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
  push "toggle_quotes"
  call "print" 1
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
  push "end_line"
  call "print" 1
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
  push "next_line\n"
  call "print" 1
  get pos
  push 1
  add
  set pos

  jmp while_loop


pre_proc_end:
  push "pre_proc_end\n"
  get output
  push "\n"
  call "print" 3

  jmp prog_end


.prog_end
  push "prog_end\n"
  call "print" 1
  halt
