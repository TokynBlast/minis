; MASM example - read file and print contents
; Sections start with .name
; Labels end with :
; Comments start with ;

.main
  push "example.txt"   ; filename
  push "r"             ; mode
  call "open" 2        ; -> file handle on stack
  set fh               ; pop into variable fh

  get fh               ; push file handle
  push 99999           ; max bytes
  call "read" 2        ; -> string on stack
  set contents         ; pop into variable

  get fh
  call "close" 1       ; close the file

  get contents
  println              ; print with newline

  halt

; Example with conditionals and jumps
.hell
  push "Enter yes or no: "
  call "input" 1       ; prompt -> result on stack
  set x

  get x
  push "yes"
  eq                   ; compares, pushes true/false
  jmpif go_in          ; jump if top of stack is true

  push false
  ret                  ; return false

go_in:
  push true
  ret                  ; return true

; Math example
.math_demo
  push 10
  push 20
  add                  ; pops 20,10 -> pushes 30
  set result

  get result
  push 5
  mult                 ; pops 5,30 -> pushes 150
  println

  ; increment/decrement
  inc counter          ; counter = counter + 1
  decc counter         ; counter = counter - 1

  ; stack manipulation
  push 1
  push 2
  dup                  ; stack: 1 2 2
  swap                 ; stack: 1 2 2 -> 1 2 2 (swaps top two)
  pop                  ; discard top

  halt
