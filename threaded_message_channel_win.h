#pragma once

#include "pch.h"

#include <memory>
#include <iostream>
#include <thread>
#include <string>
#include <variant>
#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <winrt/base.h>
#include "utilities.h"

#include "named_pipe_win.h"

#include <iostream>


class ThreadedMessageChannelWin;
typedef struct  { 
   OVERLAPPED oOverlap;
   ThreadedMessageChannelWin* owner;
} PipeContext, *PipeContextPtr;

class ThreadedMessageChannelWin : public std::enable_shared_from_this<ThreadedMessageChannelWin> {
public:

struct ChannelInit {};
struct ChannelConnectPending {};
struct ChannelConnected {};
struct ChannelError {
  std::string message;
  DWORD error_code = 0; // from GLE
};

using MessageChannelState = std::variant<ChannelInit, ChannelConnectPending, ChannelConnected, ChannelError>;

public:
  ThreadedMessageChannelWin(NamedPipeWin named_pipe);
  ~ThreadedMessageChannelWin();
  void ReadComplete(DWORD error_code, DWORD bytes_transferred) noexcept;
  bool SendMessageOver(const std::string& message) noexcept;
  void ConnectOnIOThread();
  bool IsInError() const {
    return std::holds_alternative<ChannelError>(state_);
  }
private:
  bool ConnectToClient();
  void ConnectInternal();
  void DoWait();
  void DisconnectAndClose();
  std::thread io_thread_;
  NamedPipeWin named_pipe_;
  // TODO: https://github.com/microsoft/wil/wiki/Event-handles
  // Not sure how to use SetEvent_scope_exit()
  // using vanila event for now.
  // There is also a winrt::handle that provides some nice wrappers
  // to check error and throw GLE
  winrt::handle io_event_;
  OVERLAPPED io_overlapped_;
  MessageChannelState state_;
  wil::unique_hglobal_ptr<PipeContext> context_;
};