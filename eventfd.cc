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

/* eventfd wrapper
 */

#include "eventfd.h"
#include <sys/eventfd.h>
#include <utility>
#include <cassert>

EventFD::EventFD() : FD(eventfd(0, EFD_CLOEXEC)) { }

EventFD::EventFD(EventFD && other) : FD(std::move(other)) { }

EventFD::~EventFD() { }

EventFD & EventFD::operator = (EventFD && other) {
    FD::operator =(std::move(other));
    return *this;
}

uint64_t EventFD::read() {
    assert(fd_ != -1);
    uint64_t res = 0;
    FD::read(&res, sizeof(res));
    return res;
}

void EventFD::write(uint64_t t) {
    FD::write(&t, sizeof(t));
}

