

#include "process.h"

#include <boost/process.hpp>
#include <boost/process/async.hpp>
#include <boost/process/child.hpp>
#include <boost/process/extend.hpp>
#include <iostream>

using namespace boost::process;

namespace base {

// This is a way to do via async_handler extension
struct async_bar : extend::async_handler {
  async_bar() {
    handler = [](int, const std::error_code&) -> void {
      std::cout << " Ravi on async_bar exit \n";
    };
  }
  template <class Executor>
  void on_setup(Executor& ec) {
    std::cout << "Ravi in on setup!!!!\n";
    on_exit_handler(ec);
  }

  std::function<void(int, const std::error_code&)> handler;
  template <typename Executor>
  std::function<void(int, const std::error_code&)> on_exit_handler(
      Executor& exec) {
    auto handler_ = this->handler;
    return [handler_](int exit_code, const std::error_code& ec) {
      handler_(static_cast<int>(exit_code), ec);
    };
  }
};

/* static */
std::optional<Process> Process::CreateProcessWithExeAndArgs(
    boost::asio::io_context& io,
  const std::filesystem::path& exe_path,
    std::vector<std::string> in_args) {
  if (exe_path.empty()) {
    // DCHECK here
    return std::nullopt;
  }

  // FIXME: Re=-enabled command line
  auto command_line{ exe_path.string() };
  std::for_each(in_args.begin(), in_args.end(),
              [&command_line](const std::string& arg) {
                 command_line.append(std::string(" ") + arg);
               });

  // References:
  // https://www.boost.org/doc/libs/1_75_0/doc/html/boost_process/extend.html
  //
  // The following is a way to do using simple lambda
  std::error_code ec;
  boost::process::child child_process(
      command_line.c_str(), io,
      boost::process::extend::on_success =
          [](const auto& exec) {
            std::cout << "Process success thread id: "
                      << std::this_thread::get_id() << "\n";
          },
      on_exit =
          [](int exit, const std::error_code& ec_in) {
            std::cout << "Process exited thread id: "
                      << std::this_thread::get_id() << "\n";
          },
      ec);

  // The following uses a class
  // boost::process::child child_process(exe_path, io, async_bar(), ec);

  if (ec) {
    std::cout << "Error " << ec.message() << std::endl;
    return std::nullopt;
  } else {
    return std::move(Process(child_process));
  }
}

Process::Process(boost::process::child& child)
    : child_process_(std::move(child)) {}

Process::~Process() {
  Terminate();
}

// FIXME: Temporarily using terminate on main process exit
// Need to be design properly. In posix, child process exits automatically
// if parent process dies, if they were not detached explicitly
// Need to figure out windows story.
void Process::Terminate() {
  if (child_process_.running()) {
    child_process_.terminate();
  }
}
}  // namespace base
