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

/* wrappers around file descriptor
 */

#include "fd.h"
#include <stdio.h>
#include <errno.h>
#include <cassert>

FD::~FD() {
    if (fd_ != -1) {
	int res = ::close(fd_);
	if (res != 0) {
	    perror(__PRETTY_FUNCTION__);
	}
    }
    fd_ = -1;
}

FD::FD(FD && other) : fd_(other.fd_) {
    assert(fd_ != -1);
    other.fd_ = -1;
}

void FD::close() {
    assert(fd_ != -1);
    int res = ::close(fd_);
    assert(res == 0);
    fd_ = -1;
}

ssize_t FD::read(void *buf, size_t size) {
    assert(fd_ != -1);
    while (true) {
	ssize_t len = ::read(fd_, buf, size);
	if (len == -1) {
	    if (errno == EINTR) continue;
	    perror(__PRETTY_FUNCTION__);
	    assert(false);
	}
	if (len == 0) {
	    close();
	    return 0;
	}
	assert(size_t(len) == size);
	    return len;
    }
}

void FD::write(void *buf, size_t size) {
    assert(fd_ != -1);
    while (true) {
	ssize_t len = ::write(fd_, buf, size);
	if (len == -1) {
	    if (errno == EINTR) continue;
	    perror(__PRETTY_FUNCTION__);
	    assert(false);
	}
	assert(size_t(len) == size);
	return;
    }
}

FD::FD(int fd) : fd_(fd) { }
    
FD & FD::operator =(FD && other) {
    assert(fd_ == -1);
    assert(other.fd_ != -1);
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
}

FD FD::dup() const {
    assert(fd_ != -1);
    int t = ::dup(fd_);
    assert(t != -1);
    return FD(t);
}
