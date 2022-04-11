/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2019 ScyllaDB Ltd.
 */

#include <seastar/core/reactor.hh>
#include "core/thread_pool.hh"

namespace seastar {

/* not yet implemented for OSv. TODO: do the notification like we do class smp. */
#ifndef HAVE_OSV
thread_pool::thread_pool(reactor* r, sstring name) : _reactor(r), _worker_thread([this, name] { work(name); }) {
}

void thread_pool::work(sstring name) {
    pthread_setname_np(pthread_self(), name.c_str());
    sigset_t mask;
    sigfillset(&mask);
    auto r = ::pthread_sigmask(SIG_BLOCK, &mask, NULL);
    throw_pthread_error(r);
    std::array<syscall_work_queue::work_item*, syscall_work_queue::queue_length> tmp_buf;
    while (true) {
        uint64_t count;
        /* balus(N): 如果线程池没有任务，那么该线程会被阻塞在这个 read 操作上 */
        auto r = ::read(inter_thread_wq._start_eventfd.get_read_fd(), &count, sizeof(count));
        assert(r == sizeof(count));
        if (_stopped.load(std::memory_order_relaxed)) {
            break;
        }
        auto end = tmp_buf.data();
        inter_thread_wq._pending.consume_all([&] (syscall_work_queue::work_item* wi) {
            *end++ = wi;
        });
        for (auto p = tmp_buf.data(); p != end; ++p) {
            auto wi = *p;
            wi->process();
            inter_thread_wq._completed.push(wi);
        }
        /* balus(N): 走到这里说明线程池已经处理了任务，所以有一些 future 会被 resolve，那么其 continuation
         * 就需要 worker 线程去执行，如果 worker 线程正在睡眠，那么线程池就通过这个 notify eventfd 通知它，
         * 写该 fd 会产生一个可读事件从而被 epoll 感知到并唤醒 worker 线程继续执行其 main run loop */
        if (_main_thread_idle.load(std::memory_order_seq_cst)) {
            uint64_t one = 1;
            ::write(_reactor->_notify_eventfd.get(), &one, 8);
        }
    }
}

thread_pool::~thread_pool() {
    _stopped.store(true, std::memory_order_relaxed);
    inter_thread_wq._start_eventfd.signal(1);
    _worker_thread.join();
}
#endif

}
