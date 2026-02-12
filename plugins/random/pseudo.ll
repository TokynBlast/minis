; This was generated via compiling Fortran code.
; It used pdeudo.f08

@_QMrandomECi64 = constant i32 8
@_QMrandomEClower_mask = constant i64 2147483647
@_QMrandomECm = constant i32 397
@_QMrandomECmatrix_a = constant i64 2567483615
@_QMrandomEmt = global [624 x i64] zeroinitializer
@_QMrandomEmti = global i32 625
@_QMrandomECn = constant i32 624
@_QMrandomECupper_mask = constant i64 2147483648
@_QMrandomFmersene_twisterEmag01 = internal global [2 x i64] [i64 0, i64 2567483615]

define void @_QMrandomPmt_init(ptr noalias %0) {
  %2 = alloca i32, i64 1, align 4
  %3 = load i64, ptr %0, align 8
  %4 = and i64 %3, 4294967295
  store i64 %4, ptr @_QMrandomEmt, align 8
  br label %5

5:
  %6 = phi i32 [ %35, %9 ], [ 2, %1 ]
  %7 = phi i64 [ %36, %9 ], [ 623, %1 ]
  %8 = icmp sgt i64 %7, 0
  br i1 %8, label %9, label %37

9:
  store i32 %6, ptr %2, align 4
  %10 = load i32, ptr %2, align 4
  %11 = sub nsw i32 %10, 1
  %12 = sext i32 %11 to i64
  %13 = sub nsw i64 %12, 1
  %14 = mul nsw i64 %13, 1
  %15 = mul nsw i64 %14, 1
  %16 = add nsw i64 %15, 0
  %17 = getelementptr i64, ptr @_QMrandomEmt, i64 %16
  %18 = load i64, ptr %17, align 8
  %19 = shl i64 %18, 30
  %20 = lshr i64 %18, 30
  %21 = select i1 true, i64 %20, i64 %19
  %22 = select i1 false, i64 0, i64 %21
  %23 = xor i64 %18, %22
  %24 = mul i64 %23, 1812433253
  %25 = sext i32 %10 to i64
  %26 = add i64 %24, %25
  %27 = sub i64 %26, 1
  %28 = and i64 %27, 4294967295
  %29 = sub nsw i64 %25, 1
  %30 = mul nsw i64 %29, 1
  %31 = mul nsw i64 %30, 1
  %32 = add nsw i64 %31, 0
  %33 = getelementptr i64, ptr @_QMrandomEmt, i64 %32
  store i64 %28, ptr %33, align 8
  %34 = load i32, ptr %2, align 4
  %35 = add nsw i32 %34, 1
  %36 = sub i64 %7, 1
  br label %5

37:
  store i32 %6, ptr %2, align 4
  store i32 624, ptr @_QMrandomEmti, align 4
  ret void
}

define i64 @_QMrandomPmersene_twister(ptr noalias %0) {
  %2 = alloca i64, i64 1, align 8
  %3 = alloca i64, i64 1, align 8
  %4 = alloca i32, i64 1, align 4
  %5 = load i32, ptr @_QMrandomEmti, align 4
  %6 = icmp sge i32 %5, 624
  br i1 %6, label %7, label %8

7:
  call void @_QMrandomPmt_init(ptr %0)
  br label %8

8:
  %9 = load i32, ptr @_QMrandomEmti, align 4
  %10 = icmp sge i32 %9, 624
  br i1 %10, label %11, label %149

11:
  br label %12

12:
  %13 = phi i32 [ %67, %16 ], [ 1, %11 ]
  %14 = phi i64 [ %68, %16 ], [ 227, %11 ]
  %15 = icmp sgt i64 %14, 0
  br i1 %15, label %16, label %69

16:
  store i32 %13, ptr %4, align 4
  %17 = load i32, ptr %4, align 4
  %18 = sext i32 %17 to i64
  %19 = sub nsw i64 %18, 1
  %20 = mul nsw i64 %19, 1
  %21 = mul nsw i64 %20, 1
  %22 = add nsw i64 %21, 0
  %23 = getelementptr i64, ptr @_QMrandomEmt, i64 %22
  %24 = load i64, ptr %23, align 8
  %25 = and i64 %24, 2147483648
  %26 = add nsw i32 %17, 1
  %27 = sext i32 %26 to i64
  %28 = sub nsw i64 %27, 1
  %29 = mul nsw i64 %28, 1
  %30 = mul nsw i64 %29, 1
  %31 = add nsw i64 %30, 0
  %32 = getelementptr i64, ptr @_QMrandomEmt, i64 %31
  %33 = load i64, ptr %32, align 8
  %34 = and i64 %33, 2147483647
  %35 = or i64 %25, %34
  store i64 %35, ptr %2, align 8
  %36 = load i32, ptr %4, align 4
  %37 = add nsw i32 %36, 397
  %38 = sext i32 %37 to i64
  %39 = sub nsw i64 %38, 1
  %40 = mul nsw i64 %39, 1
  %41 = mul nsw i64 %40, 1
  %42 = add nsw i64 %41, 0
  %43 = getelementptr i64, ptr @_QMrandomEmt, i64 %42
  %44 = load i64, ptr %2, align 8
  %45 = shl i64 %44, 1
  %46 = lshr i64 %44, 1
  %47 = select i1 true, i64 %46, i64 %45
  %48 = select i1 false, i64 0, i64 %47
  %49 = load i64, ptr %43, align 8
  %50 = xor i64 %49, %48
  %51 = and i64 %44, 1
  %52 = add nsw i64 %51, 1
  %53 = sub nsw i64 %52, 1
  %54 = mul nsw i64 %53, 1
  %55 = mul nsw i64 %54, 1
  %56 = add nsw i64 %55, 0
  %57 = getelementptr i64, ptr @_QMrandomFmersene_twisterEmag01, i64 %56
  %58 = load i64, ptr %57, align 8
  %59 = xor i64 %50, %58
  %60 = sext i32 %36 to i64
  %61 = sub nsw i64 %60, 1
  %62 = mul nsw i64 %61, 1
  %63 = mul nsw i64 %62, 1
  %64 = add nsw i64 %63, 0
  %65 = getelementptr i64, ptr @_QMrandomEmt, i64 %64
  store i64 %59, ptr %65, align 8
  %66 = load i32, ptr %4, align 4
  %67 = add nsw i32 %66, 1
  %68 = sub i64 %14, 1
  br label %12

69:
  store i32 %13, ptr %4, align 4
  br label %70

70:
  %71 = phi i32 [ %125, %74 ], [ 228, %69 ]
  %72 = phi i64 [ %126, %74 ], [ 396, %69 ]
  %73 = icmp sgt i64 %72, 0
  br i1 %73, label %74, label %127

74:
  store i32 %71, ptr %4, align 4
  %75 = load i32, ptr %4, align 4
  %76 = sext i32 %75 to i64
  %77 = sub nsw i64 %76, 1
  %78 = mul nsw i64 %77, 1
  %79 = mul nsw i64 %78, 1
  %80 = add nsw i64 %79, 0
  %81 = getelementptr i64, ptr @_QMrandomEmt, i64 %80
  %82 = load i64, ptr %81, align 8
  %83 = and i64 %82, 2147483648
  %84 = add nsw i32 %75, 1
  %85 = sext i32 %84 to i64
  %86 = sub nsw i64 %85, 1
  %87 = mul nsw i64 %86, 1
  %88 = mul nsw i64 %87, 1
  %89 = add nsw i64 %88, 0
  %90 = getelementptr i64, ptr @_QMrandomEmt, i64 %89
  %91 = load i64, ptr %90, align 8
  %92 = and i64 %91, 2147483647
  %93 = or i64 %83, %92
  store i64 %93, ptr %2, align 8
  %94 = load i32, ptr %4, align 4
  %95 = add i32 %94, -227
  %96 = sext i32 %95 to i64
  %97 = sub nsw i64 %96, 1
  %98 = mul nsw i64 %97, 1
  %99 = mul nsw i64 %98, 1
  %100 = add nsw i64 %99, 0
  %101 = getelementptr i64, ptr @_QMrandomEmt, i64 %100
  %102 = load i64, ptr %2, align 8
  %103 = shl i64 %102, 1
  %104 = lshr i64 %102, 1
  %105 = select i1 true, i64 %104, i64 %103
  %106 = select i1 false, i64 0, i64 %105
  %107 = load i64, ptr %101, align 8
  %108 = xor i64 %107, %106
  %109 = and i64 %102, 1
  %110 = add nsw i64 %109, 1
  %111 = sub nsw i64 %110, 1
  %112 = mul nsw i64 %111, 1
  %113 = mul nsw i64 %112, 1
  %114 = add nsw i64 %113, 0
  %115 = getelementptr i64, ptr @_QMrandomFmersene_twisterEmag01, i64 %114
  %116 = load i64, ptr %115, align 8
  %117 = xor i64 %108, %116
  %118 = sext i32 %94 to i64
  %119 = sub nsw i64 %118, 1
  %120 = mul nsw i64 %119, 1
  %121 = mul nsw i64 %120, 1
  %122 = add nsw i64 %121, 0
  %123 = getelementptr i64, ptr @_QMrandomEmt, i64 %122
  store i64 %117, ptr %123, align 8
  %124 = load i32, ptr %4, align 4
  %125 = add nsw i32 %124, 1
  %126 = sub i64 %72, 1
  br label %70

127:
  store i32 %71, ptr %4, align 4
  %128 = load i64, ptr getelementptr inbounds nuw (i8, ptr @_QMrandomEmt, i64 4984), align 8
  %129 = and i64 %128, 2147483648
  %130 = load i64, ptr @_QMrandomEmt, align 8
  %131 = and i64 %130, 2147483647
  %132 = or i64 %129, %131
  store i64 %132, ptr %2, align 8
  %133 = load i64, ptr %2, align 8
  %134 = shl i64 %133, 1
  %135 = lshr i64 %133, 1
  %136 = select i1 true, i64 %135, i64 %134
  %137 = select i1 false, i64 0, i64 %136
  %138 = load i64, ptr getelementptr inbounds nuw (i8, ptr @_QMrandomEmt, i64 3168), align 8
  %139 = xor i64 %138, %137
  %140 = and i64 %133, 1
  %141 = add nsw i64 %140, 1
  %142 = sub nsw i64 %141, 1
  %143 = mul nsw i64 %142, 1
  %144 = mul nsw i64 %143, 1
  %145 = add nsw i64 %144, 0
  %146 = getelementptr i64, ptr @_QMrandomFmersene_twisterEmag01, i64 %145
  %147 = load i64, ptr %146, align 8
  %148 = xor i64 %139, %147
  store i64 %148, ptr getelementptr inbounds nuw (i8, ptr @_QMrandomEmt, i64 4984), align 8
  store i32 1, ptr @_QMrandomEmti, align 4
  br label %149

149:
  %150 = load i32, ptr @_QMrandomEmti, align 4
  %151 = sext i32 %150 to i64
  %152 = sub nsw i64 %151, 1
  %153 = mul nsw i64 %152, 1
  %154 = mul nsw i64 %153, 1
  %155 = add nsw i64 %154, 0
  %156 = getelementptr i64, ptr @_QMrandomEmt, i64 %155
  %157 = load i64, ptr %156, align 8
  store i64 %157, ptr %2, align 8
  %158 = load i32, ptr @_QMrandomEmti, align 4
  %159 = add i32 %158, 1
  store i32 %159, ptr @_QMrandomEmti, align 4
  %160 = load i64, ptr %2, align 8
  %161 = shl i64 %160, 11
  %162 = lshr i64 %160, 11
  %163 = select i1 true, i64 %162, i64 %161
  %164 = select i1 false, i64 0, i64 %163
  %165 = xor i64 %160, %164
  store i64 %165, ptr %2, align 8
  %166 = load i64, ptr %2, align 8
  %167 = shl i64 %166, 7
  %168 = lshr i64 %166, 7
  %169 = select i1 false, i64 %168, i64 %167
  %170 = select i1 false, i64 0, i64 %169
  %171 = and i64 %170, 2636928640
  %172 = xor i64 %166, %171
  store i64 %172, ptr %2, align 8
  %173 = load i64, ptr %2, align 8
  %174 = shl i64 %173, 15
  %175 = lshr i64 %173, 15
  %176 = select i1 false, i64 %175, i64 %174
  %177 = select i1 false, i64 0, i64 %176
  %178 = and i64 %177, 4022730752
  %179 = xor i64 %173, %178
  store i64 %179, ptr %2, align 8
  %180 = load i64, ptr %2, align 8
  %181 = shl i64 %180, 18
  %182 = lshr i64 %180, 18
  %183 = select i1 true, i64 %182, i64 %181
  %184 = select i1 false, i64 0, i64 %183
  %185 = xor i64 %180, %184
  store i64 %185, ptr %2, align 8
  %186 = load i64, ptr %2, align 8
  %187 = and i64 %186, 4294967295
  store i64 %187, ptr %3, align 8
  %188 = load i64, ptr %3, align 8
  ret i64 %188
}
