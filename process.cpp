
#define NOMINMAX
#include <Winsock2.h>
#include <Windows.h>

#include "process.h"

#include <boost/process.hpp>
//#include <boost/winapi/handles.hpp>
//#include <boost/winapi/handle_info.hpp>
#include <boost/process/child.hpp>
#include <boost/process/extend.hpp>
#include <boost/process/async.hpp>
#include <iostream>

using namespace boost::process;

namespace base {

struct async_bar : extend::async_handler
{
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
  template<typename Executor>
  std::function<void(int, const std::error_code&)> on_exit_handler(Executor& exec)
  {
    auto handler_ = this->handler;
    return [handler_](int exit_code, const std::error_code& ec)
    {
      handler_(static_cast<int>(exit_code), ec);
    };

  }
};

/* static */
std::optional<Process> Process::CreateProcessWithExeAndArgs(
    boost::asio::io_context& io,
    const boost::filesystem::path& exe_path,
    std::vector<std::string> args) {
  if (exe_path.empty()) {
    // DCHECK here
    return std::nullopt;
  }
  //std::string command_line = exe_path.string();
  //std::for_each(args.begin(), args.end(),
 //              [&command_line](const std::string& arg) {
 //                 command_line.append(std::string(" ") + arg);
  //              });
  std::error_code ec;
  async_bar bar;

#if 0
  std::function<void(int exit, const std::error_code& ec_in)> on_exit = [](int exit, const std::error_code& ec_in) {
    std::cout << "Ravi on exit\n";
  };
    auto on_success = [](int exit, const std::error_code& ec_in) {
    std::cout << "Ravi on success\n";
  };
    auto on_error = [](int exit, const std::error_code& ec_in) {
      std::cout << "Ravi on error\n";
    };
#endif

// The following is a way to do usign simple lambda
  boost::process::child child_process(exe_path, io, on_exit = [](int exit, const std::error_code& ec_in) {
    std::cout << "Ravi on exit  thread id: " << std::this_thread::get_id() << "\n";
    }, ec);

  // The following uses a class
 
 //boost::process::child child_process(exe_path, io, async_bar(), ec);


  if (ec) {
    std::cout << "Error " << ec.message() <<std::endl;
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
