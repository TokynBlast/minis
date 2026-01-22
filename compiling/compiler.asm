; First iteration of Minis
.main
  ; str contents = read("compiler.asm")
  push "compiler.asm"    ; file name
  call "read" 1          ; read file
  set contents           ; set the contents of file

  jmp preprocess

.preprocess
  get contents
  push "\n"
  call "split" 2
  set lines

  set list output []
  set ui64 pos 0

while_loop:
  get lines
  call "len" 1
  get pos
  lt
  jmpifn pre_proc_end

  get lines
  get pos
  call "index" 2
  set curr_line

  set str new_line ""
  set i64 line_pos 0
  set bool in_quotes false

char_loop:
  get curr_line
  call "len" 1
  get line_pos
  lt
  jmpifn end_line

  get curr_line
  get line_pos
  index
  set curr_char

  get curr_char
  push "\""
  eq
  jmpif toggle_quotes

  get curr_char
  push ";"
  eq
  get in_quotes
  jmpifn end_line

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
  push 0
  eq
  jmpif set_quotes_true
  push 0
  set in_quotes
  jmp after_toggle

set_quotes_true:
  push 1
  set in_quotes

after_toggle:
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
  push 0
  eq
  jmpif next_line

  get output
  get new_line
  call "append" 2
  set output

next_line:
  get pos
  push 1
  add
  set pos
  jmp while_loop

pre_proc_end:
  push "\n"
  get output
  call "print" 2
  jmp prog_end

.prog_end
  halt
