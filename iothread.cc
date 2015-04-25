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

/* iothread using libaio
 */

#include "iothread.h"
#include <sys/select.h>
#include "eventfd.h"
#include <algorithm>

// #include <libaio.h>
// #include <stdio.h>
// #include <sys/eventfd.h>
// #include <stdint.h>

IOThread::IOThread(int max_events, ReadPipe<IOCB> in, WritePipe<IOCB> out)
    : ctx_(max_events), in_(std::move(in)), out_(std::move(out)),
      thread_(&IOThread::run, this) { }

IOThread::~IOThread() {
    thread_.join();
}

void IOThread::run(void) {
    EventFD e;
    int efd = e.fd();
    int infd = in_.fd();
    int nfds = std::max(efd, infd) + 1;
    int pending = 0;

    while (in_ || (pending > 0)) {
	// keep submitting IOCBs till max_events
	// break if events are pending and nothing to submitt
	while (in_ && (pending < ctx_.max_events())) {
	    fd_set set;
	    FD_ZERO(&set);
	    FD_SET(efd, &set);
	    FD_SET(infd, &set);
	    int res = select(nfds, &set, nullptr, nullptr, nullptr);
	    if (res == -1) {
		if (errno == EINTR) continue;
		perror(__PRETTY_FUNCTION__);
		assert(false);
	    }
	    assert(res > 0);

	    if (FD_ISSET(infd, &set)) {
		IOCB * iocb = in_.read();
		if (iocb == nullptr) {
		    assert(!in_);
		    break;
		}
		struct iocb *p = iocb->iocb();
		io_set_eventfd(p, efd);
		ctx_.submit(1, &p);
		++pending;
	    } else {
		// nothing to submit but events pending
		break;
	    }
	}
	if (pending > 0) {
	    uint64_t num_events = e.read();
	    struct io_event event[num_events];
	    int res = ctx_.getevents(num_events, num_events, event);
	    assert(res >= 0);
	    assert(uint64_t(res) == num_events);
	    pending -= res;
	    for (int i = 0; i < res; ++i) {
		IOCB * iocb = (IOCB *)event[i].obj->data;
		if ((event->res != iocb->size()) || (event->res2 != 0)) {
		    fprintf(stderr, "res = %lx, res2 = %ld at offset %lx\n",
			    event->res, event->res2, iocb->offset());
		    assert(false);
		}
		out_.write(iocb);
	    }
	}
    }
    out_.close();
}

