; ==========================================================
; random_float — platform-agnostic, takes a true_rand fn ptr
; ==========================================================

define double @random_float(double %a, double %b, ptr %rand_fn) {
entry:
  %need_swap = fcmp ogt double %a, %b
  %lo = select i1 %need_swap, double %b, double %a
  %hi = select i1 %need_swap, double %a, double %b

  ; call whichever true_rand was passed in
  %raw = call i64 %rand_fn()

  ; IEEE 754 bit-cast trick -> [0.0, 1.0)
  %mantissa = and i64 %raw, 4503599627370495       ; 0x000FFFFFFFFFFFFF
  %biased   = or  i64 %mantissa, 4607182418800017408 ; 0x3FF0000000000000
  %unit     = bitcast i64 %biased to double
  %frac     = fsub double %unit, 1.0

  %range  = fsub double %hi, %lo
  %scaled = fmul double %frac, %range
  %result = fadd double %lo, %scaled

  ret double %result
}


; ===========================================================
; random_choice — platform-agnostic, takes a true_rand fn ptr
; ===========================================================

define i64 @random_choice(ptr %arr, i64 %len, ptr %rand_fn) {
entry:
  %empty = icmp eq i64 %len, 0
  br i1 %empty, label %ret_null, label %pick

pick:
  %raw  = call i64 %rand_fn()
  %idx  = urem i64 %raw, %len
  %eptr = getelementptr i64, ptr %arr, i64 %idx
  %elem = load i64, ptr %eptr, align 8
  ret i64 %elem

ret_null:
  ret i64 0
}
