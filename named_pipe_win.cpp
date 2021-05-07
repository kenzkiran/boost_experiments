#include "named_pipe_win.h"
#include "utilities.h"

#include <iostream>

constexpr LPCWSTR kPipeNamePrefix = L"\\\\.\\pipe\\LOCAL\\maglev.";
constexpr DWORD kDefaultTimeout = 5000;  // in ms
constexpr LPCWSTR kDefaultTestPipeName = L"\\\\.\\pipe\\mynamedpipe";

namespace {
std::wstring PipeName(const std::string& name) {
  std::wstring pipe_name{kPipeNamePrefix};
  // PS: Should we check, name validities ?
  pipe_name += utils::Utf8ToUtf16(name);
  return pipe_name;
}

}  // namespace

// TODO: On need basis we can take other parameters later.

// Move to std::optional for return value.
// static
[[nodiscard]] NamedPipeWin NamedPipeWin::CreateServerPipe(
    const std::string& name, unsigned buf_size) {
  auto pipe_name =  PipeName(name);
  std::cout << "Ravi Creating Server Pipe " << utils::Utf16ToUtf8(pipe_name)
            << std::endl;
  DWORD open_mode =
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;  // read & write and overlapped
  DWORD pipe_mode =
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
      PIPE_WAIT;  // message-type pipe, message read mode, blocking wait
  HANDLE pipe_handle = CreateNamedPipeW(
      kDefaultTestPipeName,  // pipe_name.c_str(),      // pipe name
      open_mode, pipe_mode,
      1,  // only one instance
      buf_size, // out buf
      buf_size, // in buf
      kDefaultTimeout,  // client time-out in ms
      NULL);            // default security attributes
  if (pipe_handle == INVALID_HANDLE_VALUE) {
    std::cout << "Failed to Create Server Pipe : GLE " << GetLastError()
              << std::endl;
    return NamedPipeWin();
  }

  std::cout << "Ravi CreateServer Handle " << pipe_handle <<std::endl;
  // TODO: Ensure if 'DisconnectNamedPipe' is needed or not.
  // From the documentation if we don't intend to re-use the same server
  // pipe to connect to a different client, we don't need it.
  // unique_handle does call CloseHandle()
  // if needed move to wil::unique_any
  return NamedPipeWin(wil::unique_handle(pipe_handle));
}

// TODO: Move to std::optional. May be Throw is better here.
// static
[[nodiscard]] NamedPipeWin NamedPipeWin::CreateClientPipe(
    const std::string& name) {
  auto pipe_name = PipeName(name);
  std::cout << "Ravi Creating Client Pipe " << utils::Utf16ToUtf8(pipe_name)
            << std::endl;
  HANDLE pipe_handle =
      CreateFile(kDefaultTestPipeName,  // pipe_name.c_str(),   // pipe name
                 GENERIC_READ | GENERIC_WRITE,  // read and write
                 0,                             // no sharing
                 NULL,                          // default security attributes
                 OPEN_EXISTING,                 // opens existing pipe
                 FILE_FLAG_OVERLAPPED,                             // default attributes
                 NULL);                         // no template file

  if (pipe_handle == INVALID_HANDLE_VALUE) {
    std::cout << "Failed to Create Server Pipe : GLE " << GetLastError();
    return NamedPipeWin();
  }

  std::cout << "Ravi CreateClient Handle " << pipe_handle << std::endl;
  return NamedPipeWin(wil::unique_handle(pipe_handle));
}

NamedPipeWin::NamedPipeWin(wil::unique_handle handle) : pipe_(std::move(handle)) {}

bool NamedPipeWin::IsValid() const {
  return pipe_.is_valid();
}

std::optional<bool> NamedPipeWin::IsServer() const {
  if (!IsValid()) {
    return std::nullopt;
  }
  DWORD flags;
  if (!::GetNamedPipeInfo(pipe_.get(), &flags, NULL, NULL, NULL)) {
    std::cout << "Error in GetNamedPipeInfo GLE: " << GetLastError();
    return std::nullopt;
  }

  return (flags & PIPE_SERVER_END) != 0x0;
}

bool NamedPipeWin::DisconnectAndClose() noexcept {
  if (!IsValid()) {
    return false;
  }

  std::cout <<"Disconnect and Close on : " << handle();
  auto is_server = IsServer();
  bool res{ true };
  if (is_server && is_server.value()) {
    if (!::DisconnectNamedPipe(pipe_.get())) {
      std::cout << "Error in DisconnectNamedPipe " <<  GetLastError();
      res = false;
    }
  }
  pipe_.reset();
  return res;
}

NamedPipeWin::~NamedPipeWin() {
  DisconnectAndClose();
}
