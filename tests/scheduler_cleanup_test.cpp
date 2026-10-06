#include "scheduler.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <cerrno>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <vector>
#include <utility>
#include <sys/wait.h>

class ScopedFile {
public:
    explicit ScopedFile(std::string path) : path_(std::move(path)) {}

    bool remove_stale() {
        if (unlink(path_.c_str()) == -1 && errno != ENOENT) {
            return false;
        }

        return true;
    }

    ~ScopedFile() {
        unlink(path_.c_str());
    }

    const std::string& path() const {
        return path_;
    }

    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;

private:
    std::string path_;
};

bool wait_for_file(const std::string& path) {
    for (int attempt = 0; attempt < 200; attempt++) {
        if (access(path.c_str(), F_OK) == 0) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

bool reap_foreign_child_with_timeout(pid_t pid) {
    int status;

    for (int attempt = 0; attempt < 200; attempt++) {
        pid_t waited_pid = waitpid(pid, &status, WNOHANG);

        if (waited_pid == pid) {
            return true;
        }

        if (waited_pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    kill(pid, SIGKILL);

    for (int attempt = 0; attempt < 200; attempt++) {
        pid_t waited_pid = waitpid(pid, &status, WNOHANG);

        if (waited_pid == pid) {
            break;
        }

        if (waited_pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

int main() {
    ScopedFile term_received = ScopedFile("/tmp/work_executor_scheduler_test_" + std::to_string(getpid()) + "_term_received");

    if (!term_received.remove_stale()) {
        std::cerr << "FAIL: could not remove stale term_received marker" << std::endl;

        return 1;
    }

    ScopedFile j0_ready = ScopedFile("/tmp/work_executor_scheduler_test_" + std::to_string(getpid()) + "_J0_ready");

    if (!j0_ready.remove_stale()) {
        std::cerr << "FAIL: could not remove stale J0_ready marker" << std::endl;

        return 1;
    }

    std::string j0_command = "trap 'touch " + term_received.path() + "' TERM && touch " + j0_ready.path() + " && while :; do :; done";

    Job J0 {
        "/bin/sh",
        {"-c", j0_command}
    };

    std::vector<Job> jobs = {J0};

    int pipe_fds[2];

    if (pipe2(pipe_fds, O_CLOEXEC) == -1) {
        int pipe_errno = errno;

        std::cerr << "Error: create pipe failed, errno: " << pipe_errno << std::endl;

        return 1;
    }

    pid_t foreign_child_pid = fork();

    if (foreign_child_pid < 0) {
        int fork_errno = errno;

        close(pipe_fds[0]);
        close(pipe_fds[1]);

        std::cerr << "Error: fork foreign child failed, errno: " << fork_errno << std::endl;

        return 1;
    }

    else if (foreign_child_pid == 0) {
        close(pipe_fds[1]);

        char message;

        while (true) {
            ssize_t read_size = read(pipe_fds[0], &message, 1);

            if (read_size == 1) {
                close(pipe_fds[0]);

                _exit(EXIT_SUCCESS);
            }

            if (read_size == -1 && errno == EINTR) {
                continue;
            }

            close(pipe_fds[0]);

            _exit(EXIT_FAILURE);
        }
    }

    else {
        close(pipe_fds[0]);

        int foreign_child_write_fd = pipe_fds[1];

        bool callback_failed = false;

        SchedulerEventCallback on_launch = [&](std::size_t job_index) {
            if (job_index != 0 || !wait_for_file(j0_ready.path())) {
                callback_failed = true;

                close(foreign_child_write_fd);
                foreign_child_write_fd = -1;

                return;
            }

            char message = 'x';

            while (true) {
                ssize_t written_size = write(foreign_child_write_fd, &message, 1);

                if(written_size == 1) {
                    break;
                }

                if (written_size == -1 && errno == EINTR) {
                    continue;
                }

                callback_failed = true;

                break;
            }

            close(foreign_child_write_fd);
            foreign_child_write_fd = -1;
        };

        SchedulerResult result = scheduler(jobs, 1, on_launch, {});

        if (foreign_child_write_fd != -1) {
            close(foreign_child_write_fd);
            foreign_child_write_fd = -1;
        }

        bool foreign_child_reaped_by_scheduler = result.scheduler_status == SchedulerStatus::UntrackedChild
        && result.untracked_pid.has_value() && *result.untracked_pid == foreign_child_pid;

        if (!foreign_child_reaped_by_scheduler) {
            if (!reap_foreign_child_with_timeout(foreign_child_pid)) {
                std::cerr << "FAIL: could not reap foreign child" << std::endl;

                return 1;
            }
        }

        if (callback_failed) {
            std::cerr << "FAIL: test synchronization callback failed" << std::endl;
            return 1;
        }

        if (access(term_received.path().c_str(), F_OK) != 0) {
            std::cerr << "FAIL: expected J0 to receive SIGTERM during cleanup" << std::endl;
            return 1;
        }

        if (result.scheduler_status != SchedulerStatus::UntrackedChild) {
            std::cerr << "FAIL: expected scheduler to detect untracked child" << std::endl;
            return 1;
        }

        if (!result.untracked_pid.has_value() || *result.untracked_pid != foreign_child_pid) {
            std::cerr << "FAIL: scheduler reaped unexpected untracked child" << std::endl;
            return 1;
        }

        if (!result.job_cleanup_result.has_value()) {
            std::cerr << "FAIL: expected cleanup result" << std::endl;
            return 1;
        }

        if (!(result.job_cleanup_result->reaped_pids.size() == 1 && result.job_cleanup_result->reaped_pids[0] != foreign_child_pid)) {
            std::cerr << "FAIL: expected cleanup to reap exactly one tracked job, not the foreign child" << std::endl;
            return 1;
        }

        if (!result.job_cleanup_result->failures.empty()) {
            std::cerr << "FAIL: expected cleanup to complete without failures" << std::endl;
            return 1;
        }
    }
}
