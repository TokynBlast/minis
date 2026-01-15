! This whole file is 99% boiler plate
! Everything takes values and n
! values is all of the numbers
! n is the number of numbers (haha) to operate on
! Each function is named in a descriptive way

! FIXME: Possibly use a boilerplate generator, lowering maitmence cost.

module math
  use iso_c_binding
  implicit none
contains

  !=========================
  ! SIGNED INTEGER MULTI-ADD
  !=========================
  function add_multi_i8(values, n) bind(C, name="add_multi_i8") result(r)
    integer(c_uint64_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i16(values, n) bind(C, name="add_multi_i16") result(r)
    integer(c_uint64_t), value :: n
    integer(c_int16_t), dimension(n), intent(in) :: values
    integer(c_int16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i32(values, n) bind(C, name="add_multi_i32") result(r)
    integer(c_uint64_t), value :: n
    integer(c_int32_t), dimension(n), intent(in) :: values
    integer(c_int32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i64(values, n) bind(C, name="add_multi_i64") result(r)
    integer(c_uint64_t), value :: n
    integer(c_int64_t), dimension(n), intent(in) :: values
    integer(c_int64_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  !===========================
  ! UNSIGNED INTEGER MULTI-ADD
  !===========================

  function add_multi_ui8(values, n) bind(C, name="add_multi_ui8") result(r)
    integer(c_uint64_t), value :: n
    integer(c_uint8_t), dimension(n), intent(in) :: values
    integer(c_uint8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui16(values, n) bind(C, name="add_multi_ui16") result(r)
    integer(c_uint64_t), value :: n
    integer(c_uint16_t), dimension(n), intent(in) :: values
    integer(c_uint16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui32(values, n) bind(C, name="add_multi_ui32") result(r)
    integer(c_uint64_t), value :: n
    integer(c_uint32_t), dimension(n), intent(in) :: values
    integer(c_uint32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui64(values, n) bind(C, name="add_multi_ui64") result(r)
    integer(c_uint64_t), value :: n
    integer(c_uint64_t), dimension(n), intent(in) :: values
    integer(c_uint64_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  ! ========================
  ! FLOATING POINT MULTI-ADD
  ! ========================

  function add_multi_f64(values, n) bind(C, name="add_multi_f64") result(r)
    integer(c_uint64_t), value :: n
    real(c_double), dimension(n), intent(in) :: values
    real(c_double) :: r
    integer :: i

    r = 0.0_c_double

    do i = 1, n
      r = r + values(i)
    end do
  end function

  ! ======================
  ! INTEGER MULTI-MULTIPLY
  ! ======================

  function multiply_multi_i8(values, n) bind(C, name="multiply_multi_i8") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_int_8_t), dimension(n), intent(in) :: value
    integer(c_int_8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_i16(values, n) bind(C, name="multiply_multi_i16") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_int_16_t), dimension(n), intent(in) :: value
    integer(c_int_16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_i32(values, n) bind(C, name="multiply_multi_i32") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_int_32_t), dimension(n), intent(in) :: value
    integer(c_int_32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_i64(values, n) bind(C, name="multiply_multi_i64") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_int_64_t), dimension(n), intent(in) :: value
    integer(c_int_64_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_ui8(values, n) bind(C, name="multiply_multi_ui8") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_uint_8_t), dimension(n), intent(in) :: value
    integer(c_uint_8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_ui16(values, n) bind(C, name="multiply_multi_ui16") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_uint_16_t), dimension(n), intent(in) :: value
    integer(c_uint_16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_ui32(values, n) bind(C, name="multiply_multi_ui32") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_uint_32_t), dimension(n), intent(in) :: value
    integer(c_uint_32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function

  function multiply_multi_ui64(values, n) bind(C, name="multiply_multi_ui64") result(r)
    integer(c_uint_64_t), value :: n
    integer(c_uint_64_t), dimension(n), intent(in) :: value
    integer(c_uint_64_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r * vaues(i)
    end do
  end function
end module
