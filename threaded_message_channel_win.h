#pragma once

#include "pch.h"

#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <winrt/base.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <variant>
#include <atomic>
#include "utilities.h"

#include "named_pipe_win.h"

#include <iostream>

struct ChannelError {
  std::string message;
  unsigned long error_code = 0;
};

enum class ChannelConnectStatus { kConnectPending, kConnected, kDisconnected };

struct PipeContext;

class ThreadedMessageChannelWin {
 public:
  struct Delegate {
    virtual void OnMessage(std::string message) = 0;
    virtual void OnError(const ChannelError& error) = 0;
    virtual void OnConnectStatus(ChannelConnectStatus status) = 0;
  };

  explicit ThreadedMessageChannelWin(NamedPipeWin named_pipe);
  ~ThreadedMessageChannelWin();
  void ReadComplete(DWORD error_code, DWORD bytes_transferred) noexcept;
  bool SendMessageOver(const std::string& message) noexcept;
  bool ConnectOnIOThread(Delegate* delegate);
  // TODO: This is the only one that reads error on main thread
  // All error condition updates happen on IO thread.
  bool IsInError() const;

 private:
  ThreadedMessageChannelWin(const ThreadedMessageChannelWin&) = delete;
  ThreadedMessageChannelWin& operator=(const ThreadedMessageChannelWin&) =
      delete;
  struct StateInit {};
  struct StateConnectPending {};
  struct StateConnected {};
  struct StateError {
    std::string message;
    DWORD error_code = 0;
    ChannelError ToChannelError() {
      ChannelError err;
      err.message = message;
      err.error_code = error_code;
      return err;
    }
  };
  using ChannelState =
      std::variant<StateInit, StateConnectPending, StateConnected, StateError>;

  bool AcceptConnection();
  bool ConnectToClient();
  void ConnectInternal();
  void DoWait();
  void DisconnectAndClose();
  void HandleIOCompletion();

  // Delegate notifications
  void NotifyMessage();
  void NotifyConnectStatus(ChannelConnectStatus status);
  void NotifyError(const ChannelError& error);

  std::thread io_thread_;
  NamedPipeWin named_pipe_;
  Delegate* delegate_ = nullptr;
  // https://github.com/microsoft/wil/wiki/Event-handles
  wil::unique_event io_event_;
  OVERLAPPED io_overlapped_;
  ChannelState state_;
  std::unique_ptr<PipeContext> context_;
  std::vector<char> read_buf_;
  std::string message_;
  std::atomic_bool quit_io_thread_{false};
};