! math stuffs in Fortran :)
! For the logical register :3
! So we can have AMAZING math!!
module math
  use iso_c_binding
  implicit none
contains

  !=========================
  ! SIGNED INTEGER MULTI-ADD
  !=========================
  function add_multi_i8(values, n) bind(C, name="add_multi_i8") result(r)
    integer(c_int64_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_i16(values, n) bind(C, name="add_multi_i16") result(r)
    integer(c_int64_t), value :: n
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
    integer(c_int64_t), value :: n
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
    integer(c_int64_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui16(values, n) bind(C, name="add_multi_ui16") result(r)
    integer(c_int64_t), value :: n
    integer(c_int16_t), dimension(n), intent(in) :: values
    integer(c_int16_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui32(values, n) bind(C, name="add_multi_ui32") result(r)
    integer(c_int64_t), value :: n
    integer(c_int32_t), dimension(n), intent(in) :: values
    integer(c_int32_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

  function add_multi_ui64(values, n) bind(C, name="add_multi_ui64") result(r)
    integer(c_int64_t), value :: n
    integer(c_int64_t), dimension(n), intent(in) :: values
    integer(c_int64_t) :: r
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
    integer(c_int64_t), value :: n
    real(c_double), dimension(n), intent(in) :: values
    real(c_double) :: r
    integer :: i

    r = 0.0_c_double

    do i = 1, n
      r = r + values(i)
    end do
  end function

end module
