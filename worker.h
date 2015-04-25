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

/* background worker class
 */

#ifndef WORKER_H
#define WORKER_H 1

#include <thread>
#include <vector>
#include "pipe.h"

template<class READ, class WRITE>
class Worker {
public:
    using Read = READ;
    using Write = WRITE;
    Worker(ReadPipe<Read> in, WritePipe<Write> out)
	: in_(std::move(in)), out_(std::move(out)),
	  thread_(&Worker::run, this) { }

    virtual ~Worker(void) {
	thread_.join();
    }

    std::thread::id get_id(void) const {
	return thread_.get_id();
    }

    virtual Write * work(Read * input) = 0;
private:
    Worker(Worker &&) = delete;
    Worker & operator =(Worker &&) = delete;

    void run(void) {
	while (true) {
	    Read * input = in_.read();
	    if (!in_) break;
	    Write * output = work(input);
	    out_.write(output);
	}
	out_.close();
    }

    ReadPipe<Read> in_;
    WritePipe<Write> out_;
    std::thread thread_;
};

template<class W>
class Workers {
public:
    using Read = typename W::Read;
    using Write = typename W::Write;

    Workers(int num, ReadPipe<Read> in, WritePipe<Write> out) {
	while (num-- > 0) {
	    worker_.emplace_back(new W(std::move(in.dup()),
				       std::move(out.dup())));
	}
    }

    ~Workers() {
	for (typename std::vector<W *>::iterator it = worker_.begin();
	     it != worker_.end(); ++it) {
	    delete *it;
	}
    }
private:
    Workers(Workers &&) = delete;
    Workers & operator =(Workers &&) = delete;

    std::vector<W *> worker_;
};

#endif // #ifndef WORKER_H
