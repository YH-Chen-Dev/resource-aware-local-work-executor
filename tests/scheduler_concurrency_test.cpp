#include "scheduler.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cerrno>
#include <thread>
#include <chrono>

class ScopedFile {
public:
    explicit ScopedFile(std::string path) : path_(std::move(path)) {}

    bool create() {
        if (mkfifo(path_.c_str(), 0600) == -1) {
            return false;
        }

        created_ = true;
        return true;
    }

    ~ScopedFile() {
        if (created_) {
            unlink(path_.c_str());
        }
    }

    const std::string& path() const {
        return path_;
    }

    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;

private:
    std::string path_;
    bool created_ = false;
};

bool release_gate(const std::string& gate_path) {
    int fd = -1;

    for (int attempt = 0; attempt < 200; attempt++) {
        fd = open(gate_path.c_str(), O_WRONLY | O_NONBLOCK);

        if (fd >= 0) {
            break;
        }

        if (errno != ENXIO) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (fd == -1) {
        return false;
    }

    const char message[] = "go\n";
    std::size_t total_written = 0;

    while (total_written < sizeof(message) - 1) {
        ssize_t written_size = write(fd, message + total_written, sizeof(message) - 1 - total_written);

        if (written_size > 0) {
            total_written += static_cast<std::size_t>(written_size);

            continue;
        }

        if (written_size == -1 && errno == EINTR) {
            continue;
        }

        close(fd);
        return false;
    }

    close(fd);
    return true;
}

int main() {
    std::string pid_text = std::to_string(getpid());

    ScopedFile gate0("/tmp/work_executor_scheduler_test_" + pid_text + "_gate0");
    ScopedFile gate1("/tmp/work_executor_scheduler_test_" + pid_text + "_gate1");

    if (!gate0.create()) {
        std::cerr << "Failed to create gate0" << std::endl;
        return 1;
    }

    if (!gate1.create()) {
        std::cerr << "Failed to create gate1" << std::endl;
        return 1;
    }

    std::string j0_command = "timeout 5 /bin/sh -c 'read message < " + gate0.path() + "'";
    std::string j1_command = "timeout 5 /bin/sh -c 'read message < " + gate1.path() + "'";

    Job J0 {
        "/bin/sh",
        {"-c", j0_command}
    };

    Job J1 {
        "/bin/sh",
        {"-c", j1_command}
    };

    Job J2 {
        "/bin/true",
        {}
    };

    std::vector<Job> jobs {J0, J1, J2};
    std::vector<std::string> events;
    std::vector<std::string> expected_events {
        "Launch(0)",
        "Launch(1)",
        "Reap(1)",
        "Launch(2)",
        "Reap(2)",
        "Reap(0)"
    };

    bool callback_failed = false;

    SchedulerEventCallback on_launch = [&events, &gate1, &callback_failed](std::size_t job_index) {
        events.push_back("Launch(" + std::to_string(job_index) + ")");

        if (job_index == 1) {
            if (!release_gate(gate1.path())) {
                callback_failed = true;
            }
        }
    };

    SchedulerEventCallback on_reap = [&events, &gate0, &callback_failed](std::size_t job_index) {
        events.push_back("Reap(" + std::to_string(job_index) + ")");

        if (job_index == 2) {
            if (!release_gate(gate0.path())) {
                callback_failed = true;
            }
        }
    };

    SchedulerResult result = scheduler(jobs, 2, on_launch, on_reap);

    if (result.scheduler_status != SchedulerStatus::Success) {
        std::cerr << "FAIL: scheduler did not complete successfully" << std::endl;
        return 1;
    }

    if (!result.job_infrastructure_failures.empty()) {
        std::cerr << "FAIL: scheduler reported infrastructure failure" << std::endl;
        return 1;
    }

    if (callback_failed) {
        std::cerr << "FAIL: scheduler callback operation failed" << std::endl;
        return 1;
    }

    if (events != expected_events) {
        std::cerr << "FAIL: unexpected scheduler event order" << std::endl;

        std::cerr << "Expected:" << std::endl;
        for (const auto& event : expected_events) {
            std::cerr << event << std::endl;
        }

        std::cerr << "Actual:" << std::endl;
        for (const auto& event : events) {
            std::cerr << event << std::endl;
        }

        return 1;
    }

    return 0;
}
