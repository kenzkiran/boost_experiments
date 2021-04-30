// TestBoost.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <iostream>

#include <Winsock2.h>
#include <Windows.h>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
//#include <boost/process.hpp>
#include <boost/process/child.hpp>
#include <chrono>

#include "process.h"

using io_context_work_gaurd = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

std::wstring GetThreadIdToWString() {
  std::wstringstream ss;
  ss << std::this_thread::get_id();
  return ss.str();
}

void ExitObserver(boost::asio::io_context& io) {
  std::cout <<"Child thread : "<< std::this_thread::get_id() <<"\n";
  io.run();
  std::cout << "Child thread exited\n";
}
int main()
{
  std::cout << "Hello World!\n";
  boost::asio::thread_pool pool;
  boost::asio::io_context io; 
  auto executor = io.get_executor();
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard = boost::asio::make_work_guard(executor);

  std::thread exit_observer_thread(ExitObserver, std::ref(io));
  std::error_code ec;
  boost::filesystem::path exe_path{L"C:\\Windows\\system32\\notepad.exe"};

  boost::filesystem::path exe_path2{ L"C:\\Windows\\system32\\ntepad.exe" };
  auto process = base::Process::CreateProcessWithExeAndArgs(io, exe_path);
  process.value().NativeProcess().detach();
  std::cout << "Ravi created process " << &process.value() <<"\n";
  //auto process2 = base::Process::CreateProcessWithExeAndArgs(io, exe_path2);

  // boost::process::child child_process(exe_path.wstring().c_str(), ec);
  // process->NativeProcess().detach();
  
#if 0
  auto executor = pool.get_executor();
  auto work_guard = boost::asio::make_work_guard(executor);
  boost::asio::post(pool, [&] {
    std::cout << "Ravi inside task\n";
    boost::asio::post(pool, [&] {
      std::cout << "Ravi2 inside task\n";
      work_guard.reset();
      });
    });
  pool.wait();
#endif

  // boost::asio::signal_set signals(io, SIGTERM);
  // signals.async_wait([&](auto, auto) { io_context.stop(); });


  auto thread_id_w = GetThreadIdToWString();
  // std::cout << thread_id_w.c_str() << std::endl;

  std::cout << "Main thread Id: " << std::this_thread::get_id() << std::endl;
  // std::cout << "Ravi waiting on io context run\n";
  // io.run();
  // std::cout << "Ravi exiting after io context run\n";
  std::cout << "Ravi waiting on main thread for 10 seconds\n";
  std::this_thread::sleep_for(std::chrono::seconds(5));
  std::cout << "Ravi Done Sleeping on main thread for 10 seconds\n";
  // process.value().Terminate();
  work_guard.reset();
  io.stop();
  exit_observer_thread.join();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
