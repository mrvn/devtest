/* Copyright (C) 2015 Goswin von Brederlow <goswin-v-b@web.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* file wrapper
 */

#include "file.h"
#include <stdio.h>
#include <fcntl.h>
#include <cassert>
#include <sys/types.h>
#include <unistd.h>

File::File(const char * name) {
//    fd_ = open(".", O_RDWR | O_CLOEXEC | O_TMPFILE | O_EXCL | O_DIRECT,
//	       S_IRUSR | S_IWUSR);
//	fd_ = open("/tmp", O_RDWR | O_CLOEXEC | O_TMPFILE | O_EXCL,
//		   S_IRUSR | S_IWUSR);
    fd_ = open(name, O_RDWR | O_CLOEXEC | O_DIRECT);
    if (fd_ == -1) {
	perror("File(): open()");
	assert(false);
    }
    /*
    int res = fallocate(fd_, 0, 0, size);
    if (res == -1) {
	perror("File(): fallocate()");
	assert(false);
    }
    */
    size_ = lseek(fd_, 0, SEEK_END);
    assert(size_ > 0);
}

File::~File() {
    int res = close(fd_);
    if (res != 0) {
	perror("~File(): close()");
	assert(false);
    }
}
