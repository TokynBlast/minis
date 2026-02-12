; This was generated via compiling Fortran code.
; It used pdeudo.f08

@i64_bytes = constant i32 8
@mt_lower_mask = constant i64 2147483647
@mt_m = constant i32 397
@mt_matrix_a = constant i64 2567483615
@mt_state = global [624 x i64] zeroinitializer
@mt_index = global i32 625
@mt_n = constant i32 624
@mt_upper_mask = constant i64 2147483648
@mt_mag01 = internal global [2 x i64] [i64 0, i64 2567483615]

define void @mt_init(ptr noalias %seed_ptr) {
  %i_ptr = alloca i32, i64 1, align 4
  %seed_val = load i64, ptr %seed_ptr, align 8
  %seed_masked = and i64 %seed_val, 4294967295
  store i64 %seed_masked, ptr @mt_state, align 8
  br label %5

5:
  %i = phi i32 [ %i_next, %9 ], [ 2, %1 ]
  %remaining = phi i64 [ %remaining_next, %9 ], [ 623, %1 ]
  %has_remaining = icmp sgt i64 %remaining, 0
  br i1 %has_remaining, label %9, label %37

9:
  store i32 %i, ptr %i_ptr, align 4
  %i_val = load i32, ptr %i_ptr, align 4
  %i_minus1 = sub nsw i32 %i_val, 1
  %i_minus1_i64 = sext i32 %i_minus1 to i64
  %mt_idx_base = sub nsw i64 %i_minus1_i64, 1
  %mt_idx_mul1 = mul nsw i64 %mt_idx_base, 1
  %mt_idx_mul2 = mul nsw i64 %mt_idx_mul1, 1
  %mt_idx = add nsw i64 %mt_idx_mul2, 0
  %mt_ptr = getelementptr i64, ptr @mt_state, i64 %mt_idx
  %mt_prev = load i64, ptr %mt_ptr, align 8
  %mt_prev_shl30 = shl i64 %mt_prev, 30
  %mt_prev_shr30 = lshr i64 %mt_prev, 30
  %mt_prev_shift = select i1 true, i64 %mt_prev_shr30, i64 %mt_prev_shl30
  %mt_prev_shift_mask = select i1 false, i64 0, i64 %mt_prev_shift
  %mt_prev_xor = xor i64 %mt_prev, %mt_prev_shift_mask
  %mul = mul i64 %mt_prev_xor, 1812433253
  %i_val_i64 = sext i32 %i_val to i64
  %seed_add = add i64 %mul, %i_val_i64
  %seed_add_minus1 = sub i64 %seed_add, 1
  %seed_masked2 = and i64 %seed_add_minus1, 4294967295
  %i_val_minus1_i64 = sub nsw i64 %i_val_i64, 1
  %mt_idx2_mul1 = mul nsw i64 %i_val_minus1_i64, 1
  %mt_idx2_mul2 = mul nsw i64 %mt_idx2_mul1, 1
  %mt_idx2 = add nsw i64 %mt_idx2_mul2, 0
  %mt_ptr2 = getelementptr i64, ptr @mt_state, i64 %mt_idx2
  store i64 %seed_masked2, ptr %mt_ptr2, align 8
  %i_loaded = load i32, ptr %i_ptr, align 4
  %i_next = add nsw i32 %i_loaded, 1
  %remaining_next = sub i64 %remaining, 1
  br label %5

37:
  store i32 %i, ptr %i_ptr, align 4
  store i32 624, ptr @mt_index, align 4
  ret void
}

define i64 @mersenne_twister(ptr noalias %seed_ptr) {
  %y_ptr = alloca i64, i64 1, align 8
  %result_ptr = alloca i64, i64 1, align 8
  %kk_ptr = alloca i32, i64 1, align 4
  %index_val = load i32, ptr @mt_index, align 4
  %need_init = icmp sge i32 %index_val, 624
  br i1 %need_init, label %7, label %8

7:
  call void @mt_init(ptr %seed_ptr)
  br label %8

8:
  %index_val2 = load i32, ptr @mt_index, align 4
  %need_twist = icmp sge i32 %index_val2, 624
  br i1 %need_twist, label %11, label %149

11:
  br label %12

12:
  %kk = phi i32 [ %kk_next, %16 ], [ 1, %11 ]
  %remaining1 = phi i64 [ %remaining1_next, %16 ], [ 227, %11 ]
  %has_remaining1 = icmp sgt i64 %remaining1, 0
  br i1 %has_remaining1, label %16, label %69

16:
  store i32 %kk, ptr %kk_ptr, align 4
  %kk_val = load i32, ptr %kk_ptr, align 4
  %kk_i64 = sext i32 %kk_val to i64
  %kk_minus1 = sub nsw i64 %kk_i64, 1
  %idx_mul1 = mul nsw i64 %kk_minus1, 1
  %idx_mul2 = mul nsw i64 %idx_mul1, 1
  %idx = add nsw i64 %idx_mul2, 0
  %mt_ptr = getelementptr i64, ptr @mt_state, i64 %idx
  %mt_val = load i64, ptr %mt_ptr, align 8
  %upper = and i64 %mt_val, 2147483648
  %kk_plus1 = add nsw i32 %kk_val, 1
  %kk_plus1_i64 = sext i32 %kk_plus1 to i64
  %kk_plus1_minus1 = sub nsw i64 %kk_plus1_i64, 1
  %idx2_mul1 = mul nsw i64 %kk_plus1_minus1, 1
  %idx2_mul2 = mul nsw i64 %idx2_mul1, 1
  %idx2 = add nsw i64 %idx2_mul2, 0
  %mt_ptr2 = getelementptr i64, ptr @mt_state, i64 %idx2
  %mt_val2 = load i64, ptr %mt_ptr2, align 8
  %lower = and i64 %mt_val2, 2147483647
  %y = or i64 %upper, %lower
  store i64 %y, ptr %y_ptr, align 8
  %kk_val2 = load i32, ptr %kk_ptr, align 4
  %kk_plus_m = add nsw i32 %kk_val2, 397
  %kk_plus_m_i64 = sext i32 %kk_plus_m to i64
  %kk_plus_m_minus1 = sub nsw i64 %kk_plus_m_i64, 1
  %idx3_mul1 = mul nsw i64 %kk_plus_m_minus1, 1
  %idx3_mul2 = mul nsw i64 %idx3_mul1, 1
  %idx3 = add nsw i64 %idx3_mul2, 0
  %mt_ptr3 = getelementptr i64, ptr @mt_state, i64 %idx3
  %y_loaded = load i64, ptr %y_ptr, align 8
  %y_shl1 = shl i64 %y_loaded, 1
  %y_shr1 = lshr i64 %y_loaded, 1
  %y_shift = select i1 true, i64 %y_shr1, i64 %y_shl1
  %y_shift_mask = select i1 false, i64 0, i64 %y_shift
  %mt_val3 = load i64, ptr %mt_ptr3, align 8
  %twist_xor = xor i64 %mt_val3, %y_shift_mask
  %y_lsb = and i64 %y_loaded, 1
  %mag_idx = add nsw i64 %y_lsb, 1
  %mag_idx_minus1 = sub nsw i64 %mag_idx, 1
  %mag_idx_mul1 = mul nsw i64 %mag_idx_minus1, 1
  %mag_idx_mul2 = mul nsw i64 %mag_idx_mul1, 1
  %mag_idx0 = add nsw i64 %mag_idx_mul2, 0
  %mag_ptr = getelementptr i64, ptr @mt_mag01, i64 %mag_idx0
  %mag_val = load i64, ptr %mag_ptr, align 8
  %twisted = xor i64 %twist_xor, %mag_val
  %kk_val3 = sext i32 %kk_val2 to i64
  %kk_val3_minus1 = sub nsw i64 %kk_val3, 1
  %idx4_mul1 = mul nsw i64 %kk_val3_minus1, 1
  %idx4_mul2 = mul nsw i64 %idx4_mul1, 1
  %idx4 = add nsw i64 %idx4_mul2, 0
  %mt_ptr4 = getelementptr i64, ptr @mt_state, i64 %idx4
  store i64 %twisted, ptr %mt_ptr4, align 8
  %kk_loaded = load i32, ptr %kk_ptr, align 4
  %kk_next = add nsw i32 %kk_loaded, 1
  %remaining1_next = sub i64 %remaining1, 1
  br label %12

69:
  store i32 %kk, ptr %kk_ptr, align 4
  br label %70

70:
  %kk2 = phi i32 [ %kk2_next, %74 ], [ 228, %69 ]
  %remaining2 = phi i64 [ %remaining2_next, %74 ], [ 396, %69 ]
  %has_remaining2 = icmp sgt i64 %remaining2, 0
  br i1 %has_remaining2, label %74, label %127

74:
  store i32 %kk2, ptr %kk_ptr, align 4
  %kk2_val = load i32, ptr %kk_ptr, align 4
  %kk2_i64 = sext i32 %kk2_val to i64
  %kk2_minus1 = sub nsw i64 %kk2_i64, 1
  %idx5_mul1 = mul nsw i64 %kk2_minus1, 1
  %idx5_mul2 = mul nsw i64 %idx5_mul1, 1
  %idx5 = add nsw i64 %idx5_mul2, 0
  %mt_ptr5 = getelementptr i64, ptr @mt_state, i64 %idx5
  %mt_val5 = load i64, ptr %mt_ptr5, align 8
  %upper2 = and i64 %mt_val5, 2147483648
  %kk2_plus1 = add nsw i32 %kk2_val, 1
  %kk2_plus1_i64 = sext i32 %kk2_plus1 to i64
  %kk2_plus1_minus1 = sub nsw i64 %kk2_plus1_i64, 1
  %idx6_mul1 = mul nsw i64 %kk2_plus1_minus1, 1
  %idx6_mul2 = mul nsw i64 %idx6_mul1, 1
  %idx6 = add nsw i64 %idx6_mul2, 0
  %mt_ptr6 = getelementptr i64, ptr @mt_state, i64 %idx6
  %mt_val6 = load i64, ptr %mt_ptr6, align 8
  %lower2 = and i64 %mt_val6, 2147483647
  %y2 = or i64 %upper2, %lower2
  store i64 %y2, ptr %y_ptr, align 8
  %kk2_val2 = load i32, ptr %kk_ptr, align 4
  %kk2_minus_m = add i32 %kk2_val2, -227
  %kk2_minus_m_i64 = sext i32 %kk2_minus_m to i64
  %kk2_minus_m_minus1 = sub nsw i64 %kk2_minus_m_i64, 1
  %idx7_mul1 = mul nsw i64 %kk2_minus_m_minus1, 1
  %idx7_mul2 = mul nsw i64 %idx7_mul1, 1
  %idx7 = add nsw i64 %idx7_mul2, 0
  %mt_ptr7 = getelementptr i64, ptr @mt_state, i64 %idx7
  %y2_loaded = load i64, ptr %y_ptr, align 8
  %y2_shl1 = shl i64 %y2_loaded, 1
  %y2_shr1 = lshr i64 %y2_loaded, 1
  %y2_shift = select i1 true, i64 %y2_shr1, i64 %y2_shl1
  %y2_shift_mask = select i1 false, i64 0, i64 %y2_shift
  %mt_val7 = load i64, ptr %mt_ptr7, align 8
  %twist2_xor = xor i64 %mt_val7, %y2_shift_mask
  %y2_lsb = and i64 %y2_loaded, 1
  %mag2_idx = add nsw i64 %y2_lsb, 1
  %mag2_idx_minus1 = sub nsw i64 %mag2_idx, 1
  %mag2_idx_mul1 = mul nsw i64 %mag2_idx_minus1, 1
  %mag2_idx_mul2 = mul nsw i64 %mag2_idx_mul1, 1
  %mag2_idx0 = add nsw i64 %mag2_idx_mul2, 0
  %mag2_ptr = getelementptr i64, ptr @mt_mag01, i64 %mag2_idx0
  %mag2_val = load i64, ptr %mag2_ptr, align 8
  %twisted2 = xor i64 %twist2_xor, %mag2_val
  %kk2_val3 = sext i32 %kk2_val2 to i64
  %kk2_val3_minus1 = sub nsw i64 %kk2_val3, 1
  %idx8_mul1 = mul nsw i64 %kk2_val3_minus1, 1
  %idx8_mul2 = mul nsw i64 %idx8_mul1, 1
  %idx8 = add nsw i64 %idx8_mul2, 0
  %mt_ptr8 = getelementptr i64, ptr @mt_state, i64 %idx8
  store i64 %twisted2, ptr %mt_ptr8, align 8
  %kk2_loaded = load i32, ptr %kk_ptr, align 4
  %kk2_next = add nsw i32 %kk2_loaded, 1
  %remaining2_next = sub i64 %remaining2, 1
  br label %70

127:
  store i32 %kk2, ptr %kk_ptr, align 4
  %last_mt_val = load i64, ptr getelementptr inbounds nuw (i8, ptr @mt_state, i64 4984), align 8
  %last_upper = and i64 %last_mt_val, 2147483648
  %first_mt_val = load i64, ptr @mt_state, align 8
  %first_lower = and i64 %first_mt_val, 2147483647
  %y_last = or i64 %last_upper, %first_lower
  store i64 %y_last, ptr %y_ptr, align 8
  %y_last_loaded = load i64, ptr %y_ptr, align 8
  %y_last_shl1 = shl i64 %y_last_loaded, 1
  %y_last_shr1 = lshr i64 %y_last_loaded, 1
  %y_last_shift = select i1 true, i64 %y_last_shr1, i64 %y_last_shl1
  %y_last_shift_mask = select i1 false, i64 0, i64 %y_last_shift
  %mt_val_m = load i64, ptr getelementptr inbounds nuw (i8, ptr @mt_state, i64 3168), align 8
  %twist_last_xor = xor i64 %mt_val_m, %y_last_shift_mask
  %y_last_lsb = and i64 %y_last_loaded, 1
  %mag_last_idx = add nsw i64 %y_last_lsb, 1
  %mag_last_idx_minus1 = sub nsw i64 %mag_last_idx, 1
  %mag_last_idx_mul1 = mul nsw i64 %mag_last_idx_minus1, 1
  %mag_last_idx_mul2 = mul nsw i64 %mag_last_idx_mul1, 1
  %mag_last_idx0 = add nsw i64 %mag_last_idx_mul2, 0
  %mag_last_ptr = getelementptr i64, ptr @mt_mag01, i64 %mag_last_idx0
  %mag_last_val = load i64, ptr %mag_last_ptr, align 8
  %twisted_last = xor i64 %twist_last_xor, %mag_last_val
  store i64 %twisted_last, ptr getelementptr inbounds nuw (i8, ptr @mt_state, i64 4984), align 8
  store i32 1, ptr @mt_index, align 4
  br label %149

149:
  %index_val3 = load i32, ptr @mt_index, align 4
  %index_i64 = sext i32 %index_val3 to i64
  %index_minus1 = sub nsw i64 %index_i64, 1
  %idx9_mul1 = mul nsw i64 %index_minus1, 1
  %idx9_mul2 = mul nsw i64 %idx9_mul1, 1
  %idx9 = add nsw i64 %idx9_mul2, 0
  %mt_ptr9 = getelementptr i64, ptr @mt_state, i64 %idx9
  %y_out = load i64, ptr %mt_ptr9, align 8
  store i64 %y_out, ptr %y_ptr, align 8
  %index_val4 = load i32, ptr @mt_index, align 4
  %index_next = add i32 %index_val4, 1
  store i32 %index_next, ptr @mt_index, align 4
  %y0 = load i64, ptr %y_ptr, align 8
  %y0_shl11 = shl i64 %y0, 11
  %y0_shr11 = lshr i64 %y0, 11
  %y0_shift11 = select i1 true, i64 %y0_shr11, i64 %y0_shl11
  %y0_shift11_mask = select i1 false, i64 0, i64 %y0_shift11
  %y1 = xor i64 %y0, %y0_shift11_mask
  store i64 %y1, ptr %y_ptr, align 8
  %y2_temp = load i64, ptr %y_ptr, align 8
  %y2_shl7 = shl i64 %y2_temp, 7
  %y2_shr7 = lshr i64 %y2_temp, 7
  %y2_shift7 = select i1 false, i64 %y2_shr7, i64 %y2_shl7
  %y2_shift7_mask = select i1 false, i64 0, i64 %y2_shift7
  %y2_masked = and i64 %y2_shift7_mask, 2636928640
  %y2 = xor i64 %y2_temp, %y2_masked
  store i64 %y2, ptr %y_ptr, align 8
  %y3_temp = load i64, ptr %y_ptr, align 8
  %y3_shl15 = shl i64 %y3_temp, 15
  %y3_shr15 = lshr i64 %y3_temp, 15
  %y3_shift15 = select i1 false, i64 %y3_shr15, i64 %y3_shl15
  %y3_shift15_mask = select i1 false, i64 0, i64 %y3_shift15
  %y3_masked = and i64 %y3_shift15_mask, 4022730752
  %y3 = xor i64 %y3_temp, %y3_masked
  store i64 %y3, ptr %y_ptr, align 8
  %y4_temp = load i64, ptr %y_ptr, align 8
  %y4_shl18 = shl i64 %y4_temp, 18
  %y4_shr18 = lshr i64 %y4_temp, 18
  %y4_shift18 = select i1 true, i64 %y4_shr18, i64 %y4_shl18
  %y4_shift18_mask = select i1 false, i64 0, i64 %y4_shift18
  %y4 = xor i64 %y4_temp, %y4_shift18_mask
  store i64 %y4, ptr %y_ptr, align 8
  %y_final = load i64, ptr %y_ptr, align 8
  %y_final_masked = and i64 %y_final, 4294967295
  store i64 %y_final_masked, ptr %result_ptr, align 8
  %result = load i64, ptr %result_ptr, align 8
  ret i64 %result
}
