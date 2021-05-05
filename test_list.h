#pragma once

#include <boost/program_options.hpp>
#include "named_pipe.h"

namespace po = boost::program_options;

extern void TestProcess();
extern void TestAsyncPipe(const po::variables_map& vm);
extern void TestNamedPipe(const po::variables_map& vm);
extern int StartCompletionRoutineServer();