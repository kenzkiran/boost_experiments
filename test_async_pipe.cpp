
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <codecvt>
#undef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include  <iostream>
#include "test_list.h"

#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/nowide/convert.hpp>
#include <vector>


using io_context_work_gaurd =
boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;



const std::string kDefaultPipeName{"\\\\.\\pipe\\testpipe"};

namespace bp = boost::process;
namespace asio = boost::asio;

void StartServer(const std::string& name);
void StartClient(const std::string& name);

void TestAsyncPipe(const po::variables_map& vm) {
  auto is_server = true;
  std::string pipe_name{ kDefaultPipeName };
  if (vm.count("client")) {
    is_server = false;
  }

  if (vm.count("pipe_name")) {
    pipe_name = vm["pipe_name"].as<std::string>();
  }

  std::cout <<"Starting: " << ((is_server) ? "server" : "client") << " --> pipe name : " << pipe_name << "\n";
  if (is_server) {
    return StartServer(pipe_name);
  }
  StartClient(pipe_name);
}

void StartServer(const std::string& name) {
  asio::io_context pipe_context;
  auto executor = pipe_context.get_executor();
  // Step 2: Block io.run() with work_gaurd
  io_context_work_gaurd work_guard = boost::asio::make_work_guard(executor);

  try {
    std::cout << "Server : Creating pipe " << name <<std::endl;
    bp::async_pipe pipe(pipe_context, name);
    std::vector<char> buf(128);
    asio::async_read(pipe, boost::asio::buffer(buf),
      [&buf](const boost::system::error_code &ec, std::size_t size) {
      if (size != 0) {
        std::cout << "Server: got string from pipe: " << &buf << std::endl;
      }
      else {
        std::cerr << "error " << ec << std::endl;
      }
    });

    boost::process::child ch("C:\\Users\\rramachandra\\development\\TestBoost\\x64\\Debug\\TestBoost.exe  --test=async_pipe --client=client");
    // Have to run pipe context
    pipe_context.run();
  } catch (std::system_error &ec) {
    std::cout << "Server: system error: " << ec.what() << ", error code: " << ec.code() << std::endl;
  }
  std::cout << " Done reading....";
}


void StartClient(const std::string& name) {
  asio::io_context pipe_context;
  try {
    std::cout << "Client : Creating pipe " << name <<std::endl;
    bp::async_pipe pipe(pipe_context, name);
    boost::asio::streambuf buf;
    std::ostream os(&buf);
    os << "Hello, World from Client!\n";

    asio::async_write(pipe, buf.data(),
      [&buf](const boost::system::error_code &ec, std::size_t size) {
      if (size != 0) {
        std::cout << "Client: sent string from pipe: " << &buf << size <<std::endl;
      }
      else {
        std::cerr << "error " << ec << std::endl;
      }
    });

    // Have to run pipe context
    pipe_context.run();
  } catch (std::system_error &ec) {
    std::cout << "Client: system error: " << ec.what() << ", error code: " << ec.code() << std::endl;
  }
  std::cout << " Done writing....";
}