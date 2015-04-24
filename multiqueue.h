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

/* Thread safe multiple producer / multiple consumer queue
 */

#ifndef MULTIQUEUE_H
#define MULTIQUEUE_H 1

#include <queue>
#include <mutex>
#include <condition_variable>

template<class T>
class MultiQueue : private std::queue<T *> {
public:
    using Q = std::queue<T *>;

    MultiQueue() { }

    void push(T * t) {
	{
	    std::lock_guard<std::mutex> lock(mutex_);
	    Q::push(t);
	}
	notifier_.notify_one();
    }
    
    T * pop(void) {
	std::unique_lock<std::mutex> lock(mutex_);

	notifier_.wait(lock, [this] { return !Q::empty(); });
	T * t = Q::front();
	Q::pop();
	return t;
    }

    bool empty(void) {
	std::unique_lock<std::mutex> lock(mutex_);
	return Q::empty();
    }
private:
    MultiQueue(MultiQueue &&) = delete;
    MultiQueue & operator =(MultiQueue &&) = delete;

    std::mutex mutex_;
    std::condition_variable notifier_;
};

#endif // #ifndef MULTIQUEUE_H
