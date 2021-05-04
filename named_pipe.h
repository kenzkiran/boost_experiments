#pragma once
#include "test_list.h"

namespace po = boost::program_options;


namespace win_named_pipe {
  extern int StartNamedPipeClient();
  extern int StartNamedPipeServer();
}

