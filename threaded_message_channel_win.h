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


constexpr LPCWSTR kPipeNamePrefix = L"\\\\.\\pipe\\LOCAL\\maglev.";
constexpr DWORD kDefaultBufferSize = 4 * 1024;  // 4k
constexpr DWORD kDefaultTimeout = 5000;         // in ms
constexpr LPCWSTR same_pipe_name = L"\\\\.\\pipe\\mynamedpipe";

static std::wstring PipeName(const std::string& name) {
  std::wstring pipe_name{ kPipeNamePrefix };
  // PS: Should we check, name validities ?
  pipe_name += utils::Utf8ToUtf16(name);
  return pipe_name;
}

class ThreadedMessageChannelWin;
typedef struct 
{ 
   OVERLAPPED overlapped;
   ThreadedMessageChannelWin* owner;
} PipeContext, *PipeContextPtr;


class ThreadedMessageChannelWin : public std::enable_shared_from_this<ThreadedMessageChannelWin> {
public:
[[nodiscard]] static wil::unique_handle CreateClientPipe(const std::string& name) {
  auto pipe_name = PipeName(name);
  std::cout << "Ravi Creating Client Pipe " << utils::Utf16ToUtf8(pipe_name) << std::endl;
  HANDLE pipe_handle = CreateFile(
    pipe_name.c_str(),   // pipe name 
    GENERIC_READ | GENERIC_WRITE,  // read and write
    0,              // no sharing 
    NULL,           // default security attributes
    OPEN_EXISTING,  // opens existing pipe 
    0,              // default attributes 
    NULL);          // no template file

    if (pipe_handle == INVALID_HANDLE_VALUE) {
      std::cout <<"Failed to Create Server Pipe : GLE " <<GetLastError();
      return {};
    }
    return wil::unique_handle(pipe_handle);
}

// TODO: On need basis we can take other parameters later.
[[nodiscard]] static wil::unique_handle CreateServerPipe(const std::string& name) {
  auto pipe_name = PipeName(name);
  std::cout << "Ravi Creating Server Pipe " << utils::Utf16ToUtf8(pipe_name)
            << std::endl;
  DWORD open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED; // read & write and overlapped
  DWORD pipe_mode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT; // message-type pipe, message read mode, blocking wait
  HANDLE pipe_handle =
      CreateNamedPipe(same_pipe_name, //pipe_name.c_str(),      // pipe name
                      open_mode,
                      pipe_mode,
                      1,                   // only one instance
                      kDefaultBufferSize,  // output buffer size
                      kDefaultBufferSize,  // input buffer size
                      kDefaultTimeout,     // client time-out in ms
                      NULL);  // default security attributes
  if (pipe_handle == INVALID_HANDLE_VALUE) {
    std::cout <<"Failed to Create Server Pipe : GLE " <<GetLastError() << std::endl;
    return {};
  }
  // TODO: Ensure if 'DisconnectNamedPipe' is needed or not.
  // From the documentation if we don't intend to re-use the same server
  // pipe to connect to a different client, we don't need it.
  // unique_handle does call CloseHandle()
  // if needed move to wil::unique_any
  return wil::unique_handle(pipe_handle);
}

struct ChannelInit {};
struct ChannelConnectPending {};
struct ChannelConnected {};
struct ChannelError {
  std::string message;
  DWORD error_code = 0; // from GLE
};

using MessageChannelState = std::variant<ChannelInit, ChannelConnectPending, ChannelConnected, ChannelError>;

public:
  ThreadedMessageChannelWin(wil::unique_handle pipe);
  ~ThreadedMessageChannelWin();
  void ReadComplete(DWORD error_code, DWORD bytes_transferred) noexcept;
  void SendMessageOver(const std::string& message) noexcept {}
  void ConnectOnIOThread();
private:
  bool ConnectToClient();
  void ConnectInternal();
  void DoWait();
  void DisconnectAndClose();
  std::thread io_thread_;
  wil::unique_handle pipe_;
  // TODO: https://github.com/microsoft/wil/wiki/Event-handles
  // Not sure how to use SetEvent_scope_exit()
  // using vanila event for now.
  // There is also a winrt::handle that provides some nice wrappers
  // to check error and throw GLE
  winrt::handle io_event_;
  OVERLAPPED io_overlapped_;
  MessageChannelState state_;
  PipeContext context_;
};