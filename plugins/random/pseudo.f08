! Implement the fast pseudo random numbers :)

module random
  use iso_c_binding
  implicit none

  ! MT19937 parameters
  integer, parameter :: N = 624
  integer, parameter :: M = 397
  integer(c_int64_t), parameter :: MATRIX_A = int(z'9908B0DF', c_int64_t)
  integer(c_int64_t), parameter :: UPPER_MASK = int(z'80000000', c_int64_t)
  integer(c_int64_t), parameter :: LOWER_MASK = int(z'7FFFFFFF', c_int64_t)

  ! State array
  integer(c_int64_t), dimension(N) :: mt
  integer :: mti = N + 1

contains

  ! Initialize the generator with a seed
  subroutine mt_init(seed)
    integer(c_int64_t), intent(in) :: seed
    integer :: i

    mt(1) = iand(seed, int(z'FFFFFFFF', c_int64_t))
    do i = 2, N
      mt(i) = iand(1812433253_c_int64_t * ieor(mt(i-1), ishft(mt(i-1), -30)) + i - 1, &
                   int(z'FFFFFFFF', c_int64_t))
    end do
    mti = N
  end subroutine mt_init

  ! Generate a random number
  function mersene_twister(seed) bind(C, name="mersene_twister") result(r)
    integer(c_int64_t), value :: seed
    integer(c_int64_t) :: r
    integer :: kk
    integer(c_int64_t) :: y
    integer(c_int64_t), dimension(2) :: mag01 = [0_c_int64_t, MATRIX_A]

    ! Initialize if needed
    if (mti >= N) then
      call mt_init(seed)
    end if

    ! Generate N words at one time
    if (mti >= N) then
      do kk = 1, N - M
        y = ior(iand(mt(kk), UPPER_MASK), iand(mt(kk+1), LOWER_MASK))
        mt(kk) = ieor(ieor(mt(kk+M), ishft(y, -1)), mag01(iand(y, 1_c_int64_t) + 1))
      end do

      do kk = N - M + 1, N - 1
        y = ior(iand(mt(kk), UPPER_MASK), iand(mt(kk+1), LOWER_MASK))
        mt(kk) = ieor(ieor(mt(kk+M-N), ishft(y, -1)), mag01(iand(y, 1_c_int64_t) + 1))
      end do

      y = ior(iand(mt(N), UPPER_MASK), iand(mt(1), LOWER_MASK))
      mt(N) = ieor(ieor(mt(M), ishft(y, -1)), mag01(iand(y, 1_c_int64_t) + 1))

      mti = 1
    end if

    y = mt(mti)
    mti = mti + 1

    ! Tempering
    y = ieor(y, ishft(y, -11))
    y = ieor(y, iand(ishft(y, 7), int(z'9D2C5680', c_int64_t)))
    y = ieor(y, iand(ishft(y, 15), int(z'EFC60000', c_int64_t)))
    y = ieor(y, ishft(y, -18))

    r = iand(y, int(z'FFFFFFFF', c_int64_t))
  end function mersene_twister

end module random
