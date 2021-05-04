#include "pch.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/child.hpp>
#include <chrono>
#include <iostream>

#include "test_list.h"
// custom process abstraction
#include "process.h"

using io_context_work_gaurd =
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

void ExitObserver(boost::asio::io_context& io) {
  std::cout << "Exit observer thread : " << std::this_thread::get_id() << "\n";
  io.run();
  std::cout << "Exit observer thread exited\n";
}

void TestProcess() {
  // use a generic thread pool
  boost::asio::thread_pool pool;
  boost::asio::io_context io;
  auto executor = io.get_executor();
  // Step 2: Block io.run() with work_gaurd
  io_context_work_gaurd work_guard = boost::asio::make_work_guard(executor);

  // start a thread and which will observer for process death.
  // The io.run() typically will be non-blocking if there is no work
  // one way to block and wait for future work is to create work_guard for
  // executor See
  std::thread exit_observer_thread(ExitObserver, std::ref(io));
  std::error_code ec;
  std::filesystem::path exe_path{"C:\\Windows\\system32\\notepad.exe"};

  auto process = base::Process::CreateProcessWithExeAndArgs(io, exe_path);

  std::cout << "Main thread Id: " << std::this_thread::get_id() << std::endl;
  std::cout << "Waiting on main thread for 10 seconds\n";
  std::this_thread::sleep_for(std::chrono::seconds(10));
  std::cout << "Done Sleeping on main thread for 10 seconds\n";

  // Even if we don't call terminate, the child destructor will call it
  if (process && process.value().NativeProcess().joinable())
    process.value().Terminate();

  // If we don't receive the terimate within 10 seconds, we reset work gaurd and
  // exit the test.
  work_guard.reset();
  io.stop();

  // after work gaurd reset, the thread will join.
  exit_observer_thread.join();
}
