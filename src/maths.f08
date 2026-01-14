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
  function add_multi_i8(values, n, overflowed) bind(C, name="add_multi_i8") result(r)
    integer(c_int32_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int8_t) :: r
    integer(c_int16_t) :: temp
    integer :: i

    temp = 0
    overflowed = 0

    do i = 1, n
      temp = temp + int(values(i), c_int16_t)
    end do

    if (temp > 127 .or. temp < -128) then
      overflowed = 1
      r = 0
    else
      r = int(temp, c_int8_t)
    end if
  end function

  function add_multi_i16(values, n, overflowed) bind(C, name="add_multi_i16") result(r)
    integer(c_int32_t), value :: n
    integer(c_int16_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int16_t) :: r
    integer(c_int32_t) :: temp
    integer :: i

    temp = 0
    overflowed = 0

    do i = 1, n
      temp = temp + int(values(i), c_int32_t)
    end do

    if (temp > 32767 .or. temp < -32768) then
      overflowed = 1
      r = 0
    else
      r = int(temp, c_int16_t)
    end if
  end function

  function add_multi_i32(values, n, overflowed) bind(C, name="add_multi_i32") result(r)
    integer(c_int32_t), value :: n
    integer(c_int32_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int32_t) :: r
    integer(c_int64_t) :: temp
    integer :: i

    temp = 0
    overflowed = 0

    do i = 1, n
      temp = temp + int(values(i), c_int64_t)
    end do

    if (temp > 2147483647_c_int64_t .or. temp < -2147483648_c_int64_t) then
      overflowed = 1
      r = 0
    else
      r = int(temp, c_int32_t)
    end if
  end function

  function add_multi_i64(values, n, overflowed) bind(C, name="add_multi_i64") result(r)
    integer(c_int32_t), value :: n
    integer(c_int64_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int64_t) :: r
    integer(c_int64_t) :: prev
    integer :: i

    r = 0
    overflowed = 0

    do i = 1, n
      prev = r
      r = r + values(i)
      ! Check for overflow: if signs match but result sign differs
      if (values(i) > 0 .and. prev > 0 .and. r < 0) then
        overflowed = 1
        return
      else if (values(i) < 0 .and. prev < 0 .and. r > 0) then
        overflowed = 1
        return
      end if
    end do
  end function

  !========================================
  ! UNSIGNED INTEGER MULTI-ADD FUNCTIONS
  !========================================

  function add_multi_ui8(values, n, overflowed) bind(C, name="add_multi_ui8") result(r)
    integer(c_int32_t), value :: n
    integer(c_int8_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int8_t) :: r
    integer(c_int16_t) :: temp
    integer :: i

    temp = 0
    overflowed = 0

    do i = 1, n
      temp = temp + iand(int(values(i), c_int16_t), 255)
    end do

    if (temp > 255) then
      overflowed = 1
      r = 0
    else
      r = int(temp, c_int8_t)
    end if
  end function

  function add_multi_ui16(values, n, overflowed) bind(C, name="add_multi_ui16") result(r)
    integer(c_int32_t), value :: n
    integer(c_int16_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int16_t) :: r
    integer(c_int32_t) :: temp
    integer :: i

    temp = 0
    overflowed = 0

    do i = 1, n
      temp = temp + iand(int(values(i), c_int32_t), 65535)
    end do

    if (temp > 65535) then
      overflowed = 1
      r = 0
    else
      r = int(temp, c_int16_t)
    end if
  end function

  function add_multi_ui32(values, n, overflowed) bind(C, name="add_multi_ui32") result(r)
    integer(c_int32_t), value :: n
    integer(c_int32_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int32_t) :: r
    integer(c_int64_t) :: temp
    integer :: i

    temp = 0
    overflowed = 0

    do i = 1, n
      temp = temp + iand(int(values(i), c_int64_t), 4294967295_c_int64_t)
    end do

    if (temp > 4294967295_c_int64_t) then
      overflowed = 1
      r = 0
    else
      r = int(temp, c_int32_t)
    end if
  end function

  function add_multi_ui64(values, n, overflowed) bind(C, name="add_multi_ui64") result(r)
    integer(c_int32_t), value :: n
    integer(c_int64_t), dimension(n), intent(in) :: values
    integer(c_int32_t), intent(out) :: overflowed
    integer(c_int64_t) :: r
    integer(c_int64_t) :: prev
    integer :: i

    r = 0
    overflowed = 0

    do i = 1, n
      prev = r
      r = r + values(i)
      ! Unsigned overflow: result wraps around
      if (r < prev) then
        overflowed = 1
        return
      end if
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
  ! LEGACY FUNCTIONS (kept for compatibility)
  !========================================

  function add_i8(a, b) bind(C, name="add_i8") result(r)
    integer(c_int8_t), value :: a, b
    integer(c_int8_t) :: r
    r = a + b
  end function

  function add_double(a, b) bind(C, name="add_double") result(r)
    real(c_double), value :: a, b
    real(c_double) :: r
    r = a + b
  end function

  function add_i8_multi(values, n) bind(C, name="add_i8_multi") result(r)
    integer(c_int), value :: n
    integer(c_int8_t), dimension(n) :: values
    integer(c_int8_t) :: r
    integer :: i

    r = 0
    do i = 1, n
      r = r + values(i)
    end do
  end function

end module
