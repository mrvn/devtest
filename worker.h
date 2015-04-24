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

#include <queue>
#include <mutex>
#include <condition_variable>

template<class T>
class Worker {
public:
    Worker(MultiQueue<T> & in, MultiQueue<T> & out)
	: in_(in), out_(out), thread_(&Worker::run, this) { }

    ~Worker() {
	if (thread_.joinable())
	    thread_.join();
    }
    
private:
    Worker(Worker &&) = delete;
    Worker & operator =(Worker &&) = delete;

    void run(void) {
	while (true) {
	    T * job = in_.pop();
	    if (job == &T::QUIT) {
		// put it back for other workers and quit
		in_.push(std::move(job));
		break;
	    }
	    (*job)();
	    out_.push(job);
	}
    }

    MultiQueue<T> & in_;
    MultiQueue<T> & out_;
    std::thread thread_;
};

#endif // #ifndef WORKER_H
