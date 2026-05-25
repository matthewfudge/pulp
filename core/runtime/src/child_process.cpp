#include <pulp/runtime/child_process.hpp>

namespace pulp::runtime {

std::optional<ProcessResult> run_process(
    std::string_view executable,
    const std::vector<std::string>& args,
    std::string_view working_dir,
    int timeout_ms)
{
    pulp::platform::ProcessOptions options;
    options.working_directory = std::string(working_dir);
    options.timeout_ms = timeout_ms;

    pulp::platform::ChildProcess process;
    if (!process.start(std::string(executable), args, options))
        return std::nullopt;

    return process.wait();
}

}  // namespace pulp::runtime
