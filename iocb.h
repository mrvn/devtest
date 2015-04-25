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

#ifndef IOCB_H
#define IOCB_H 1

#include <libaio.h>
#include <cassert>

class File;

class IOCB {
public:
    enum Kind {
	READ,
	WRITE,
    };

    enum State {
	BLANK,
	PREPPED,
	FILLED,
	SUBMITTED,
	DEAD,
	MOVED,
    };

    IOCB(File &file, Kind kind, size_t size);
    ~IOCB();

    void offset(off_t o) {
	assert(state_ == BLANK);
	iocb_.u.c.offset = o;
	state_ = PREPPED;
    }

    off_t offset() const {
	return iocb_.u.c.offset;
    }

    size_t size() const {
	return iocb_.u.c.nbytes;
    }

    struct iocb * iocb(void) {
	assert(state_ == FILLED);
	state_ = SUBMITTED;
	return &iocb_;
    }

    static IOCB * iocb(struct iocb *obj) {
	IOCB * res = (IOCB *)obj->data;
	return res;
    }
    
    void fill();
    void check();
    void operator()(void);
private:
    IOCB(IOCB &&) = delete;
    IOCB & operator =(IOCB &&) = delete;

    struct iocb iocb_;
    void *buf_;
    State state_;
};

#endif // #ifndef IOCB_H
