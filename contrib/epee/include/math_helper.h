// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 




#pragma once


#include <list>
#include <numeric>
#include <random>
#include <boost/timer/timer.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>

#include "syncobj.h"
#include "time_helper.h"

namespace epee
{
namespace math_helper
{

	template<typename val, int default_base>
	class average
	{
	public:

		average()
		{
			m_base = default_base;
			m_last_avg_val = 0;
		}

		bool set_base()
		{
			CRITICAL_REGION_LOCAL(m_lock);

			m_base = default_base;
			if(m_list.size() > m_base)
				m_list.resize(m_base);

      return true;
		}

		typedef val value_type;

		void push(const value_type& vl)
		{
			CRITICAL_REGION_LOCAL(m_lock);

//#ifndef DEBUG_STUB
			m_list.push_back(vl);
			if(m_list.size() > m_base )
				m_list.pop_front();
//#endif
		}

		double update(const value_type& vl)
		{
			CRITICAL_REGION_LOCAL(m_lock);
//#ifndef DEBUG_STUB
			push(vl);
//#endif

			return get_avg();
		}

		double get_avg()
		{
			CRITICAL_REGION_LOCAL(m_lock);

			value_type vl = std::accumulate(m_list.begin(), m_list.end(), value_type(0));
			if(m_list.size())
				return m_last_avg_val = (double)(vl/m_list.size());
	
			return m_last_avg_val = (double)vl;
		}

		value_type get_last_val()
		{
			CRITICAL_REGION_LOCAL(m_lock);
			if(m_list.size())
				return m_list.back();

			return 0;
		}

	private:
		unsigned int m_base;
		double m_last_avg_val;
		std::list<value_type> m_list;
		critical_section m_lock;
	};

	
#ifdef WINDOWS_PLATFORM
	
	/************************************************************************/
	/*                                                                      */
	/************************************************************************/
	class timing_guard_base
	{
	public:
		virtual ~timing_guard_base(){};
	};

	template<class T>
	class timing_guard: public timing_guard_base
	{
	public:
		timing_guard(T& avrg):m_avrg(avrg)
		{
			m_start_ticks = ::GetTickCount();
		}

		~timing_guard()
		{
			m_avrg.push(::GetTickCount()-m_start_ticks);
		}

	private:
		T& m_avrg;
		DWORD m_start_ticks;
	};

	template<class t_timing>
	timing_guard_base* create_timing_guard(t_timing&  timing){return new timing_guard<t_timing>(timing);}

#define BEGIN_TIMING_ZONE(timing_var) {		boost::shared_ptr<math_helper::timing_guard_base> local_timing_guard_ptr(math_helper::create_timing_guard(timing_var));
#define END_TIMING_ZONE()			  }
#endif

//#ifdef WINDOWS_PLATFORM_EX
	template<uint64_t default_time_window>
	class speed
	{
	public:

		speed()
		{
			m_time_window = default_time_window;
			m_last_speed_value = 0;
		}
		bool chick()
		{
#ifndef DEBUG_STUB
			uint64_t ticks = misc_utils::get_tick_count();
			CRITICAL_REGION_BEGIN(m_lock);
			m_chicks.push_back(ticks);
			CRITICAL_REGION_END();
			//flush(ticks);
#endif
			return true;			
		}

		bool chick(size_t count)
		{
			for(size_t s = 0; s != count; s++)
				chick();

			return true;			
		}


		size_t get_speed()
		{
			flush(misc_utils::get_tick_count());
			return m_last_speed_value = m_chicks.size();
		}
	private:

		bool flush(uint64_t ticks)
		{
			CRITICAL_REGION_BEGIN(m_lock);
			std::list<uint64_t>::iterator it = m_chicks.begin();
			while(it != m_chicks.end())
			{
				if(*it + m_time_window < ticks)
					m_chicks.erase(it++);
				else
					break;
			}
			CRITICAL_REGION_END();
			return true;
		}

		std::list<uint64_t> m_chicks;
		uint64_t m_time_window;
		size_t m_last_speed_value;
		critical_section m_lock;
	};
//#endif

	template<class tlist>
	void randomize_list(tlist& t_list)
	{
		for(typename tlist::iterator  it = t_list.begin();it!=t_list.end();it++)
		{
			size_t offset = rand()%t_list.size();
			typename tlist::iterator  it_2 = t_list.begin();
			for(size_t local_offset = 0;local_offset!=offset;local_offset++)
				it_2++;
			if(it_2 == it)
				continue;
			std::swap(*it_2, *it);
		}

	}
	template<typename get_interval, bool start_immediate = true>
	class once_a_time
	{
    uint64_t get_time() const
    {
#ifdef _WIN32
      FILETIME fileTime;
      GetSystemTimeAsFileTime(&fileTime);
      unsigned __int64 present = 0;
      present |= fileTime.dwHighDateTime;
      present = present << 32;
      present |= fileTime.dwLowDateTime;
      present /= 10;  // mic-sec
      return present;
#else
      struct timeval tv;
      gettimeofday(&tv, NULL);
      return tv.tv_sec * 1000000 + tv.tv_usec;
#endif
    }

    void set_next_interval()
    {
      m_interval = get_interval()();
    }

	public:
		once_a_time()
		{
			m_last_worked_time = 0;
      if(!start_immediate)
        m_last_worked_time = get_time();
      set_next_interval();
		}

    void trigger()
    {
      m_last_worked_time = 0;
    }

		template<class functor_t>
		bool do_call(functor_t functr)
		{
			uint64_t current_time = get_time();

      if(current_time - m_last_worked_time > m_interval)
			{
				bool res = functr();
				m_last_worked_time = get_time();
        set_next_interval();
				return res;
			}
			return true;
		}

	private:
		uint64_t m_last_worked_time;
		uint64_t m_interval;
	};

  template<uint64_t N> struct get_constant_interval { public: uint64_t operator()() const { return N; } };

  template<int default_interval, bool start_immediate = true>
  class once_a_time_seconds: public once_a_time<get_constant_interval<default_interval * (uint64_t)1000000>, start_immediate> {};
  template<int default_interval, bool start_immediate = true>
  class once_a_time_milliseconds: public once_a_time<get_constant_interval<default_interval * (uint64_t)1000>, start_immediate> {};
  template<typename get_interval, bool start_immediate = true>
  class once_a_time_seconds_range: public once_a_time<get_interval, start_immediate> {};
}
}
