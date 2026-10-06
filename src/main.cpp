#include "scheduler.hpp"
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <utility>
#include <charconv>

enum class ManifestLoadStatus {
    Success,
    FileOpenFailed,
    JsonParseFailed,
    SchemaInvalid
};

struct ConcurrencyParseResult {
    std::optional<std::size_t> value;
    std::optional<std::string> error_message;
};

struct CliOptions {
    std::string manifest_path;
    std::size_t max_concurrency;
};

struct CliParseResult {
    std::optional<CliOptions> options;
    std::optional<std::string> error_message;
};

struct ManifestLoadResult {
    ManifestLoadStatus manifest_load_status = ManifestLoadStatus::Success;
    std::vector<Job> jobs;
    std::optional<std::string> error_message;
    std::optional<std::size_t> job_index;
};

struct JobParseResult {
    std::optional<Job> job;
    std::optional<std::string> error_message;
};

JobParseResult parse_job(const nlohmann::json& job_json) {
    JobParseResult result;

    if (!job_json.is_object()) {
        result.error_message = "Job must be a JSON object.";

        return result;
    }

    for (auto field = job_json.begin(); field != job_json.end(); ++field) {
        if (!(field.key() == "executable" || field.key() == "arguments")) {
            result.error_message = "Unknown field: " + field.key();

            return result;
        }
    }

    if (!job_json.contains("executable")) {
        result.error_message = "Missing required field: executable";

        return result;
    }

    if (!job_json["executable"].is_string()) {
        result.error_message = "Field 'executable' must be a string.";

        return result;
    }

    std::string executable = job_json["executable"].get<std::string>();

    if (executable.find('\0') != std::string::npos) {
        result.error_message = "Field 'executable' must not contain NUL byte.";

        return result;
    }

    if (executable.empty()) {
        result.error_message = "Field 'executable' cannot be empty.";

        return result;
    }

    std::vector<std::string> arguments;

    if (job_json.contains("arguments")) {
        if (!job_json["arguments"].is_array()) {
            result.error_message = "Field 'arguments' must be an array.";

            return result;
        }

        for (const auto& element : job_json["arguments"]) {
            if (!element.is_string()) {
                result.error_message = "Each element of field 'arguments' must be a string.";

                return result;
            }

            std::string argument = element.get<std::string>();

            if (argument.find('\0') != std::string::npos) {
                result.error_message = "Each element of field 'arguments' must not contain NUL byte.";

                return result;
            }

            arguments.push_back(argument);
        }
    }

    result.job = Job {executable, arguments};

    return result;
}

void print_manifest_load_error(const std::string& manifest_path, const ManifestLoadResult& result) {
    switch (result.manifest_load_status) {
        case ManifestLoadStatus::FileOpenFailed:
            std::cerr << "Error: failed to open manifest file: " << manifest_path << std::endl;
            break;

        case ManifestLoadStatus::JsonParseFailed:
            std::cerr << "Error: failed to parse manifest file: " << manifest_path << std::endl;
            std::cerr << *result.error_message << std::endl;
            break;

        case ManifestLoadStatus::SchemaInvalid:
            std::cerr << "Error: invalid manifest schema: " << manifest_path << std::endl;

            if (result.job_index.has_value()) {
                std::cerr << "Job " << *result.job_index << ": ";
            }

            std::cerr << *result.error_message << std::endl;
            break;

        case ManifestLoadStatus::Success:
            break;
    }
}

ManifestLoadResult load_manifest(const std::string& manifest_path) {
    ManifestLoadResult manifest_load_result;

    std::ifstream manifest_file(manifest_path);

    if (!manifest_file.is_open()) {
        manifest_load_result.manifest_load_status = ManifestLoadStatus::FileOpenFailed;

        return manifest_load_result;
    }

    nlohmann::json manifest_json;

    try {
        manifest_json = nlohmann::json::parse(manifest_file);
    }
    catch (const nlohmann::json::parse_error& error) {
        manifest_load_result.manifest_load_status = ManifestLoadStatus::JsonParseFailed;

        manifest_load_result.error_message = error.what();

        return manifest_load_result;
    }

    if (!manifest_json.is_array()) {
        manifest_load_result.manifest_load_status = ManifestLoadStatus::SchemaInvalid;

        manifest_load_result.error_message = "The top-level must be an array.";

        return manifest_load_result;
    }

    std::vector<Job> jobs;

    std::size_t index = 0;
    for (const auto& job_json : manifest_json) {
        JobParseResult job_parse_result = parse_job(job_json);

        if (!job_parse_result.job.has_value()) {
            manifest_load_result.manifest_load_status = ManifestLoadStatus::SchemaInvalid;

            manifest_load_result.error_message = job_parse_result.error_message;

            manifest_load_result.job_index = index;

            return manifest_load_result;
        }

        jobs.push_back(std::move(*job_parse_result.job));

        index++;
    }

    manifest_load_result.jobs = std::move(jobs);

    manifest_load_result.manifest_load_status = ManifestLoadStatus::Success;

    return manifest_load_result;
}

void print_cli_error(const CliParseResult& result) {
    std::cerr << "Error: " << *result.error_message << std::endl;
    std::cerr << "Usage: work_executor <manifest-path> --concurrency <N>" << std::endl;
}

ConcurrencyParseResult parse_concurrency(const std::string& text) {
    ConcurrencyParseResult concurrency_parse_result;

    std::size_t value;

    auto convert_result = std::from_chars(text.data(), text.data() + text.size(), value);

    if (convert_result.ec == std::errc::result_out_of_range) {
        concurrency_parse_result.error_message = "Concurrency value is out of range.";

        return concurrency_parse_result;
    }

    if (convert_result.ec != std::errc{} || convert_result.ptr != text.data() + text.size()) {
        concurrency_parse_result.error_message = "Concurrency value is invalid.";

        return concurrency_parse_result;
    }

    if (value == 0) {
        concurrency_parse_result.error_message = "Concurrency must be greater than zero.";

        return concurrency_parse_result;
    }

    concurrency_parse_result.value = value;

    return concurrency_parse_result;
}

CliParseResult parse_cli(int argc, char* argv[]) {
    CliParseResult cli_parse_result;

    if (argc != 4) {
        if (argc < 4) {
            cli_parse_result.error_message = "missing required command-line arguments.";
        }

        else if (argc > 4) {
            cli_parse_result.error_message = "unexpected command-line arguments.";
        }

        return cli_parse_result;
    }

    if (std::string(argv[2]) != "--concurrency") {
        cli_parse_result.error_message = "expected \"--concurrency\" argument.";

        return cli_parse_result;
    }

    ConcurrencyParseResult concurrency_parse_result = parse_concurrency(argv[3]);

    if (!concurrency_parse_result.value.has_value()) {
        cli_parse_result.error_message = concurrency_parse_result.error_message;

        return cli_parse_result;
    }

    cli_parse_result.options = CliOptions {argv[1], *concurrency_parse_result.value};

    return cli_parse_result;
}

int main(int argc, char* argv[]) {
    CliParseResult cli_parse_result = parse_cli(argc, argv);

    if (!cli_parse_result.options.has_value()) {
        print_cli_error(cli_parse_result);

        std::exit(EXIT_FAILURE);
    }

    ManifestLoadResult manifest_load_result = load_manifest(cli_parse_result.options->manifest_path);

    if (manifest_load_result.manifest_load_status != ManifestLoadStatus::Success) {
        print_manifest_load_error(cli_parse_result.options->manifest_path, manifest_load_result);

        std::exit(EXIT_FAILURE);
    }

    SchedulerResult scheduler_result = scheduler(manifest_load_result.jobs, cli_parse_result.options->max_concurrency);

    if (scheduler_result.scheduler_status != SchedulerStatus::Success || !scheduler_result.job_infrastructure_failures.empty()) {
        print_scheduler_error(scheduler_result);

        std::exit(EXIT_FAILURE);
    }

    std::exit(EXIT_SUCCESS);
}
