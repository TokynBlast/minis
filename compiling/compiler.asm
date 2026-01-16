; First iteration of Minis
.main:
  push "compiler.asm"
  push "r"
  call "open" 2
  set file

  get file
  call "read" 1
