/*
Copyright (C) 2008-2011 Vana Development Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#pragma once

#include "Id.h"
#include "../Types.h"
#include <boost/functional/hash.hpp>
#include <boost/tr1/memory.hpp>
#include <boost/tr1/unordered_map.hpp>

namespace Timer {

class Timer;

using std::tr1::shared_ptr;
using std::tr1::unordered_map;

class Container {
public:
	int32_t checkTimer(const Id &id);
	int64_t checkTimer(const Id &id, bool msec);
	void registerTimer(Timer *timer);
	void removeTimer(const Id &id);
private:
	unordered_map<Id, shared_ptr<Timer>, boost::hash<Id> > m_timers;
};

}
