/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "oneflow/core/common/mem_util.h"

#include <unistd.h>
#include <sys/sysinfo.h>

namespace oneflow {

// Reference: https://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-runtime-using-c
void ProcessMemUsage(double& vm_usage, double& resident_set) {
  vm_usage = 0.0;
  resident_set = 0.0;

  // 'file' stat seems to give the most reliable results
  std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);

  // dummy vars for leading entries in stat that we don't care about
  std::string pid, comm, state, ppid, pgrp, session, tty_nr;
  std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
  std::string utime, stime, cutime, cstime, priority, nice;
  std::string O, itrealvalue, starttime;

  // the two fields we want
  unsigned long vsize = 0;
  long rss = 0;

  stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags
      >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime >> cutime >> cstime >> priority
      >> nice >> O >> itrealvalue >> starttime >> vsize >> rss;  // don't care about the rest

  stat_stream.close();

  long page_size_kb = sysconf(_SC_PAGE_SIZE);  // in case x86-64 is configured to use 2MB pages
  // return with MB
  vm_usage = vsize >> 20;
  // return with MB
  resident_set = (rss * page_size_kb) >> 20;
}

}  // namespace oneflow