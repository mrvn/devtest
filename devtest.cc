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

/* test device for bad blocks and data corruption
 */

#include <libaio.h>
#include <cstdint>
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <stdlib.h>
#include <thread>
#include <stdio.h>

#include "multiqueue.h"
#include "worker.h"

enum {
    MAX_EVENTS = 1024,
    BLOCK_SIZE = 128 * 1024,
    BLOCK_ALIGN = 4096,
    EXTRA_EVENTS = 16,
};

enum {
//    FILE_SIZE = 2LLU * 1024 * 1024 * 1024,
    FILE_SIZE = 256LLU * 1024 * 1024,
};

class File {
public:
    File(off_t size) : size_(size) {
	fd_ = open(".", O_RDWR | O_CLOEXEC | O_TMPFILE | O_EXCL | O_DIRECT,
		   S_IRUSR | S_IWUSR);
//	fd_ = open("/tmp", O_RDWR | O_CLOEXEC | O_TMPFILE | O_EXCL,
//		   S_IRUSR | S_IWUSR);
	if (fd_ == -1) {
	    perror("File(): open()");
	    exit(1);
	}
	int res = fallocate(fd_, 0, 0, size);
	if (res == -1) {
	    perror("File(): fallocate()");
	    exit(1);
	}
    }

    ~File() {
	int res = close(fd_);
	if (res != 0) {
	    perror("~File(): close()");
	    exit(1);
	}
    }

    int fd() const { return fd_; }
    off_t size() const { return size_; }
private:
    File(File &&) = delete;
    File & operator =(File &&) = delete;
    off_t size_;
    int fd_;
};

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

    static constexpr const char * STATE[] = {
	"BLANK",
	"PREPPED",
	"FILLED",
	"SUBMITTED",
	"DEAD",
	"MOVED",
    };

    IOCB(File &file, Kind kind, size_t size)
	: buf_(aligned_alloc(BLOCK_ALIGN, size)), state_(BLANK) {
	// std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << std::endl;
	assert(size % sizeof(off_t) == 0);
    	if (buf_ == nullptr) {
	    std::cerr << __PRETTY_FUNCTION__ << ": aligned_alloc() failed\n";
	    exit(1);
	}
	if (kind == READ) {
	    io_prep_pread(&iocb_, file.fd(), buf_, size, 0);
	} else {
	    io_prep_pwrite(&iocb_, file.fd(), buf_, size, 0);
	}
	iocb_.data = this;
    }

    ~IOCB() {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
	if ((state_ != BLANK) && (state_ != MOVED)) {
	    std::cerr << __PRETTY_FUNCTION__ << ": state = " << STATE[state_]
		      << std::endl;
	    assert(false);
	}
	free(buf_);
	state_ = DEAD;
	buf_ = nullptr;
	memset(&iocb_, 0xEE, sizeof(iocb_));
    }

    void offset(off_t o) {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
	assert(state_ == BLANK);
	iocb_.u.c.offset = o;
	state_ = PREPPED;
    }
    
    void fill() {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
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
    
    struct iocb * iocb(void) {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
	assert(state_ == FILLED);
	state_ = SUBMITTED;
	return &iocb_;
    }

    static IOCB * iocb(struct iocb *obj) {
	IOCB * res = (IOCB *)obj->data;
	//std::cerr << __PRETTY_FUNCTION__ << ": res = " << res << ": state = " << STATE[res->state_] << std::endl;
	return res;
    }
    
    void check() {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
	assert(state_ == SUBMITTED);
	if (iocb_.aio_lio_opcode == IO_CMD_PREAD) {
	   	    off_t *p = (off_t *)buf_;
	    off_t *q = (off_t *)(uintptr_t(buf_) + iocb_.u.c.nbytes);
	    off_t o = iocb_.u.c.offset;
	    while (p < q) {
		if (*p != o) {
		    std::cerr << std::hex
			      << "Read error in block at "
			      << (iocb_.u.c.offset + uintptr_t(p)
				  - uintptr_t(buf_))
			      << " expected " << o
			      << " got " << *p << std::dec
			      << std::endl;
		}
		++p;
		o += sizeof(off_t);
	    }
	}
	state_ = BLANK;
    }

    void operator()(void) {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
	switch(state_) {
	case PREPPED: fill(); break;
	case SUBMITTED: check(); break;
	default: assert(false);
	}
    }
    
    static IOCB QUIT;
private:
    explicit IOCB() : buf_(nullptr), size_(0), state_(BLANK) {
	//std::cerr << __PRETTY_FUNCTION__ << ": this = " << this << ": state = " << STATE[state_] << std::endl;
	memset(&iocb_, 0xEF, sizeof(iocb_));
    }

    IOCB(IOCB &&) = delete;
    IOCB & operator =(IOCB &&) = delete;

    struct iocb iocb_;
    void *buf_;
    size_t size_;
    State state_;
};

constexpr const char * IOCB::STATE[];
IOCB IOCB::QUIT;

class Context {
public:
    Context(int max_events) : ctx_(0), max_events_(max_events) {
	int res = io_queue_init(max_events, &ctx_);
	if (res < 0) {
	    perror("Context(): io_queue_init()");
	    std::cerr << "res = " << res << std::endl;
	    exit(1);
	}
    }

    ~Context() {
	int res = io_queue_release(ctx_);
	if (res != 0) {
	    perror("~Context(): io_queue_release()");
	    exit(1);
	}
    }

    int max_events() const { return max_events_; }

    void submit(int nr, struct iocb *iocbp[]) {
	int done = 0;
	while (done < nr) {
	    int res = io_submit(ctx_, nr - done, &iocbp[done]);
	    if (res < 0) {
		std::cerr << "Context(): io_submit(): " << strerror(-res)
			  << std::endl;
		std::cerr << "res = " << res
			  << ", iocbp[done] = " << iocbp[done] << std::endl;
		char *p = (char*)(iocbp[done]);
		std::cerr << "iocbp[done] @ " << (void*)p << std::endl;
		for(size_t i = 0; i < sizeof(struct iocb); ++i) {
		    fprintf(stderr, "%02x", (int)(unsigned char)*p++);
		    if (i % 4 == 3) fprintf(stderr, " ");
		    if (i % 16 == 15) fprintf(stderr, "\n");
		}
		fprintf(stderr, "\n");
		exit(1);
	    }
	    done += res;
	    if (done != nr) {
		std::cerr << "Context(): io_submit() was short: " << done
			  << " < " << nr << std::endl;
	    }
	}
    }

    int getevents(int min_nr, int nr, struct io_event *events) {
	int res = io_getevents(ctx_, min_nr, nr, events, nullptr);
	if (res < 0) {
	    perror("~Context(): io_getevents()");
	    exit(1);
	}
	return res;
    }
private:
    io_context_t ctx_;
    int max_events_;
};

class Engine {
public:
    Engine(File &file, int num_worker)
	: file_(file), offset_(0), needed_(file_.size()) {
	for (int i = 0; i < num_worker; ++i) {
	    worker_.emplace_back(new Worker<IOCB>(work_, done_));
	}
    }

    ~Engine() {
	work_.push(&IOCB::QUIT);
	for (std::vector<Worker<IOCB> *>::iterator it = worker_.begin();
	     it != worker_.end(); ++it) {
	    delete *it;
	}
    }

    void write(Context &ctx) {
	off_t offset = 0;
	off_t submitted = 0;
	off_t completed = 0;
	off_t size = file_.size();
	struct iocb *iocbp[ctx.max_events()];
	struct io_event event[ctx.max_events()];

	// prepare IOCBs
	for (int i = 0; i < ctx.max_events() + EXTRA_EVENTS; ++i) {
	    IOCB *iocb = new IOCB(file_, IOCB::WRITE, BLOCK_SIZE);
	    iocb->offset(offset);
	    offset += BLOCK_SIZE;
	    work_.push(iocb);
	}

	assert(offset < size);
	int pending = 0;
	while (submitted < size) {
	    // submit new work
	    int i;
	    int needed = ctx.max_events() - pending;
	    for (i = 0; i < needed; ++i) {
		// don't block unless nothing is pending
		if (done_.empty() && (i + pending != 0)) break;
		IOCB * iocb = done_.pop();
		iocbp[i] = iocb->iocb();
	    }
	    ctx.submit(i, iocbp);
	    pending += i;
	    submitted += i * BLOCK_SIZE;

	    // reap completed events
	    int num_events = ctx.getevents(1, ctx.max_events(), event);
	    pending -= num_events;
	    completed += num_events * BLOCK_SIZE;
	    std::cerr << completed
		      << ": num_events = " << num_events
		      << ", pending = " << pending << "   \n";
	    for (i = 0; i < num_events; ++i) {
		struct iocb *p = event[i].obj;
		if ((event->res != BLOCK_SIZE) || (event->res2 != 0)) {
		    std::cerr << "res = " << event->res
			      << ", res2 = " << event->res2
			      << " at offset = " << p->u.c.offset
			      << std::endl;
		    exit(1);
		}
		// schedule background filling for next block
		IOCB * iocb = IOCB::iocb(p);
		iocb->check();
		if (offset < size) {
		    iocb->offset(offset);
		    offset += BLOCK_SIZE;
		    work_.push(iocb);
		} else {
		    delete iocb;
		}
	    }
	}
	// reap remaining events
	while (pending > 0) {
	    int num_events = ctx.getevents(pending, ctx.max_events(), event);
	    pending -= num_events;
	    completed += num_events * BLOCK_SIZE;
	    std::cerr << completed
		      << ": num_events = " << num_events
		      << ", pending = " << pending << "   \n";
	    for (int i = 0; i < num_events; ++i) {
		struct iocb *p = event[i].obj;
		if ((event->res != BLOCK_SIZE) || (event->res2 != 0)) {
		    std::cerr << "res = " << event->res
			      << ", res2 = " << event->res2
			      << " at offset = " << p->u.c.offset
			      << std::endl;
		    exit(1);
		}
		IOCB * iocb = IOCB::iocb(p);
		iocb->check();
		delete iocb;
	    }
	}
	assert(work_.empty());
	assert(done_.empty());
    }

    void read(Context &ctx) {
	off_t offset = 0;
	off_t completed = 0;
	off_t size = file_.size();
	struct iocb *iocbp[ctx.max_events()];
	struct io_event event[ctx.max_events()];

	// prepare IOCBs
	for (int i = 0; i < ctx.max_events() + EXTRA_EVENTS; ++i) {
	    IOCB *iocb = new IOCB(file_, IOCB::READ, BLOCK_SIZE);
	    done_.push(iocb);
	}

	assert(offset < size);
	int pending = 0;
	while (offset < size) {
	    // submit new work
	    int i;
	    int needed = ctx.max_events() - pending;
	    for (i = 0; (i < needed) && (offset < size); ++i) {
		// don't block unless nothing is pending
		if (done_.empty() && (i + pending != 0)) break;
		IOCB * iocb = done_.pop();
		iocb->offset(offset);
		offset += BLOCK_SIZE;
		iocb->fill();
		iocbp[i] = iocb->iocb();
	    }
	    ctx.submit(i, iocbp);
	    pending += i;

	    // reap completed events
	    int num_events = ctx.getevents(1, ctx.max_events(), event);
	    pending -= num_events;
	    completed += num_events * BLOCK_SIZE;
	    std::cerr << completed
		      << ": num_events = " << num_events
		      << ", pending = " << pending << "   \n";
	    for (i = 0; i < num_events; ++i) {
		struct iocb *p = event[i].obj;
		if ((event->res != BLOCK_SIZE) || (event->res2 != 0)) {
		    std::cerr << "res = " << event->res
			      << ", res2 = " << event->res2
			      << " at offset = " << p->u.c.offset
			      << std::endl;
		    exit(1);
		}
		// schedule background checking
		IOCB * iocb = IOCB::iocb(p);
		work_.push(iocb);
	    }
	}
	// reap remaining events
	while (pending > 0) {
	    int num_events = ctx.getevents(pending, ctx.max_events(), event);
	    pending -= num_events;
	    completed += num_events * BLOCK_SIZE;
	    std::cerr << completed
		      << ": num_events = " << num_events
		      << ", pending = " << pending << "   \n";
	    for (int i = 0; i < num_events; ++i) {
		struct iocb *p = event[i].obj;
		if ((event->res != BLOCK_SIZE) || (event->res2 != 0)) {
		    std::cerr << "res = " << event->res
			      << ", res2 = " << event->res2
			      << " at offset = " << p->u.c.offset
			      << std::endl;
		    exit(1);
		}
		IOCB * iocb = IOCB::iocb(p);
		work_.push(iocb);
	    }
	}

	// reap IOCBs
	for (int i = 0; i < ctx.max_events() + EXTRA_EVENTS; ++i) {
	    IOCB *iocb = done_.pop();
	    delete iocb;
	}
	assert(work_.empty());
	assert(done_.empty());
    }
private:
    File &file_;
    off_t offset_;
    off_t needed_;
    MultiQueue<IOCB> work_;
    MultiQueue<IOCB> done_;
    std::vector<Worker<IOCB> *> worker_;
};

int main() {
    std::cerr << "devtest V0.0\n";
    Context ctx(MAX_EVENTS);
    File file(FILE_SIZE);
    Engine engine(file, 4);
    std::cerr << "Starting write\n";
    engine.write(ctx);
    std::cerr << "Starting read\n";
    engine.read(ctx);
    std::cerr << "done\n";
}
