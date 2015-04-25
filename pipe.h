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

/* wrappers around pipes
 */

#ifndef PIPE_H
#define PIPE_H 1

#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>
#include <utility>
#include <cassert>
#include <stdio.h>
#include "fd.h"

template<class T> class ReadPipe;
template<class T> class WritePipe;

template<class T>
using PipePair = std::pair<ReadPipe<T>, WritePipe<T> >;

template<class T> PipePair<T> mkpipe(void);

template<class T>
class ReadPipe : public FD {
public:
    ReadPipe(ReadPipe && other) : FD(std::move(other)) { }

    ReadPipe & operator = (ReadPipe && other) {
	FD::operator =(std::move(other));
	return *this;
    }

    ~ReadPipe(void) { }

    T * read(void) {
	T * res = nullptr;
	FD::read(&res, sizeof(res));
	return res;
    }

    ReadPipe dup(void) const {
	return FD::dup();
    }

private:
    ReadPipe(int fd) : FD(fd) { }
    ReadPipe(FD fd) : FD(std::move(fd)) { }

    friend PipePair<T> mkpipe<T>(void);
};

template<class T>
class WritePipe : public FD {
public:
    WritePipe(WritePipe && other) : FD(std::move(other)) { }

    WritePipe & operator = (WritePipe && other) {
	FD::operator =(std::move(other));
	return *this;
    }

    ~WritePipe(void) { }

    void write(T * t) {
	FD::write(&t, sizeof(t));
    }

    WritePipe dup(void) const {
	return WritePipe(FD::dup());
    }
private:
    WritePipe(int fd) : FD(fd) { }
    WritePipe(FD fd) : FD(std::move(fd)) { }
    
    friend PipePair<T> mkpipe<T>(void);
};

template<class T>
PipePair<T> mkpipe(void) {
    int pipefd[2];
    int res = pipe2(pipefd, O_CLOEXEC);
    if (res == -1) {
	perror(__PRETTY_FUNCTION__);
	assert(false);
    }
    return std::make_pair(ReadPipe<T>(pipefd[0]), WritePipe<T>(pipefd[1]));
}

#endif // #ifndef PIPE_H
