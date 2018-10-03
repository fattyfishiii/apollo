/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "cybertron/croutine/croutine.h"

#include "cybertron/common/log.h"
#include "cybertron/croutine/routine_context.h"
#include "cybertron/event/perf_event_cache.h"
#include "cybertron/time/time.h"

namespace apollo {
namespace cybertron {
namespace croutine {

using apollo::cybertron::event::PerfEventCache;
using apollo::cybertron::event::SchedPerf;

thread_local CRoutine *CRoutine::current_routine_;
thread_local std::shared_ptr<RoutineContext> CRoutine::main_context_;

static void CRoutineEntry(void *arg) {
  CRoutine *r = static_cast<CRoutine *>(arg);
  r->Run();
  SwapContext(r->GetContext(), CRoutine::GetMainContext());
}

CRoutine::CRoutine(const std::function<void()> &func) {
  func_ = func;
  MakeContext(CRoutineEntry, this, &context_);
  state_ = RoutineState::READY;
}

CRoutine::CRoutine(std::function<void()> &&func) {
  func_ = std::move(func);
  MakeContext(CRoutineEntry, this, &context_);
  state_ = RoutineState::READY;
}

CRoutine::~CRoutine() {}

RoutineState CRoutine::Resume() {
  std::unique_lock<std::mutex> ul(mutex_);
  if (force_stop_) {
    state_ = RoutineState::FINISHED;
    return RoutineState::FINISHED;
  }

  UpdateState();

  // Keep compatibility with different policies.
  if (state_ != RoutineState::RUNNING && state_ != RoutineState::READY) {
    AERROR << "Invalid Routine State!";
    return state_;
  }

  current_routine_ = this;
  auto t_start = cybertron::Time::Now().ToNanosecond();
  PerfEventCache::Instance()->AddSchedEvent(SchedPerf::SWAP_IN, id_,
                                            processor_id_, 0, 0, -1, -1);
  SwapContext(GetMainContext(), this->GetContext());
  auto t_end = cybertron::Time::Now().ToNanosecond();
  PerfEventCache::Instance()->AddSchedEvent(SchedPerf::SWAP_OUT, id_,
                                            processor_id_, 0, t_start, -1,
                                            static_cast<int>(state_));
  exec_time_ += t_end - t_start;
  if (state_ == RoutineState::RUNNING) {
    state_ = RoutineState::READY;
  }
  return state_;
}

void CRoutine::Routine() {
  while (true) {
    AINFO << "inner routine" << std::endl;
    usleep(1000000);
  }
}

void CRoutine::Stop() { force_stop_ = true; }

}  // namespace croutine
}  // namespace cybertron
}  // namespace apollo
