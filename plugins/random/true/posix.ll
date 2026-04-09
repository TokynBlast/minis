; ====================================================
; true_rand — Posix (/dev/urandom via open/read/close)
; ====================================================

@urandom_path = private constant [13 x i8] c"/dev/urandom\00"

declare i32 @open(ptr, i32, ...)
declare i64 @read(i32, ptr, i64)
declare i32 @close(i32)

define i64 @true_rand_posix() {
entry:
  %fd = call i32 (ptr, i32, ...) @open(ptr @urandom_path, i32 0)
  %buf = alloca [8 x i8], align 8
  %buf_ptr = getelementptr inbounds [8 x i8], ptr %buf, i32 0, i32 0
  call i64 @read(i32 %fd, ptr %buf_ptr, i64 8)
  call i32 @close(i32 %fd)
  br label %pack

pack:
  %b0 = load i8, ptr %buf_ptr,                              align 1
  %p1 = getelementptr i8, ptr %buf_ptr, i32 1
  %b1 = load i8, ptr %p1,                                   align 1
  %p2 = getelementptr i8, ptr %buf_ptr, i32 2
  %b2 = load i8, ptr %p2,                                   align 1
  %p3 = getelementptr i8, ptr %buf_ptr, i32 3
  %b3 = load i8, ptr %p3,                                   align 1
  %p4 = getelementptr i8, ptr %buf_ptr, i32 4
  %b4 = load i8, ptr %p4,                                   align 1
  %p5 = getelementptr i8, ptr %buf_ptr, i32 5
  %b5 = load i8, ptr %p5,                                   align 1
  %p6 = getelementptr i8, ptr %buf_ptr, i32 6
  %b6 = load i8, ptr %p6,                                   align 1
  %p7 = getelementptr i8, ptr %buf_ptr, i32 7
  %b7 = load i8, ptr %p7,                                   align 1

  %e0 = zext i8 %b0 to i64
  %e1 = shl i64 %e0,  0
  %e2 = zext i8 %b1 to i64
  %s1 = shl i64 %e2,  8
  %e3 = zext i8 %b2 to i64
  %s2 = shl i64 %e3,  16
  %e4 = zext i8 %b3 to i64
  %s3 = shl i64 %e4,  24
  %e5 = zext i8 %b4 to i64
  %s4 = shl i64 %e5,  32
  %e6 = zext i8 %b5 to i64
  %s5 = shl i64 %e6,  40
  %e7 = zext i8 %b6 to i64
  %s6 = shl i64 %e7,  48
  %e8 = zext i8 %b7 to i64
  %s7 = shl i64 %e8,  56

  %r01 = or i64 %e1,  %s1
  %r23 = or i64 %s2,  %s3
  %r45 = or i64 %s4,  %s5
  %r67 = or i64 %s6,  %s7
  %r03 = or i64 %r01, %r23
  %r47 = or i64 %r45, %r67
  %result = or i64 %r03, %r47

  ret i64 %result
}
