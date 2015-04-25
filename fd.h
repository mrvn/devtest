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

#ifndef FD_H
#define FD_H 1

#include <unistd.h>

class FD {
public:
    ~FD();
    FD(FD && other);
    void close();
    int fd(void) const { return fd_; }
    operator bool() const { return fd_ != -1; }
    ssize_t read(void *buf, size_t size);
    void write(void *buf, size_t size);
protected:
    FD(int fd = -1);    
    FD & operator =(FD && other);
    FD dup() const;

    int fd_;
};

#endif // #ifndef FD_H
