#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <cstddef>
#include <sys/types.h>

using SchedulerEventCallback = std::function<void(std::size_t)>;

enum class JobInfrastructureFailureType {
    Pipe2Failed,
    ForkFailed,
    LaunchPipeReadFailed,
    LaunchProtocolError
};

enum class SchedulerStatus {
    Success,
    InvalidConcurrency,
    WaitpidFailed,
    UntrackedChild
};

enum class JobCleanupFailureType {
    SigtermFailed,
    SigkillFailed,
    WaitpidFailed,
    ReapTimeout
};

struct Job {
    std::string executable;
    std::vector<std::string> arguments;
    std::optional<std::string> id;
};

struct JobInfrastructureFailure {
    std::size_t job_index;
    JobInfrastructureFailureType failure_type;
};

struct JobCleanupFailure {
    pid_t pid;
    JobCleanupFailureType failure_type;
    std::optional<int> errno_number;
};

struct JobCleanupResult {
    std::vector<pid_t> reaped_pids;
    std::vector<JobCleanupFailure> failures;
};

struct SchedulerResult {
    SchedulerStatus scheduler_status = SchedulerStatus::Success;
    std::vector<JobInfrastructureFailure> job_infrastructure_failures;
    std::optional<JobCleanupResult> job_cleanup_result;
    std::optional<int> waitpid_errno;
    std::optional<pid_t> untracked_pid;
};

void print_scheduler_error(const SchedulerResult& result);

SchedulerResult scheduler(
    const std::vector<Job>& jobs,
    std::size_t max_concurrency,
    const SchedulerEventCallback& on_launch = {},
    const SchedulerEventCallback& on_reap = {}
);
