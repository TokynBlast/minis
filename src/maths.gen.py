def _mult_template(bit_width: int, signed: bool) -> str:
  cmp_op = "slt" if signed else "ult"
  suffix = "signed" if signed else "unsigned"
  return f"""define i{bit_width} @multiply_{suffix}_i{bit_width}(i64 %num_kwargs, %struct.Keyword* noalias %kwargs_array) {{
  %result = alloca i{bit_width}
  store i{bit_width} 1, i{bit_width}* %result
  %loop_counter = alloca i64
  store i64 0, i64* %loop_counter
  br label %loop_start

loop_start:
  %current_index = load i64, i64* %loop_counter
  %cond = icmp {cmp_op} i64 %current_index, %num_kwargs
  br i1 %cond, label %loop_body, label %loop_end

loop_body:
  %val = load i{bit_width}, i{bit_width}* %result
  %incremented = add i{bit_width} %val, 1
  store i{bit_width} %incremented, i{bit_width}* %result
  %next_index = add i64 %current_index, 1
  store i64 %next_index, i64* %loop_counter
  br label %loop_start

loop_end:
  %final_result = load i{bit_width}, i{bit_width}* %result
  ret i{bit_width} %final_result
}}
"""

def _add_template(bit_width: int, signed: bool) -> str:
  suffix = "signed" if signed else "unsigned"
  return f"""define alwaysinline i{bit_width} @add_{suffix}_i{bit_width}(i{bit_width} %a, i{bit_width} %b) {{
  %result = add i{bit_width} %a, %b
  ret i{bit_width} %result
}}
"""

def _sub_template(bit_width: int, signed: bool) -> str:
  suffix = "signed" if signed else "unsigned"
  return f"""define alwaysinline i{bit_width} @sub_{suffix}_i{bit_width}(i{bit_width} %a, i{bit_width} %b) {{
  %result = sub i{bit_width} %a, %b
  ret i{bit_width} %result
}}
"""

def _square_template(bit_width: int, signed: bool) -> str:
  suffix = "signed" if signed else "unsigned"
  return f"""define alwaysinline i{bit_width} @square_{suffix}_i{bit_width}(i{bit_width} %a) {{
  %result = mul i{bit_width} %a, %a
  ret i{bit_width} %result
}}
"""

def _div_template(bit_width: int, signed: bool) -> str:
  suffix = "signed" if signed else "unsigned"
  div_op = "sdiv" if signed else "udiv"
  return f"""define i{bit_width} @div_{suffix}_i{bit_width}(i{bit_width} %a, i{bit_width} %b) {{
  %is_power_of_2 = call i1 @is_power_of_2_i{bit_width}(i{bit_width} %b)
  br i1 %is_power_of_2, label %fast_div, label %slow_div

fast_div:
  %shift_amt = call i{bit_width} @ctz_i{bit_width}(i{bit_width} %b)
  %result_fast = ashr i{bit_width} %a, %shift_amt
  ret i{bit_width} %result_fast

slow_div:
  %result_slow = {div_op} i{bit_width} %a, %b
  ret i{bit_width} %result_slow
}}
"""

def _is_power_of_2_template(bit_width: int) -> str:
  return f"""define alwaysinline i1 @is_power_of_2_i{bit_width}(i{bit_width} %a) {{
  %cmp_zero = icmp eq i{bit_width} %a, 0
  br i1 %cmp_zero, label %false_branch, label %check_bit

check_bit:
  %sub_one = sub i{bit_width} %a, 1
  %and_result = and i{bit_width} %a, %sub_one
  %result = icmp eq i{bit_width} %and_result, 0
  ret i1 %result

false_branch:
  ret i1 0
}}
"""

def _ctz_template(bit_width: int) -> str:
  return f"""define alwaysinline i{bit_width} @ctz_i{bit_width}(i{bit_width} %a) {{
  %ctz = call i{bit_width} @llvm.cttz.i{bit_width}(i{bit_width} %a, i1 0)
  ret i{bit_width} %ctz
}}
"""

def _sqrt_template(bit_width: int, signed: bool) -> str:
  suffix = "signed" if signed else "unsigned"
  return f"""define i{bit_width} @sqrt_{suffix}_i{bit_width}(i{bit_width} %a) {{
  %result = call i{bit_width} @isqrt_i{bit_width}(i{bit_width} %a)
  ret i{bit_width} %result
}}
"""

def _isqrt_template(bit_width: int) -> str:
  return f"""define i{bit_width} @isqrt_i{bit_width}(i{bit_width} %a) {{
  %cmp_zero = icmp eq i{bit_width} %a, 0
  br i1 %cmp_zero, label %return_zero, label %newton_method

return_zero:
  ret i{bit_width} 0

newton_method:
  %x = alloca i{bit_width}
  %x_prev = alloca i{bit_width}
  store i{bit_width} %a, i{bit_width}* %x
  store i{bit_width} 0, i{bit_width}* %x_prev
  br label %loop_start

loop_start:
  %x_val = load i{bit_width}, i{bit_width}* %x
  %x_prev_val = load i{bit_width}, i{bit_width}* %x_prev
  %x_div_2 = ashr i{bit_width} %x_val, 1
  %a_div_x = udiv i{bit_width} %a, %x_val
  %x_new = add i{bit_width} %x_div_2, %a_div_x
  %x_new_div_2 = ashr i{bit_width} %x_new, 1
  store i{bit_width} %x_new_div_2, i{bit_width}* %x
  %cmp = icmp ult i{bit_width} %x_new_div_2, %x_val
  br i1 %cmp, label %loop_continue, label %loop_end

loop_continue:
  store i{bit_width} %x_val, i{bit_width}* %x_prev
  br label %loop_start

loop_end:
  %result = load i{bit_width}, i{bit_width}* %x
  ret i{bit_width} %result
}}
"""

def ll_gen() -> str:
  chunks = ["%%struct.Keyword = type { i8*, i8* }"] # { key_ptr, value_ptr }
  sizes = [8, 16, 32, 64, 128, 256]

  for size in sizes:
    chunks.append(_mult_template(size, signed=True))
    chunks.append(_mult_template(size, signed=False))
    chunks.append(_add_template(size, signed=True))
    chunks.append(_add_template(size, signed=False))
    chunks.append(_sub_template(size, signed=True))
    chunks.append(_sub_template(size, signed=False))
    chunks.append(_square_template(size, signed=True))
    chunks.append(_square_template(size, signed=False))
    chunks.append(_div_template(size, signed=True))
    chunks.append(_div_template(size, signed=False))
    chunks.append(_is_power_of_2_template(size))
    chunks.append(_ctz_template(size))
    chunks.append(_sqrt_template(size, signed=True))
    chunks.append(_sqrt_template(size, signed=False))
    chunks.append(_isqrt_template(size))

  return "\n".join(chunks)

output = ll_gen()
with open("src/maths.ll", "w") as mathf:
  mathf.write(output)
  mathf.close()
