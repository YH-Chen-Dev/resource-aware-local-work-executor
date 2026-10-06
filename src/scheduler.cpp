#include "scheduler.hpp"
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <csignal>

enum class ExecutorFailureStatus {
    Pipe2Failed,
    ForkFailed,
    None
};

enum class ChildTerminationStatus {
    Exited,
    Signaled,
    None
};

enum class ChildLaunchStatus {
    Success,
    ExecFailed,
    ReadFailed,
    ProtocolError,
    None
};

enum class JobReapStatus {
    Success,
    WaitpidFailed,
    UntrackedChild
};

enum class TimedReapStatus {
    Reaped,
    Timeout,
    WaitpidFailed
};

struct JobExecutionResult {
    std::optional<int> child_exec_errno;
    std::optional<int> launch_pipe_read_errno;
    std::optional<int> executor_errno;
    std::optional<int> termination_value;
    ExecutorFailureStatus executor_failure_status = ExecutorFailureStatus::None;
    ChildTerminationStatus child_termination_status = ChildTerminationStatus::None;
    ChildLaunchStatus child_launch_status = ChildLaunchStatus::None;
};

struct JobLaunchResult {
    std::optional<pid_t> pid;
    JobExecutionResult job_execution_result;
};

struct ChildRecord {
    std::size_t job_index;
    JobExecutionResult job_execution_result;
};

struct ReapedChild {
    pid_t pid;
    std::size_t job_index;
    JobExecutionResult job_execution_result;
};

struct JobReapResult {
    JobReapStatus job_reap_status = JobReapStatus::Success;
    std::optional<ReapedChild> reaped_child;
    std::optional<int> waitpid_errno;
    std::optional<pid_t> untracked_pid;
};

void print_scheduler_error(const SchedulerResult& result) {
    switch (result.scheduler_status) {
        case SchedulerStatus::InvalidConcurrency:
            std::cerr << "Executor error: concurrency must be greater than zero." << std::endl;
            break;

        case SchedulerStatus::UntrackedChild:
            std::cerr << "Executor error: untracked child." << std::endl;
            std::cerr << "Untracked child pid: " << *result.untracked_pid << std::endl;
            break;

        case SchedulerStatus::WaitpidFailed:
            std::cerr << "Executor error: waitpid failed." << std::endl;
            std::cerr << "Waitpid errno: " << *result.waitpid_errno << std::endl;
            break;

        case SchedulerStatus::Success:
            break;
    }

    if (!result.job_infrastructure_failures.empty()) {
        const auto& job_infrastructure_failures = result.job_infrastructure_failures;

        std::cerr << "Job infrastructure failures:" << std::endl;

        for (const JobInfrastructureFailure& job_infrastructure_failure : job_infrastructure_failures) {
            std::cerr << "Job " << job_infrastructure_failure.job_index << ": ";

            switch (job_infrastructure_failure.failure_type) {
                case JobInfrastructureFailureType::Pipe2Failed:
                    std::cerr << "create pipe failed." << std::endl;
                    break;

                case JobInfrastructureFailureType::ForkFailed:
                    std::cerr << "fork failed." << std::endl;
                    break;

                case JobInfrastructureFailureType::LaunchPipeReadFailed:
                    std::cerr << "launch pipe read failed." << std::endl;
                    break;

                case JobInfrastructureFailureType::LaunchProtocolError:
                    std::cerr << "launch protocol error." << std::endl;
                    break;
            }
        }
    }

    if (result.job_cleanup_result.has_value()) {
        const auto& failures = result.job_cleanup_result->failures;

        if (!failures.empty()) {
            std::cerr << "Cleanup waitpid failures:" << std::endl;

            for (const JobCleanupFailure& failure : failures) {
                std::cerr << "PID " << failure.pid << ": ";

                switch (failure.failure_type) {
                    case JobCleanupFailureType::SigtermFailed:
                        std::cerr << "SIGTERM failed";
                        break;

                    case JobCleanupFailureType::SigkillFailed:
                        std::cerr << "SIGKILL failed";
                        break;

                    case JobCleanupFailureType::WaitpidFailed:
                        std::cerr << "waitpid failed";
                        break;

                    case JobCleanupFailureType::ReapTimeout:
                        std::cerr << "reap timed out";
                        break;
                }

                if (failure.errno_number.has_value()) {
                    std::cerr << ", errno" << *failure.errno_number;
                }

                std::cerr << std::endl;
            }
        }
    }
}

void print_job_execution_result(std::size_t index, const JobExecutionResult& result) {
    std::cout << "Job " << index << ":" << std::endl;

    switch (result.child_launch_status) {
        case ChildLaunchStatus::Success:
            std::cout << "Launch: success" << std::endl;
            break;

        case ChildLaunchStatus::ReadFailed:
            std::cout << "Launch: error-report read failed" << std::endl;
            std::cout << "Launch pipe read errno: " << *result.launch_pipe_read_errno << std::endl;
            break;

        case ChildLaunchStatus::ExecFailed:
            std::cout << "Launch: exec failed" << std::endl;
            std::cout << "Child exec errno: " << *result.child_exec_errno << std::endl;
            break;

        case ChildLaunchStatus::ProtocolError:
            std::cout << "Launch: exec failed with incomplete error report" << std::endl;
            break;

        case ChildLaunchStatus::None:
            break;
    }

    switch (result.child_termination_status) {
        case ChildTerminationStatus::Exited:
            std::cout << "Termination: exited" << std::endl;
            std::cout << "Exit status: " << *result.termination_value << std::endl;
            break;

        case ChildTerminationStatus::Signaled:
            std::cout << "Termination: signaled" << std::endl;
            std::cout << "Signal: " << *result.termination_value << std::endl;
            break;

        case ChildTerminationStatus::None:
            break;
    }

    switch (result.executor_failure_status) {
        case ExecutorFailureStatus::Pipe2Failed:
            std::cout << "Executor failure: pipe creation failed" << std::endl;
            std::cout << "errno: " << *result.executor_errno << std::endl;
            break;

        case ExecutorFailureStatus::ForkFailed:
            std::cout << "Executor failure: fork failed" << std::endl;
            std::cout << "errno: " << *result.executor_errno << std::endl;
            break;

        case ExecutorFailureStatus::None:
            break;
    }
}

TimedReapStatus reap_child_with_timeout(pid_t pid, int& status, int& waitpid_errno) {
    for (int attempt = 0; attempt < 200; attempt++) {
        pid_t waited_pid = waitpid(pid, &status, WNOHANG);

        if (waited_pid == pid) {
            return TimedReapStatus::Reaped;
        }

        if (waited_pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            waitpid_errno = errno;

            return TimedReapStatus::WaitpidFailed;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return TimedReapStatus::Timeout;
}

JobCleanupResult cleanup_unreaped_children(std::unordered_map<pid_t, ChildRecord>& unreaped_children) {
    JobCleanupResult result;

    for (auto it = unreaped_children.begin(); it != unreaped_children.end();) {
        pid_t pid = it->first;
        ChildRecord& child_record = it->second;
        int status;
        int waitpid_errno;
        int sigterm_errno;
        int sigkill_errno;

        if (kill(pid, SIGTERM) == -1) {
            sigterm_errno = errno;

            result.failures.push_back(
                JobCleanupFailure {
                    pid,
                    JobCleanupFailureType::SigtermFailed,
                    sigterm_errno
                }
            );
        }

        TimedReapStatus sigterm_reap_status = reap_child_with_timeout(pid, status, waitpid_errno);

        if (sigterm_reap_status == TimedReapStatus::Reaped) {
            if (WIFEXITED(status)) {
                child_record.job_execution_result.termination_value = WEXITSTATUS(status);
                child_record.job_execution_result.child_termination_status = ChildTerminationStatus::Exited;
            }

            else if (WIFSIGNALED(status)) {
                child_record.job_execution_result.termination_value = WTERMSIG(status);
                child_record.job_execution_result.child_termination_status = ChildTerminationStatus::Signaled;
            }

            result.reaped_pids.push_back(pid);

            print_job_execution_result(child_record.job_index, child_record.job_execution_result);

            it = unreaped_children.erase(it);

            continue;
        }

        else if (sigterm_reap_status == TimedReapStatus::WaitpidFailed) {
            result.failures.push_back(
                JobCleanupFailure {
                    pid,
                    JobCleanupFailureType::WaitpidFailed,
                    waitpid_errno
                }
            );

            it++;
            continue;
        }

        if (kill(pid, SIGKILL) == -1) {
            sigkill_errno = errno;

            result.failures.push_back(
                JobCleanupFailure {
                    pid,
                    JobCleanupFailureType::SigkillFailed,
                    sigkill_errno
                }
            );
        }

        TimedReapStatus sigkill_reap_status = reap_child_with_timeout(pid, status, waitpid_errno);

        if (sigkill_reap_status == TimedReapStatus::Reaped) {
            if (WIFEXITED(status)) {
                child_record.job_execution_result.termination_value = WEXITSTATUS(status);
                child_record.job_execution_result.child_termination_status = ChildTerminationStatus::Exited;
            }

            else if (WIFSIGNALED(status)) {
                child_record.job_execution_result.termination_value = WTERMSIG(status);
                child_record.job_execution_result.child_termination_status = ChildTerminationStatus::Signaled;
            }

            result.reaped_pids.push_back(pid);

            print_job_execution_result(child_record.job_index, child_record.job_execution_result);

            it = unreaped_children.erase(it);

            continue;
        }

        else if (sigkill_reap_status == TimedReapStatus::WaitpidFailed) {
            result.failures.push_back(
                JobCleanupFailure {
                    pid,
                    JobCleanupFailureType::WaitpidFailed,
                    waitpid_errno
                }
            );

            it++;
        }

        else if (sigkill_reap_status == TimedReapStatus::Timeout) {
            result.failures.push_back(
                JobCleanupFailure {
                    pid,
                    JobCleanupFailureType::ReapTimeout,
                    {}
                }
            );

            it++;
        }
    }

    return result;
}

JobReapResult reap_one_job(const std::unordered_map<pid_t, ChildRecord>& unreaped_children) {
    JobReapResult result;

    int status;

    while (true) {
        pid_t waited_pid = waitpid(-1, &status, 0);

        if (waited_pid == -1) {
            if (errno == EINTR) {
                continue;
            }

            result.waitpid_errno = errno;
            result.job_reap_status = JobReapStatus::WaitpidFailed;

            return result;
        }

        auto it = unreaped_children.find(waited_pid);

        if (it == unreaped_children.end()) {
            result.untracked_pid = waited_pid;
            result.job_reap_status = JobReapStatus::UntrackedChild;

            return result;
        }

        const ChildRecord& child_record = it->second;

        JobExecutionResult job_execution_result = child_record.job_execution_result;

        if (WIFEXITED(status)) {
            job_execution_result.termination_value = WEXITSTATUS(status);
            job_execution_result.child_termination_status = ChildTerminationStatus::Exited;
        }

        else if (WIFSIGNALED(status)) {
            job_execution_result.termination_value = WTERMSIG(status);
            job_execution_result.child_termination_status = ChildTerminationStatus::Signaled;
        }

        result.reaped_child = ReapedChild {
            waited_pid,
            child_record.job_index,
            job_execution_result
        };

        result.job_reap_status = JobReapStatus::Success;

        break;
    }

    return result;
}

JobLaunchResult launch_job(const Job& job) {
    JobLaunchResult result;

    // Create argv
    std::vector<std::string> argv_storage;
    std::vector<char*> job_argv;

    argv_storage.push_back(job.executable);

    for (const std::string& argument : job.arguments) {
        argv_storage.push_back(argument);
    }

    for (std::string& argument : argv_storage) {
        job_argv.push_back(argument.data());
    }

    job_argv.push_back(nullptr);

    int pipe_fds[2];

    if (pipe2(pipe_fds, O_CLOEXEC) == -1) {
        result.job_execution_result.executor_errno = errno;
        result.job_execution_result.executor_failure_status = ExecutorFailureStatus::Pipe2Failed;

        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.job_execution_result.executor_errno = errno;
        result.job_execution_result.executor_failure_status = ExecutorFailureStatus::ForkFailed;

        close(pipe_fds[0]);
        close(pipe_fds[1]);

        return result;
    }

    else if (pid == 0) {
        close(pipe_fds[0]);

        execv(job.executable.data(), job_argv.data());

        int child_exec_errno = errno;

        const char* data = reinterpret_cast<const char*>(&child_exec_errno);
        std::size_t total_written = 0;

        while (total_written < sizeof(child_exec_errno)) {
            const char* w_addr = data + total_written;
            std::size_t w_size = sizeof(child_exec_errno) - total_written;
            ssize_t written_size = write(pipe_fds[1], w_addr, w_size);

            if (written_size > 0) {
                total_written += static_cast<std::size_t>(written_size);

                continue;
            }

            if (written_size == -1 && errno == EINTR) {
                continue;
            }

            break;
        }

        close(pipe_fds[1]);

        _exit(EXIT_FAILURE);
    }

    else {
        result.pid = pid;

        close(pipe_fds[1]);

        int child_exec_errno;
        int launch_pipe_read_errno;
        bool read_failed = false;
        char* data = reinterpret_cast<char*>(&child_exec_errno);

        std::size_t total_read = 0;

        while (total_read < sizeof(child_exec_errno)) {
            char* r_addr = data + total_read;
            std::size_t r_size = sizeof(child_exec_errno) - total_read;
            ssize_t read_size = read(pipe_fds[0], r_addr, r_size);

            if (read_size > 0) {
                total_read += read_size;

                continue;
            }

            else if (read_size == 0) {
                break;
            }

            else if (read_size == -1) {
                if (errno == EINTR) {
                    continue;
                }

                launch_pipe_read_errno = errno;
                read_failed = true;
            }

            break;
        }

        if (read_failed) {
            result.job_execution_result.launch_pipe_read_errno = launch_pipe_read_errno;
            result.job_execution_result.child_launch_status = ChildLaunchStatus::ReadFailed;
        }

        else if (total_read == sizeof(child_exec_errno)) {
            result.job_execution_result.child_exec_errno = child_exec_errno;
            result.job_execution_result.child_launch_status = ChildLaunchStatus::ExecFailed;
        }

        else if (total_read > 0) {
            result.job_execution_result.child_launch_status = ChildLaunchStatus::ProtocolError;
        }

        else {
            result.job_execution_result.child_launch_status = ChildLaunchStatus::Success;
        }

        close(pipe_fds[0]);

        return result;
    }
}

SchedulerResult scheduler(
    const std::vector<Job>& jobs,
    std::size_t max_concurrency,
    const SchedulerEventCallback& on_launch,
    const SchedulerEventCallback& on_reap
) {
    if (max_concurrency == 0) {
        return SchedulerResult {
            SchedulerStatus::InvalidConcurrency,
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt
        };
    }

    std::size_t next_launch_job_index = 0;
    std::unordered_map<pid_t, ChildRecord> unreaped_children;

    JobLaunchResult job_launch_result;
    std::vector<JobInfrastructureFailure> job_infrastructure_failures;
    JobReapResult job_reap_result;

    while (!(next_launch_job_index >= jobs.size() && unreaped_children.empty())) {
        if (next_launch_job_index < jobs.size() && unreaped_children.size() < max_concurrency) {
            std::size_t job_index = next_launch_job_index;

            job_launch_result = launch_job(jobs[job_index]);

            if (job_launch_result.pid.has_value()) {
                unreaped_children.emplace(
                    *job_launch_result.pid,
                    ChildRecord {
                        job_index,
                        job_launch_result.job_execution_result
                    }
                );

                if (on_launch) {
                    on_launch(job_index);
                }
            }

            else {
                print_job_execution_result(job_index, job_launch_result.job_execution_result);
            }

            if (job_launch_result.job_execution_result.executor_failure_status == ExecutorFailureStatus::Pipe2Failed) {
                job_infrastructure_failures.push_back(
                    JobInfrastructureFailure {
                        job_index,
                        JobInfrastructureFailureType::Pipe2Failed
                    }
                );
            }

            else if (job_launch_result.job_execution_result.executor_failure_status == ExecutorFailureStatus::ForkFailed) {
                job_infrastructure_failures.push_back(
                    JobInfrastructureFailure {
                        job_index,
                        JobInfrastructureFailureType::ForkFailed
                    }
                );
            }

            else if (job_launch_result.job_execution_result.child_launch_status == ChildLaunchStatus::ReadFailed) {
                job_infrastructure_failures.push_back(
                    JobInfrastructureFailure {
                        job_index,
                        JobInfrastructureFailureType::LaunchPipeReadFailed
                    }
                );
            }

            else if (job_launch_result.job_execution_result.child_launch_status == ChildLaunchStatus::ProtocolError) {
                job_infrastructure_failures.push_back(
                    JobInfrastructureFailure {
                        job_index,
                        JobInfrastructureFailureType::LaunchProtocolError
                    }
                );
            }

            next_launch_job_index++;
        }

        if (!unreaped_children.empty() && (unreaped_children.size() >= max_concurrency || next_launch_job_index >= jobs.size())) {
            job_reap_result = reap_one_job(unreaped_children);

            switch (job_reap_result.job_reap_status) {
                case JobReapStatus::Success:
                    unreaped_children.erase(job_reap_result.reaped_child->pid);
                    if (on_reap) {
                        on_reap(job_reap_result.reaped_child->job_index);
                    }
                    print_job_execution_result(job_reap_result.reaped_child->job_index, job_reap_result.reaped_child->job_execution_result);
                    break;

                case JobReapStatus::UntrackedChild:
                    return SchedulerResult {
                        SchedulerStatus::UntrackedChild,
                        job_infrastructure_failures,
                        cleanup_unreaped_children(unreaped_children),
                        std::nullopt,
                        job_reap_result.untracked_pid
                    };

                case JobReapStatus::WaitpidFailed:
                    return SchedulerResult {
                        SchedulerStatus::WaitpidFailed,
                        job_infrastructure_failures,
                        cleanup_unreaped_children(unreaped_children),
                        job_reap_result.waitpid_errno,
                        std::nullopt
                    };
            }
        }
    }

    return SchedulerResult {
        SchedulerStatus::Success,
        job_infrastructure_failures,
        std::nullopt,
        std::nullopt,
        std::nullopt
    };
}