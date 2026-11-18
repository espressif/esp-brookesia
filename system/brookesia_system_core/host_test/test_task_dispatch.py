# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check the system task queue's non-inline delivery contract."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


PROGRAM = r'''
#include <cassert>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <utility>
namespace lib_utils {
struct TaskScheduler {
    using Group = std::string;
    using OnceTask = std::function<void()>;
    bool running = false;
    bool reject = false;
    OnceTask pending;
    bool is_running() const { return running; }
    bool post(OnceTask task, std::nullptr_t, const Group &) {
        if (reject) return false;
        pending = std::move(task);
        return true;
    }
};
}
struct FakeImpl {
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    bool same_domain = false;
    bool is_current_task_domain(const std::string &) const { return same_domain; }
    std::expected<void, std::string> post_task(const std::string &, std::function<void()>, bool);
};
'''

TESTS = r'''
int main() {
    FakeImpl impl;
    int calls = 0;
    auto callback = [&] { ++calls; };
    assert(!impl.post_task("app", callback, false) && calls == 0);
    assert(impl.post_task("app", callback, true) && calls == 1);
    impl.task_scheduler_ = std::make_shared<lib_utils::TaskScheduler>();
    assert(!impl.post_task("app", callback, false) && calls == 1);
    impl.task_scheduler_->running = true;
    impl.same_domain = true;
    assert(impl.post_task("app", callback, false) && calls == 1);
    impl.task_scheduler_->pending();
    assert(calls == 2);
    assert(impl.post_task("app", callback, true) && calls == 3);
    impl.task_scheduler_->reject = true;
    assert(!impl.post_task("app", callback, false) && calls == 3);
}
'''


def main():
    source = (ROOT / 'src/system/task.cpp').read_text()
    method = extract(source, 'std::expected<void, std::string> System::Impl::post_task(')
    method = method.replace('System::Impl::post_task', 'FakeImpl::post_task')
    with tempfile.TemporaryDirectory(prefix='system-task-dispatch-') as temporary:
        source_path = Path(temporary) / 'test.cpp'
        binary = Path(temporary) / 'test'
        source_path.write_text(PROGRAM + method + TESTS)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        str(source_path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('System task dispatch tests passed')


if __name__ == '__main__':
    main()
