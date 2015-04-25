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

/* struct iocb wrapper
 */

#include "iocb.h"
#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include "file.h"

enum {
    BLOCK_ALIGN = 4096,
};

static constexpr const char * STATE[] = {
    "BLANK",
    "PREPPED",
    "FILLED",
    "SUBMITTED",
    "DEAD",
    "MOVED",
};

IOCB::IOCB(File &file, Kind kind, size_t size)
    : buf_(aligned_alloc(BLOCK_ALIGN, size)), state_(BLANK) {
    assert(size % sizeof(off_t) == 0);
    if (buf_ == nullptr) {
	fprintf(stderr, "%s: aligned_alloc() failed\n",
		__PRETTY_FUNCTION__);
	exit(1);
    }
    if (kind == READ) {
	io_prep_pread(&iocb_, file.fd(), buf_, size, 0);
    } else {
	io_prep_pwrite(&iocb_, file.fd(), buf_, size, 0);
    }
    iocb_.data = this;
}

IOCB::~IOCB() {
    if ((state_ != BLANK) && (state_ != MOVED)) {
	fprintf(stderr, "%s: state = %s\n",
		__PRETTY_FUNCTION__, STATE[state_]);
	assert(false);
    }
    free(buf_);
    state_ = DEAD;
    buf_ = nullptr;
    memset(&iocb_, 0xEE, sizeof(iocb_));
}

void IOCB::fill() {
    assert(state_ == PREPPED);
    off_t o = iocb_.u.c.offset;
    if (iocb_.aio_lio_opcode == IO_CMD_PWRITE) {
	off_t *p = (off_t *)buf_;
	off_t *q = (off_t *)(uintptr_t(buf_) + iocb_.u.c.nbytes);
	while (p < q) {
	    *p = o;
	    ++p;
	    o += sizeof(off_t);
	}
    }
    state_ = FILLED;
}
    
void IOCB::check() {
    if (iocb_.aio_lio_opcode == IO_CMD_PREAD) {
	assert(state_ == SUBMITTED);
	off_t *p = (off_t *)buf_;
	off_t *q = (off_t *)(uintptr_t(buf_) + iocb_.u.c.nbytes);
	off_t o = iocb_.u.c.offset;
	while (p < q) {
	    if (*p != o) {
		fprintf(stderr,
			"Read error in block at %#llx: expected %#lx, got %#lx\n",
			iocb_.u.c.offset + uintptr_t(p) - uintptr_t(buf_),
			o, *p);
	    }
	    ++p;
	    o += sizeof(off_t);
	}
    } else {
	assert((state_ == SUBMITTED) || (state_ == BLANK));
    }
    state_ = BLANK;
}

void IOCB::operator()(void) {
    switch(state_) {
    case PREPPED: fill(); break;
    case SUBMITTED: check(); break;
    default: assert(false);
    }
}
