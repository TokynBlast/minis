; Declare the OS-specific functions from your files
declare i64 @true_rand_windows()      ; from windows.ll
declare i64 @true_rand_posix() ; from posix.ll
declare i64 @true_rand_macos()        ; from mac.ll

; which OS we'll be using :)
@TARGET_OS = internal constant i32 1

define i64 @get_os_entropy() alwaysinline {
entry:
  %os = load i32, ptr @TARGET_OS
  switch i32 %os, label %default [
    i32 0, label %posix   ; Linux
    i32 1, label %windows ; Windows
    i32 2, label %macos   ; Mac
    i32 3, label %posix   ; Solaris
    i32 4, label %posix   ; OS2
  ]

posix:
  %res_lin = call i64 @true_rand_linux()
  ret i64 %res_lin

windows:
  %res_win = call i64 @true_rand_windows()
  ret i64 %res_win

macos:
  %res_mac = call i64 @true_rand_macos()
  ret i64 %res_mac

default:
  ret i64 0
}
