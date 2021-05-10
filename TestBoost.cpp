// TestBoost.cpp : This file contains the 'main' function. Program execution
// begins and ends there.

#include <iostream>
#include <random>

#include "named_pipe_win.h"
#include "pch.h"
#include "test_list.h"
#include "threaded_message_channel_win.h"

// Notes: Program options need a lib
// Reference https://www.boost.org/doc/libs/1_76_0/doc/html/program_options/
#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::random_device
    rd;  // Will be used to obtain a seed for the random number engine
std::mt19937 gen(rd());  // Standard mersenne_twister_engine seeded with rd()

auto CreateAsciiVector(size_t size, bool random = false) {
  std::string output;
  output.reserve(size);
  for (auto i = 0; i < size; ++i) {
    if (random) {
      std::uniform_int_distribution<> distrib(21, 126);
      output.push_back(distrib(gen));
    } else {
      output.push_back('a' + (i % 26));
    }
  }
  return output;
}

int GenericTest(int argc, char** argv);
int PipeTests(int argc, char** argv);

int main(int argc, char** argv) {
  PipeTests(argc, argv);
  return 0;
}

std::string ConnectStatusToString(const ChannelConnectStatus& status) {
  if (status == ChannelConnectStatus::kConnected) {
    return "Connected";
  }
  if (status == ChannelConnectStatus::kDisconnected) {
    return "Disconnected";
  }
  return "ConnectPending";
}

int PipeTests(int argc, char** argv) {
  std::cout << argc << std::endl;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")("client", "client type")(
      "num_messages", po::value<int>()->default_value(1), "num_messages")(
      "message_size", po::value<unsigned>()->default_value(1024),
      "message_size to be sent")(
      "sleep_after_tests", po::value<int>()->default_value(0),
      "sleep after tests")("dont_exit", "does not exit");

  // Variable Map is boosts extension of std::map
  // Variable Value is boost::any ->
  // https://www.boost.org/doc/libs/1_76_0/doc/html/boost/program_options/variable_value.html
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    /* for (const auto& [key, value] : vm) {
       std::cout << key << ":" << (value.empty() ? "null" :
     value.as<std::string>()) << std::endl;
     }*/
  } catch (...) {
    std::cout << "Exception in parsing...";
    return 0;
  }

  bool is_client = vm.count("client") != 0;
  const std::string entity = is_client ? "Client" : "Server";

  struct StubDelegate : public ThreadedMessageChannelWin::Delegate {
    void OnMessage(std::string message) override {
      std::cout << "Delegate Received Message : " << message.size()
                << std::endl;
    }
    void OnError(const ChannelError& error) override {
      std::cout << "Delegate Received Error " << error.message;
    }
    void OnConnectStatus(ChannelConnectStatus status) {
      std::cout << "Delegate Received Connect Status "
                << ConnectStatusToString(status);
    }
  };

  StubDelegate stub_delegate;
  NamedPipeWin pipe = is_client ? NamedPipeWin::CreateClientPipe("some_name")
                                : NamedPipeWin::CreateServerPipe("some_name");
  if (!pipe.IsValid()) {
    std::cout <<"Pipe creation failed "<<std::endl;
    return 0;
  }
  auto tmc = std::shared_ptr<ThreadedMessageChannelWin>(
      new ThreadedMessageChannelWin(std::move(pipe)));
  tmc->ConnectOnIOThread(&stub_delegate);

  auto num_messages = vm["num_messages"].as<int>();

  unsigned int sleep_time = is_client ? 5 : 10;
  std::cout << "Waiting for connections  " << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(sleep_time));
  std::cout << "Done waiting connections " << std::endl;

  std::cout << entity << ": Sending " << num_messages << std::endl;
  unsigned message_size = vm["message_size"].as<unsigned>();
  auto message = CreateAsciiVector(message_size);
  for (auto i = 0; i < num_messages; ++i) {
    std::cout << "Sending message : " << i << std::endl;
    if (!tmc->SendMessageOver(message)) {
      std::cout << "Send message failed: " << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (tmc->IsInError()) {
    std::cout << "TMC is in Error, quitting " << std::endl;
    return 0;
  }
  while (1) {
    std::cout << "sleeping..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(100));
  }
}

int GenericTest(int argc, char** argv) {
  std::cout << argc << std::endl;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "test", po::value<std::string>(), "test type")(
      "client", po::value<std::string>(), "client type")(
      "pipe_name", po::value<std::string>(), "name of pipe");

  // Variable Map is boosts extension of std::map
  // Variable Value is boost::any ->
  // https://www.boost.org/doc/libs/1_76_0/doc/html/boost/program_options/variable_value.html
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    for (const auto& [key, value] : vm) {
      std::cout << key << ":"
                << (value.empty() ? "null" : value.as<std::string>())
                << std::endl;
    }
  } catch (...) {
    std::cout << "Exception in parsing...";
    return 0;
  }

  if (vm.count("test")) {
    auto test = vm["test"].as<std::string>();

    if (test == "process") {
      TestProcess();
      return 0;
    }
    if (test == "async_pipe") {
      TestAsyncPipe(vm);
      return 0;
    }

    if (test == "win_named_pipe") {
      TestNamedPipe(vm);
      return 0;
    }

    if (test == "completion_server") {
      StartCompletionRoutineServer();
      return 0;
    }

    if (test == "tmcw") {
      std::string name{"some_pipe"};
      auto pipe = NamedPipeWin::CreateServerPipe(name);
      auto tmc = std::make_shared<ThreadedMessageChannelWin>(std::move(pipe));
      tmc->ConnectOnIOThread(nullptr);
      std::this_thread::sleep_for(std::chrono::seconds(10));
      return 0;
    }

    std::cout << "No test specified";
    return 0;
  }
}
