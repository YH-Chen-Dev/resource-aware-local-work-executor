#include "scheduler.hpp"
#include <iostream>

int main() {
    std::vector<Job> jobs {
        Job {
            "/bin/true",
            {}
        }
    };

    bool launched = false;

    SchedulerResult result = scheduler(
        jobs,
        0,
        [&launched](std::size_t) {
            launched = true;
        },
        {}
    );

    if (result.scheduler_status != SchedulerStatus::InvalidConcurrency) {
        std::cerr << "FAIL: the concurrency should be invalid" << std::endl;

        return 1;
    }

    if (!result.job_infrastructure_failures.empty()) {
        std::cerr << "FAIL: scheduler should not have infrastructure failure since the concurrency is invalid" << std::endl;

        return 1;
    }

    if (result.job_cleanup_result.has_value()) {
        std::cerr << "FAIL: scheduler should not have cleanup result since the concurrency is invalid" << std::endl;

        return 1;
    }

    if (launched) {
        std::cerr << "FAIL: no job should be launched when concurrency is invalid" << std::endl;

        return 1;
    }

    return 0;
}