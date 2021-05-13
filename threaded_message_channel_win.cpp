#include "threaded_message_channel_win.h"

constexpr int kReadBufSize = 4 * 1024;  // 4K

struct PipeContext : OVERLAPPED {
  ThreadedMessageChannelWin* owner{};
};

using PipeContextPtr = PipeContext*;

void DoReadCompleteRoutine(DWORD error_code,
                           DWORD bytes_transferred,
                           LPOVERLAPPED ctx) {
  PipeContextPtr context = (PipeContextPtr)ctx;
  context->owner->ReadComplete(error_code, bytes_transferred);
}

ThreadedMessageChannelWin::ThreadedMessageChannelWin(NamedPipeWin pipe)
    : named_pipe_(std::move(pipe)) {
  if (!named_pipe_.IsValid()) {
    throw std::invalid_argument("Invalid Named Pipe Passed");
  }
  // As per documentation here
  // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe
  // The event should be a manual reset event.
  io_event_.create(wil::EventOptions::ManualReset |
                   wil::EventOptions::Signaled);

  io_overlapped_.hEvent = io_event_.get();
  context_ = std::make_unique<PipeContext>();
  context_->owner = this;
  read_buf_.reserve(kReadBufSize);
}

ThreadedMessageChannelWin::~ThreadedMessageChannelWin() {
  std::cout << "~TMCW Destructor " << std::endl;
  // This should cause the IO Thread loop to break
  // due pipe broken error
  DisconnectAndClose();
  quit_io_thread_.store(true);
  io_event_.SetEvent();
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
}

bool ThreadedMessageChannelWin::IsInError() const {
  return std::holds_alternative<StateError>(state_);
}

bool ThreadedMessageChannelWin::ConnectToClient() {
  // We should be "init" state.
  // If we are in any other state, we have to explicitly
  // call Disconnect to drop current connection and start
  // new connection. This should also be called on IOThread.
  if (!std::holds_alternative<StateInit>(state_)) {
    throw std::logic_error{
        "Connection can only be established from Init State"};
  }

  std::cout << "Now connecting to client " << std::endl;
  BOOL connect_result = ConnectNamedPipe(named_pipe_.handle(), &io_overlapped_);
  auto gle = GetLastError();

  if (connect_result) {
    std::cout << "Error in ConnectNamedPipe : GLE " << gle << std::endl;
    auto error = StateError{std::string("ConnectNamedPipe failed"), gle};
    state_ = error;
    NotifyError(error.ToChannelError());
    return false;
  }

  if (gle == ERROR_IO_PENDING) {
    state_ = StateConnectPending();
    NotifyConnectStatus(ChannelConnectStatus::kConnectPending);
    std::cout << "Named Pipe Connect Pending" << std::endl;
  } else if (gle == ERROR_PIPE_CONNECTED) {
    std::cout << "Named Pipe Connected" << std::endl;
    state_ = StateConnected();
    NotifyConnectStatus(ChannelConnectStatus::kConnected);
    ReadComplete(0, 0);
  } else {
    auto error = StateError{std::string("ConnectNamedPipe failed"), gle};
    NotifyError(error.ToChannelError());
    return false;
  }
  return true;
}

bool ThreadedMessageChannelWin::SendMessageOver(
    const std::string& message) noexcept {
  if (!std::holds_alternative<StateConnected>(state_)) {
    std::cout << " Not in the right state, failed to send message" << std::endl;
    return false;
  }

  DWORD bytes_written{0};
  BOOL success = WriteFile(named_pipe_.handle(),  // pipe handle
                           message.data(),        // message
                           message.size(),        // message length
                           &bytes_written,        // bytes written
                           NULL);

  if (!success) {
    std::cout << "Error : WriteFile to pipe failed. GLE" << GetLastError()
              << std::endl;
    state_ = StateError{std::string("WriteFile error"), GetLastError()};
    // Should we call OnError since we are synchronously telling the API failed
    // ??
    return false;
  }
  return true;
}

void ThreadedMessageChannelWin::DoWait() {
  bool quit{false};
  while (!quit) {
    std::cout << "Ravi Inside while loop" << std::endl;
    DWORD wait = ::WaitForSingleObjectEx(io_event_.get(), INFINITE, TRUE);
    switch (wait) {
      case WAIT_OBJECT_0: {
        if (quit_io_thread_.load()) {
          quit = true;
          break;
        }
        // case 1: We were waiting for the connection.
        if (std::holds_alternative<StateConnectPending>(state_)) {
          if (!AcceptConnection()) {
            quit = true;
          }
        }
        break;
      }  // case WAIT_OBJECT_0

      case WAIT_IO_COMPLETION: {
        HandleIOCompletion();
        break;
      }
      default: {
        std::cout << "Signaled (May be to exit) ..." << std::endl;
        quit = true;
      }
    }
  }
  std::cout << "IO Thread exit XXXXXXXXXXXXXXX " << std::endl;
}

void ThreadedMessageChannelWin::HandleIOCompletion() {
  // If IO failed, we would have set the error.
  if (auto ptr = std::get_if<StateError>(&state_)) {
    if (ptr->error_code == ERROR_BROKEN_PIPE ||
        ptr->error_code == ERROR_PIPE_NOT_CONNECTED) {
      NotifyConnectStatus(ChannelConnectStatus::kDisconnected);
    } else {
      NotifyError(ptr->ToChannelError());
    }
    return;
  }
  std::cout << "Ravi HandleIOCompletion " << message_.size() << std::endl;
  NotifyMessage();
}

bool ThreadedMessageChannelWin::AcceptConnection() {
  _ASSERT_EXPR(std::holds_alternative<StateConnectPending>(state_),
               L"Invalid State Operation: Accept connection should only happen "
               L"in StateConnectPending");

  std::cout << "Ravi inside Accept Connection" << std::endl;
  DWORD bytes_transfered{0};
  auto success = ::GetOverlappedResult(named_pipe_.handle(), &io_overlapped_,
                                       &bytes_transfered, FALSE);
  if (!success) {
    std::cout << " Failed to GetOverlappedResult " << GetLastError()
              << std::endl;
    auto error =
        StateError{std::string("Error in GetOverlappedResult"), GetLastError()};
    state_ = error;
    NotifyError(error.ToChannelError());
    return false;
  }

  std::cout << "Connection Established Successfully, kicking off Read "
            << std::endl;
  state_ = StateConnected();
  io_event_.ResetEvent();
  // if we are here connection was successful, kick off Read
  ReadComplete(0, 0);
  return true;
}

void ThreadedMessageChannelWin::ReadComplete(DWORD error_code,
                                             DWORD bytes_transferred) noexcept {
  static int received_count = -1;
  // Check if bytes transferred is <= what we are expecting.
  if (error_code) {
    state_ = StateError{std::string("ReadComplete Error"), error_code};
    DisconnectAndClose();
  } else {
    if (bytes_transferred > 0) {
      message_.append(read_buf_.data(), bytes_transferred);
      read_buf_.clear();
    }

    BOOL success =
        ReadFileEx(named_pipe_.handle(), read_buf_.data(), kReadBufSize,
                   (LPOVERLAPPED)context_.get(), DoReadCompleteRoutine);
    if (!success) {
      state_ = StateError{std::string("ReadFileEx Error "), GetLastError()};
      DisconnectAndClose();
    }
  }
}

void ThreadedMessageChannelWin::ConnectInternal() {
  auto is_server = named_pipe_.IsServer();
  if (!is_server) {
    std::cout << " Could not figure out if we were a server " << std::endl;
    return;
  }
  if (is_server.value()) {
    if (!ConnectToClient()) {
      std::cout << "Error in connecting to client" << std::endl;
      return;
    }
  } else {
    state_ = StateConnected();
    io_event_.ResetEvent();
    ReadComplete(0, 0);
  }
  DoWait();
}

bool ThreadedMessageChannelWin::ConnectOnIOThread(Delegate* delegate) {
  if (io_thread_.joinable()) {
    std::cout << "IO thread active" << std::endl;
    return false;
  }

  delegate_ = delegate;
  _ASSERT_EXPR(delegate_ != nullptr, L"Null delegate");

  state_ = StateInit();
  io_thread_ = std::thread([this] { ConnectInternal(); });
  return true;
}

void ThreadedMessageChannelWin::DisconnectAndClose() {
  message_.clear();
  named_pipe_.DisconnectAndClose();
}

void ThreadedMessageChannelWin::NotifyMessage() {
  if (message_.empty()) {
    return;
  }
  if (delegate_) {
    delegate_->OnMessage(std::move(message_));
  }
}

void ThreadedMessageChannelWin::NotifyConnectStatus(
    ChannelConnectStatus status) {
  if (delegate_) {
    delegate_->OnConnectStatus(status);
  }
}

void ThreadedMessageChannelWin::NotifyError(const ChannelError& error) {
  if (delegate_) {
    delegate_->OnError(error);
  }
}
