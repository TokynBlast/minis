import subprocess
import platform
import os
import tempfile
vals = "%%struct.Keyword = type { i8*, i8* }" # { key_ptr, value_ptr }

def _mult_template(bit_width: int, signed: bool) -> str:
  cmp_op = "slt" if signed else "ult"
  suffix = "signed" if signed else "unsigned"
  return f"""{vals}
define i{bit_width} @multiply_{suffix}_i{bit_width}(i64 %num_kwargs, %struct.Keyword* %kwargs_array) {{
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
}}"""


def ll_mult_gen() -> str:
  chunks = []
  sizes = [8, 16, 32, 64]
  for i in range(1,4):
    chunks.append(_mult_template(sizes[i], signed=True))
    chunks.append(_mult_template(sizes[i], signed=False))
  return "\n".join(chunks)

with open("maths.ll", "w") as mathf:
  mathf.write(ll_mult_gen())
  mathf.close()
