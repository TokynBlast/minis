! math stuffs in Fortran :)
! For the logical register :3
! So we can have AMAZING math!!
module math
  use iso_c_binding
  implicit none
contains

  !========================================
  ! SIGNED INTEGER MULTI-ADD FUNCTIONS
  !========================================
  function add_multi_i8(values, n) bind(C, name="add_multi_i8") result(r)
    integer(c_int32_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i16(values, n) bind(C, name="add_multi_i16") result(r)
    integer(c_int32_t), value :: n
    integer(c_int16_t), dimension(n), intent(in) :: values
    integer(c_int16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i32(values, n) bind(C, name="add_multi_i32") result(r)
    integer(c_int32_t), value :: n
    integer(c_int32_t), dimension(n), intent(in) :: values
    integer(c_int32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i64(values, n) bind(C, name="add_multi_i64") result(r)
    integer(c_int32_t), value :: n
    integer(c_int64_t), dimension(n), intent(in) :: values
    integer(c_int64_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  !========================================
  ! UNSIGNED INTEGER MULTI-ADD FUNCTIONS
  !========================================

  function add_multi_ui8(values, n) bind(C, name="add_multi_ui8") result(r)
    integer(c_int32_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui16(values, n) bind(C, name="add_multi_ui16") result(r)
    integer(c_int32_t), value :: n
    integer(c_int16_t), dimension(n), intent(in) :: values
    integer(c_int16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui32(values, n) bind(C, name="add_multi_ui32") result(r)
    integer(c_int32_t), value :: n
    integer(c_int32_t), dimension(n), intent(in) :: values
    integer(c_int32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui64(values, n) bind(C, name="add_multi_ui64") result(r)
    integer(c_int32_t), value :: n
    integer(c_int64_t), dimension(n), intent(in) :: values
    integer(c_int64_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  !========================================
  ! FLOATING POINT MULTI-ADD
  !========================================

  function add_multi_f64(values, n) bind(C, name="add_multi_f64") result(r)
    integer(c_int32_t), value :: n
    real(c_double), dimension(n), intent(in) :: values
    real(c_double) :: r
    integer :: i

    r = 0.0_c_double

    do i = 1, n
      r = r + values(i)
    end do
  end function

  !========================================
  ! SINGLE ADD FUNCTIONS
  !========================================
  function add_i8(a, b) bind(C, name="add_i8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a + b
  end function

  function add_i16(a, b) bind(C, name="add_i16") result(r)
    integer(c_int16_t), value :: a, b
    integer(c_int16_t) :: r
    r = a + b
  end function

  function add_i32(a, b) bind(C, name="add_i32") result(r)
    integer(c_int32_t), value :: a, b
    integer(c_int32_t) :: r
    r = a + b
  end function

  function add_i64(a, b) bind(C, name="add_i64") result(r)
    integer(c_int64_t), value :: a, b
    integer(c_int64_t) :: r
    r = a + b
  end function

  function add_ui8(a, b) bind(C, name="add_ui8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a + b
  end function

  function add_ui16(a, b) bind(C, name="add_ui16") result(r)
    integer(c_int16_t), value :: a, b
    integer(c_int16_t) :: r
    r = a + b
  end function

  function add_ui32(a, b) bind(C, name="add_ui32") result(r)
    integer(c_int32_t), value :: a, b
    integer(c_int32_t) :: r
    r = a + b
  end function

  function add_ui64(a, b) bind(C, name="add_ui64") result(r)
    integer(c_int64_t), value :: a, b
    integer(c_int64_t) :: r
    r = a + b
  end function

  function add_double(a, b) bind(C, name="add_double") result(r)
    real(c_double), value :: a, b
    real(c_double) :: r
    r = a + b
  end function

  !========================================
  ! MULTIPLY FUNCTIONS
  !========================================
  function mult_i8(a, b) bind(C, name="mult_i8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a * b
  end function

  function mult_i16(a, b) bind(C, name="mult_i16") result(r)
    integer(c_int16_t), value :: a, b
    integer(c_int16_t) :: r
    r = a * b
  end function

  function mult_i32(a, b) bind(C, name="mult_i32") result(r)
    integer(c_int32_t), value :: a, b
    integer(c_int32_t) :: r
    r = a * b
  end function

  function mult_i64(a, b) bind(C, name="mult_i64") result(r)
    integer(c_int64_t), value :: a, b
    integer(c_int64_t) :: r
    r = a * b
  end function

  function mult_ui8(a, b) bind(C, name="mult_ui8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a * b
  end function

  function mult_ui16(a, b) bind(C, name="mult_ui16") result(r)
    integer(c_int16_t), value :: a, b
    integer(c_int16_t) :: r
    r = a * b
  end function

  function mult_ui32(a, b) bind(C, name="mult_ui32") result(r)
    integer(c_int32_t), value :: a, b
    integer(c_int32_t) :: r
    r = a * b
  end function

  function mult_ui64(a, b) bind(C, name="mult_ui64") result(r)
    integer(c_int64_t), value :: a, b
    integer(c_int64_t) :: r
    r = a * b
  end function

  function mult_double(a, b) bind(C, name="mult_double") result(r)
    real(c_double), value :: a, b
    real(c_double) :: r
    r = a * b
  end function

  !========================================
  ! DIVIDE FUNCTIONS
  !========================================
  function div_i8(a, b) bind(C, name="div_i8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a / b
  end function

  function div_i16(a, b) bind(C, name="div_i16") result(r)
    integer(c_int16_t), value :: a, b
    integer(c_int16_t) :: r
    r = a / b
  end function

  function div_i32(a, b) bind(C, name="div_i32") result(r)
    integer(c_int32_t), value :: a, b
    integer(c_int32_t) :: r
    r = a / b
  end function

  function div_i64(a, b) bind(C, name="div_i64") result(r)
    integer(c_int64_t), value :: a, b
    integer(c_int64_t) :: r
    r = a / b
  end function

  function div_ui8(a, b) bind(C, name="div_ui8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a / b
  end function

  function div_ui16(a, b) bind(C, name="div_ui16") result(r)
    integer(c_int16_t), value :: a, b
    integer(c_int16_t) :: r
    r = a / b
  end function

  function div_ui32(a, b) bind(C, name="div_ui32") result(r)
    integer(c_int32_t), value :: a, b
    integer(c_int32_t) :: r
    r = a / b
  end function

  function div_ui64(a, b) bind(C, name="div_ui64") result(r)
    integer(c_int64_t), value :: a, b
    integer(c_int64_t) :: r
    r = a / b
  end function

  function div_double(a, b) bind(C, name="div_double") result(r)
    real(c_double), value :: a, b
    real(c_double) :: r
    r = a / b
  end function

end module
