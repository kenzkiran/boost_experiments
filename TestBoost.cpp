// TestBoost.cpp : This file contains the 'main' function. Program execution
// begins and ends there.

#include <iostream>

#include "pch.h"
#include "test_list.h"

// Notes: Program options need a lib
// Reference https://www.boost.org/doc/libs/1_76_0/doc/html/program_options/
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv) {

  std::cout << argc <<std::endl;
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("test", po::value<std::string>(), "test type")
    ("client", po::value<std::string>(), "client type")
    ("pipe_name", po::value<std::string>(), "name of pipe");


  // Variable Map is boosts extension of std::map 
  // Variable Value is boost::any -> https://www.boost.org/doc/libs/1_76_0/doc/html/boost/program_options/variable_value.html
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    for (const auto& [key, value] : vm) {
      std::cout << key << ":" << (value.empty() ? "null" : value.as<std::string>()) << std::endl;
    }
  } catch(...) {
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

    std::cout << "No test specified";
    return 0;
  }
 
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add
//   Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project
//   and select the .sln file
