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

/* libaio context wrapper
 */

#include "context.h"
#include <stdio.h>
#include <errno.h>
#include <cassert>

Context::Context(int max_events) : ctx_(0), max_events_(max_events) {
    int res = io_queue_init(max_events, &ctx_);
    if (res < 0) {
	fprintf(stderr, "%s: io_queue_init(): %s\n",
		__PRETTY_FUNCTION__, strerror(-res));
	assert(false);
    }
}

Context::~Context() {
    int res = io_queue_release(ctx_);
    if (res != 0) {
	fprintf(stderr, "%s: io_queue_release(): %s\n",
		__PRETTY_FUNCTION__, strerror(-res));
	assert(false);
    }
}

void Context::submit(int nr, struct iocb *iocbp[]) {
    int done = 0;
    while (done < nr) {
	int res = io_submit(ctx_, nr - done, &iocbp[done]);
	if (res < 0) {
	    fprintf(stderr, "%s: io_submit(): %s\n",
		    __PRETTY_FUNCTION__, strerror(-res));
	    assert(false);
	}
	done += res;
	if (done != nr) {
	    fprintf(stderr, "%s: io_submit() was short: %d < %d\n",
		    __PRETTY_FUNCTION__, done, nr);
	}
    }
}

int Context::getevents(int min_nr, int nr, struct io_event *events) {
    while (true) {
	int res = io_getevents(ctx_, min_nr, nr, events, nullptr);
	if (res < 0) {
	    if (errno == EINTR) continue;
	    fprintf(stderr, "%s: io_getevents(): %s\n",
		    __PRETTY_FUNCTION__, strerror(-res));
	    assert(false);
	}
	return res;
    }
}
