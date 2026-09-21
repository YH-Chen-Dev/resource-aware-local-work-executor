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

enum class LaunchStatus {
    Success,
    ExecFailed,
    ReadFailed,
    ProtocolError,
};

struct Job {
    std::string executable;
    std::vector<std::string> arguments;
};

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

    int pipe_fds[2];

    if (pipe2(pipe_fds, O_CLOEXEC) == -1) {
        std::cerr << "Pipe created failed." << std::endl;

        std::exit(EXIT_FAILURE);
    }

    Job job {
        "/bin/echo",
        {"hello", "world"}
    };

    std::vector<char*> job_argv;

    job_argv.push_back(job.executable.data());

    for (std::string& argument : job.arguments) {
        job_argv.push_back(argument.data());
    }

    job_argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "Fork failed." << std::endl;

        close(pipe_fds[0]);
        close(pipe_fds[1]);

        std::exit(EXIT_FAILURE);
    }

    else if (pid == 0) {
        close(pipe_fds[0]);

        execv(job.executable.data(), job_argv.data());

        int exec_errno = errno;

        const char* data = reinterpret_cast<const char*>(&exec_errno);
        std::size_t total_written = 0;

        while (total_written < sizeof(exec_errno)) {
            const char* w_addr = data + total_written;
            std::size_t w_size = sizeof(exec_errno) - total_written;
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
        int status;

        close(pipe_fds[1]);

        int exec_errno;
        int read_errno;
        bool read_failed = false;
        char* data = reinterpret_cast<char*>(&exec_errno);

        std::size_t total_read = 0;

        while (total_read < sizeof(exec_errno)) {
            char* r_addr = data + total_read;
            std::size_t r_size = sizeof(exec_errno) - total_read;
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

                read_errno = errno;
                read_failed = true;
            }

            break;
        }

        LaunchStatus launch_status;

        if (read_failed) {
            launch_status = LaunchStatus::ReadFailed;
        }

        else if (total_read == sizeof(exec_errno)) {
            launch_status = LaunchStatus::ExecFailed;
        }

        else if (total_read > 0) {
            launch_status = LaunchStatus::ProtocolError;
        }

        else {
            launch_status = LaunchStatus::Success;
        }

        close(pipe_fds[0]);

        while (true) {
            pid_t waited_pid = waitpid(pid, &status, 0);

            if (waited_pid == -1) {
                if (errno == EINTR) {
                    continue;
                }

                std::cerr << "waitpid failed." << std::endl;

                std::exit(EXIT_FAILURE);
            }

            if (WIFEXITED(status)) {
                std::cout << "Exited, status: " << WEXITSTATUS(status) << std::endl;
            }

            else if (WIFSIGNALED(status)) {
                std::cout << "Killed by signal, status: " << WTERMSIG(status) << std::endl;
            }

            break;
        }

        switch(launch_status) {
            case LaunchStatus::Success:
                std::cout << "LaunchStatus::Success" << std::endl;
                break;

            case LaunchStatus::ReadFailed:
                std::cout << "LaunchStatus::ReadFailed" << std::endl;
                std::cout << "read_errno 對應 " << read_errno << std::endl;
                break;

            case LaunchStatus::ExecFailed:
                std::cout << "LaunchStatus::ExecFailed" << std::endl;
                std::cout << "exec_errno 對應 " << exec_errno << std::endl;
                break;

            case LaunchStatus::ProtocolError:
                std::cout << "LaunchStatus::ProtocolError" << std::endl;
                break;
        }
    }

    std::exit(EXIT_SUCCESS);
}
