! math stuffs in Fortran :)

module linear_algebra
  use iso_c_binding
  implict none

  type, bind(C) :: PluginFunctionEntry
    type(c_ptr)    :: name
    type(c_funptr) :: function
  end type

  type, bind(C) :: PluginInterface
    type(c_ptr)    :: name
    type(c_ptr)    :: version
    type(c_funptr) :: init
    type(c_funptr) :: get_functions
    type(c_funptr) :: cleanup
  end type

contains

  function math_init() bind(c) result(ok)
    integer(c_int) :: ok
    ok = 1
  end function

  subroutine math_cleanup() bind(c)
    ! nothing :)
  end subroutine


  ! =========
  ! Functions
  ! =========

  ! When possible, do array.add(),
  ! so we can organize the math based on types
  !
  ! We could also do add.array()
  function array_add(args) bind(c) result(ret)
    real :: ret = 1
  end function

  ! ============================
  ! Function table
  ! ============================

  type(PluginFunctionEntry), target :: functions(2) = [ &
    PluginFunctionEntry( c_loc(c_null_char // "addArray"), c_funloc(array_add) ), &
    PluginFunctionEntry( c_null_ptr, c_null_funptr ) &
  ]

  function get_functions() bind(C) result(p)
    type(c_ptr) :: p
    p = c_loc(functions)
  end function

  ! ============================
  ! Plugin interface
  ! ============================

  type(PluginInterface), target :: iface = PluginInterface( &
    c_loc(c_null_char // "math"), &
    c_loc(c_null_char // "0.1.0"), &
    c_funloc(plugin_init), &
    c_funloc(get_functions), &
    c_funloc(plugin_cleanup) &
  )

end module

! ==============================
! Required exported symbol
! ==============================
function get_plugin_interface() bind(C) result(p)
  use iso_c_binding
  use bare_plugin
  type(c_ptr) :: p
  p = c_loc(iface)
end function
