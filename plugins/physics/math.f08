module physics
  use iso_c_binding

  ! Let's us use double equivilant
  use, intrinsic :: iso_fortran_env
  implicit none
contains
  ! Need m/s^2
  real(real64) :: g = 9.8
end module physics
