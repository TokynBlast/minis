declare i64 @writev(i32, i8*, i32)

; iovec struct: {ptr, len}
@iov = global [1024 x {i8*, i64}] zeroinitializer  ; up to 1024 chunks
@iov_count = global i32 0

; WriteBuffer — just stores pointer and length, zero copying!
define void @write(i8* %str, i64 %len) {
    %idx = load i32, i32* @iov_count
    %slot = getelementptr [1024 x {i8*, i64}], [1024 x {i8*, i64}]* @iov, i32 0, i32 %idx
    %ptr_field = getelementptr {i8*, i64}, {i8*, i64}* %slot, i32 0, i32 0
    %len_field = getelementptr {i8*, i64}, {i8*, i64}* %slot, i32 0, i32 1
    store i8* %str, i8** %ptr_field
    store i64 %len, i64* %len_field
    %new_count = add i32 %idx, 1
    store i32 %new_count, i32* @iov_count
    ret void
}

; WriteOut — one syscall, writes all chunks at once!
define void @flush() {
    %count = load i32, i32* @iov_count
    %ptr = getelementptr [1024 x {i8*, i64}], [1024 x {i8*, i64}]* @iov, i32 0, i32 0
    call i64 @writev(i32 1, i8* %ptr, i32 %count)
    store i32 0, i32* @iov_count  ; reset
    ret void
}

; print — still direct, no copy, no buffer
define void @print(i8* %str, i64 %len) {
    call i64 @write(i32 1, i8* %str, i64 %len)
    ret void
}

declare i64 @write(i32, i8*, i64)
