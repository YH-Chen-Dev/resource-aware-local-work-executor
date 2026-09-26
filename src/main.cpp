#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cerrno>
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

enum class ExecutorFailureStatus {
    Pipe2Failed,
    ForkFailed,
    WaitpidFailed,
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

struct Job {
    std::string executable;
    std::vector<std::string> arguments;
};

struct JobParseResult {
    bool success;
    Job job;
    std::string error_message;
};

struct JobExecutionResult {
    int child_exec_errno = 0;
    int launch_pipe_read_errno = 0;
    int executor_errno = 0;
    int termination_value = 0;
    ExecutorFailureStatus executor_failure_status = ExecutorFailureStatus::None;
    ChildTerminationStatus child_termination_status = ChildTerminationStatus::None;
    ChildLaunchStatus child_launch_status = ChildLaunchStatus::None;
};

JobParseResult parse_job(const nlohmann::json& job_json) {
    JobParseResult result;

    if (!job_json.is_object()) {
        result.success = false;

        result.error_message = "Job must be a JSON object.";

        return result;
    }

    for (auto field = job_json.begin(); field != job_json.end(); ++field) {
        if (!(field.key() == "executable" || field.key() == "arguments")) {
            result.success = false;

            result.error_message = "Unknown field: " + field.key();

            return result;
        }
    }

    if (!job_json.contains("executable")) {
        result.success = false;

        result.error_message = "Missing required field: executable";

        return result;
    }

    if (!job_json["executable"].is_string()) {
        result.success = false;

        result.error_message = "Field 'executable' must be a string.";

        return result;
    }

    std::string executable = job_json["executable"].get<std::string>();

    if (executable.empty()) {
        result.success = false;

        result.error_message = "Field 'executable' cannot be empty.";

        return result;
    }

    std::vector<std::string> arguments;

    if (job_json.contains("arguments")) {
        if (!job_json["arguments"].is_array()) {
            result.success = false;

            result.error_message = "Field 'arguments' must be an array.";

            return result;
        }

        for (const auto& element : job_json["arguments"]) {
            if (!element.is_string()) {
                result.success = false;

                result.error_message = "Each element of field 'arguments' must be a string.";

                return result;
            }

            arguments.push_back(element.get<std::string>());
        }
    }

    result.success = true;

    result.job = {executable, arguments};

    return result;
}

JobExecutionResult execute_job(const Job& job) {
    JobExecutionResult result;

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
        result.executor_errno = errno;
        result.executor_failure_status = ExecutorFailureStatus::Pipe2Failed;

        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        result.executor_errno = errno;
        result.executor_failure_status = ExecutorFailureStatus::ForkFailed;

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
                total_written += written_size;

                continue;
            }

            else if (written_size == -1) {
                if (errno == EINTR) {
                    continue;
                }
            }

            break;
        }

        close(pipe_fds[1]);

        _exit(EXIT_FAILURE);
    }

    else {
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
            result.launch_pipe_read_errno = launch_pipe_read_errno;
            result.child_launch_status = ChildLaunchStatus::ReadFailed;
        }

        else if (total_read == sizeof(child_exec_errno)) {
            result.child_exec_errno = child_exec_errno;
            result.child_launch_status = ChildLaunchStatus::ExecFailed;
        }

        else if (total_read > 0) {
            result.child_launch_status = ChildLaunchStatus::ProtocolError;
        }

        else {
            result.child_launch_status = ChildLaunchStatus::Success;
        }

        close(pipe_fds[0]);

        int status;

        while (true) {
            pid_t waited_pid = waitpid(pid, &status, 0);

            if (waited_pid == -1) {
                if (errno == EINTR) {
                    continue;
                }

                result.executor_errno = errno;
                result.executor_failure_status = ExecutorFailureStatus::WaitpidFailed;

                return result;
            }

            if (WIFEXITED(status)) {
                result.termination_value = WEXITSTATUS(status);
                result.child_termination_status = ChildTerminationStatus::Exited;
            }

            else if (WIFSIGNALED(status)) {
                result.termination_value = WTERMSIG(status);
                result.child_termination_status = ChildTerminationStatus::Signaled;
            }

            break;
        }

        return result;
    }
}

void print_job_execution_result(size_t index, const JobExecutionResult& result) {
    std::cout << "Job " << index << ":" << std::endl;

    switch (result.child_launch_status) {
        case ChildLaunchStatus::Success:
            std::cout << "Launch: success" << std::endl;
            break;

        case ChildLaunchStatus::ReadFailed:
            std::cout << "Launch: error-report read failed" << std::endl;
            std::cout << "Launch pipe read errno: " << result.launch_pipe_read_errno << std::endl;
            break;

        case ChildLaunchStatus::ExecFailed:
            std::cout << "Launch: exec failed" << std::endl;
            std::cout << "Child exec errno: " << result.child_exec_errno << std::endl;
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
            std::cout << "Exit status: " << result.termination_value << std::endl;
            break;

        case ChildTerminationStatus::Signaled:
            std::cout << "Termination: signaled" << std::endl;
            std::cout << "Signal: " << result.termination_value << std::endl;
            break;

        case ChildTerminationStatus::None:
            break;
    }

    switch (result.executor_failure_status) {
        case ExecutorFailureStatus::Pipe2Failed:
            std::cout << "Executor failure: pipe creation failed" << std::endl;
            std::cout << "errno: " << result.executor_errno << std::endl;
            break;

        case ExecutorFailureStatus::ForkFailed:
            std::cout << "Executor failure: fork failed" << std::endl;
            std::cout << "errno: " << result.executor_errno << std::endl;
            break;

        case ExecutorFailureStatus::WaitpidFailed:
            std::cout << "Executor failure: waitpid failed" << std::endl;
            std::cout << "errno: " << result.executor_errno << std::endl;
            break;

        case ExecutorFailureStatus::None:
            break;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        if (argc < 2) {
            std::cerr << "Error: missing manifest file path." << std::endl;
        }

        else if (argc > 2) {
            std::cerr << "Error: unexpected command-line arguments." << std::endl;
        }

        std::cerr << "Usage: work_executor <manifest-path>" << std::endl;

        std::exit(EXIT_FAILURE);
    }

    const std::string manifest_path = argv[1];
    std::ifstream manifest_file(manifest_path);

    if (!manifest_file.is_open()) {
        std::cerr << "Error: failed to open manifest file: " << manifest_path << std::endl;

        std::exit(EXIT_FAILURE);
    }

    nlohmann::json manifest_json;

    try {
        manifest_json = nlohmann::json::parse(manifest_file);
    }
    catch (const nlohmann::json::parse_error& error) {
        std::cerr << "Error: failed to parse manifest file: " << manifest_path << std::endl;

        std::cerr << error.what() << std::endl;

        std::exit(EXIT_FAILURE);
    }

    if (!manifest_json.is_array()) {
        std::cerr << "Error: invalid manifest schema: " << manifest_path << std::endl;

        std::cerr << "The top-level must be an array." << std::endl;

        std::exit(EXIT_FAILURE);
    }

    std::vector<Job> jobs;

    size_t index = 0;
    for (const auto& job_json : manifest_json) {
        JobParseResult result = parse_job(job_json);

        if (!result.success) {
            std::cerr << "Error: invalid manifest schema: " << manifest_path << std::endl;
            std::cerr << "Job " << index << ": " << result.error_message << std::endl;

            std::exit(EXIT_FAILURE);
        }

        jobs.push_back(result.job);

        index++;
    }

    index = 0;
    for (const Job& job : jobs) {
        JobExecutionResult result = execute_job(job);

        print_job_execution_result(index, result);

        index++;
    }

    std::exit(EXIT_SUCCESS);
}
