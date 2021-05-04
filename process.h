#pragma once

#include <filesystem>
#include <boost/process/child.hpp>
#include <optional>

namespace base {
class Process {
 public:
  // Creates a Process with Exe name and command line args
  // On success will provide a Process else nullopt for failure
  static std::optional<Process> CreateProcessWithExeAndArgs(
      boost::asio::io_context& io,
      const std::filesystem::path& exe_path,
      std::vector<std::string> args = {});
  boost::process::child& NativeProcess() { return child_process_; }
  void Terminate();
  ~Process();
  void SetContext(boost::asio::io_context& io);
  // Move only semantics allowed
  Process(Process&& other) = default;
  Process& operator=(Process&& other) = default;

  // The copy operations are implicitly deleted, but you can
  // spell that out explicitly if you want:
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;

 private:
  Process(boost::process::child& child);
  boost::process::child child_process_;
};

}  // namespace base
