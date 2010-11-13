//-----------------------------------------------------------------------------------------
#if 0

Fix8 is released under the New BSD License.

Copyright (c) 2010, David L. Dight <fix@fix8.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of
	 	conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list
	 	of conditions and the following disclaimer in the documentation and/or other
		materials provided with the distribution.
    * Neither the name of the author nor the names of its contributors may be used to
	 	endorse or promote products derived from this software without specific prior
		written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
OR  IMPLIED  WARRANTIES,  INCLUDING,  BUT  NOT  LIMITED  TO ,  THE  IMPLIED  WARRANTIES  OF
MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE ARE  DISCLAIMED. IN  NO EVENT  SHALL
THE  COPYRIGHT  OWNER OR  CONTRIBUTORS BE  LIABLE  FOR  ANY DIRECT,  INDIRECT,  INCIDENTAL,
SPECIAL,  EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING,  BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF USE, DATA,  OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED  AND ON ANY THEORY OF LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------------------
$Id$
$Date$
$URL$

#endif
//-----------------------------------------------------------------------------------------
#include <config.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <iterator>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <bitset>
#include <time.h>

#ifdef HAS_TR1_UNORDERED_MAP
#include <tr1/unordered_map>
#endif

#include <strings.h>
#include <regex.h>

#include <f8exception.hpp>
#include <thread.hpp>
#include <f8types.hpp>
#include <f8utils.hpp>
#include <traits.hpp>
#include <field.hpp>
#include <message.hpp>
#include <gzstream.hpp>
#include <logger.hpp>

//-------------------------------------------------------------------------------------------------
const std::string TRANSLATIONUNIT(__FILE__);
using namespace FIX8;
using namespace std;

//-------------------------------------------------------------------------------------------------
int Logger::operator()()
{
   unsigned received(0);
	bool stopping(false);

   for (;;)
   {
		string msg;
		if (stopping)	// make sure we dequeue any pending msgs before exiting
		{
			if (!_msg_queue.try_pop(msg))
				break;
		}
		else
			_msg_queue.pop (msg); // will block

      if (msg.empty())  // means exit
		{
         stopping = true;
			continue;
		}

		++received;

		if (_flags & sequence)
			get_stream() << setw(7) << right << setfill('0') << ++_sequence << ' ';

		if (_flags & timestamp)
		{
			string ts;
			get_stream() << GetTimeAsStringMS(ts) << ' ';
		}

		get_stream() << msg << endl;
   }

   return 0;
}

//-------------------------------------------------------------------------------------------------
const string& Logger::GetTimeAsStringMS(string& result, timespec *tv)
{
   timespec *startTime, gotTime;
   if (tv)
      startTime = tv;
   else
   {
      clock_gettime(CLOCK_REALTIME, &gotTime);
      startTime = &gotTime;
   }

   struct tm tim;
   localtime_r(&startTime->tv_sec, &tim);
   double secs(tim.tm_sec + startTime->tv_nsec/1000000000.);
   ostringstream oss;
   oss << setfill('0') << setw(4) << (tim.tm_year + 1900);
   oss << setw(2) << (tim.tm_mon + 1) << setw(2) << tim.tm_mday << ' ' << setw(2) << tim.tm_hour;
   oss << ':' << setw(2) << tim.tm_min << ':';
   oss.setf(ios::showpoint);
   oss.setf(ios::fixed);
   oss << setw(9) << setfill('0') << setprecision(6) << secs;
   return result = oss.str();
}

//-------------------------------------------------------------------------------------------------
FileLogger::FileLogger(const std::string& fname, const ebitset<Flags> flags, const unsigned rotnum)
	: Logger(flags), _rotnum(rotnum)
{
   // | uses IFS safe cfpopen; ! uses old popen if available
   if (fname[0] == '|' || fname[0] == '!')
   {
      _pathname = fname.substr(1);
      FILE *pcmd(
#ifdef HAVE_POPEN
         fname[0] == '!' ? popen(_pathname.c_str(), "w") :
#endif
                        cfpopen(const_cast<char*>(_pathname.c_str()), const_cast<char*>("w")));
      if (pcmd == 0)
      {
         //SimpleLogError(TRANSLATIONUNIT, __LINE__, _pathname);
#ifdef DEBUG
         *errofs << _pathname << " failed to execute" << endl;
#endif
      }
      else if (ferror(pcmd))
#ifdef DEBUG
         *errofs << _pathname << " shows ferror" << endl
#endif
         ;
      else
      {
         _ofs = new fptrostream(pcmd, fname[0] == '|');
         _flags |= pipe;
      }
   }
   else
   {
      _pathname = fname;
      rotate();
   }
}

//-------------------------------------------------------------------------------------------------
bool FileLogger::rotate()
{
   if (!(_flags & pipe))
   {
      string thislFile(_pathname);
      if (_flags & compress)
         thislFile += ".gz";

      if (_rotnum > 0)
      {
         vector<string> rlst;
         rlst.push_back(thislFile);

         for (unsigned ii(0); ii < _rotnum && ii < max_rotation; ++ii)
         {
            ostringstream ostr;
            ostr << _pathname << '.' << (ii + 1);
            if (_flags & compress)
               ostr << ".gz";
            rlst.push_back(ostr.str());
         }

         for (int ii(_rotnum); ii; --ii)
            rename (rlst[ii - 1].c_str(), rlst[ii].c_str());   // ignore errors
      }

      delete _ofs;

#ifdef HAVE_COMPRESSION
      if (_flags & compress)
         _ofs = new ogzstream(thislFile.c_str());
      else
#endif
         _ofs = new ofstream(thislFile.c_str(),
				std::ios_base::out|(_flags & append) ? std::ios_base::app : std::ios_base::trunc);
   }

   return true;
}

