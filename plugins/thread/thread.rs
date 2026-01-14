// Implement threading :3
use std::os::raw::c_int;
use std::thread;

#[no_mangle]
pub extern "C" fn spawn
