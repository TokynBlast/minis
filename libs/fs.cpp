/*
 * fs.cpp - a filesystem interaction library for the Minis language
 * Copyright (C) 2026 Ruri / Mei
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <string>
#include "../fast_io/include/fast_io.h"

using fast_io::io::perr;
using fast_io::io::print;
using fast_io::io::scan;

extern "C" {
class fs {
  bool exist() {}
  // return a FILE pointer
  bool open(std::string filename) {
    fast_io::native_file_loader file_data(file_name);
    return file_data;
  }
}
}
