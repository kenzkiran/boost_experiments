#include "threaded_message_channel_win.h"

#include <vector>

constexpr int BUFSIZE = 512;
std::vector<char> read_buf2(BUFSIZE);
OVERLAPPED oConnect2;

void DoReadCompleteRoutine(DWORD error_code,
                          DWORD bytes_transferred,
                          LPOVERLAPPED ctx) {
  std::cout << "Received Completion Routine " << std::this_thread::get_id()<<std::endl;
  PipeContextPtr context = (PipeContextPtr)ctx;
  context->owner->ReadComplete(error_code, bytes_transferred);
}

// The input must be a OVERLAPPED PIPE
// TODO: How to ensure that ? Can we retrieve that info from the pipe ?
// GetPipeInfo did not seem to provide.
ThreadedMessageChannelWin::ThreadedMessageChannelWin(NamedPipeWin pipe)
    : named_pipe_(std::move(pipe)) {
  // As per documentation here
  // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe
  // The event should be a manual reset event.
  // FIXME: Research or ask: Should we not use CreateEventEx
  io_event_ = ::winrt::handle(
      ::CreateEvent(NULL,  // default security attribute
                    TRUE,  // manual reset event
                    TRUE,  // initial state = signaled (This is how example did.
                           // Need some re-thinking here)
                    NULL));  // unnamed event object
  ::winrt::check_bool(bool{io_event_});
  // Assign this to overlapped.
  io_overlapped_.hEvent = io_event_.get();
  context_.reset((PipeContextPtr)GlobalAlloc(GPTR, sizeof(PipeContext)));
  context_->owner = this;
}

ThreadedMessageChannelWin::~ThreadedMessageChannelWin() {
  std::cout << "DDDDDDDDDDD  ~TMCW Destructor " << std::endl;
  // This should cause the IO Thread loop to break
  // due pipe broken error
  named_pipe_.DisconnectAndClose();

  // Remove once we move to unique_event
  io_event_.close();

  if (io_thread_.joinable()) {
    io_thread_.join();
  }
}

bool ThreadedMessageChannelWin::ConnectToClient() {
  // We should be "init" state.
  // If we are in any other state, we have to explicitly
  // call Disconnect to drop current connection and start
  // new connection. This should also be called on IOThread.
  if (!std::holds_alternative<ChannelInit>(state_)) {
    throw std::logic_error{
        "Connection can only be established from Init State"};
  }

  BOOL connect_result = ConnectNamedPipe(named_pipe_.handle(), &io_overlapped_);
  auto gle = GetLastError();
  if (connect_result) {
    std::cout << "Error in ConnectNamedPipe : GLE " << gle;
    state_ = ChannelError{std::string("ConnectNamedPipe failed"), gle};
    // TODO: Notify over OnErrorCallback
    return false;
  }

  bool is_pending_or_connected =
      gle == ERROR_IO_PENDING || gle == ERROR_PIPE_CONNECTED;
  if (!is_pending_or_connected) {
    state_ = ChannelError{std::string("ConnectNamedPipe failed"), gle};
    return false;
  }

  if (gle == ERROR_IO_PENDING) {
    state_ = ChannelConnectPending();
  } else {
    state_ = ChannelConnected();
    winrt::check_bool(::SetEvent(io_event_.get()));
  }
  return true;
}

void ThreadedMessageChannelWin::DoWait() {
  bool quit{false};
  while (!quit) {
    DWORD bytes_transfered{0};
    DWORD wait = WaitForSingleObjectEx(io_event_.get(), INFINITE, TRUE);
    switch (wait) {
      case WAIT_OBJECT_0: {
        // case 1: We were waiting for the connection.
        if (std::holds_alternative<ChannelConnectPending>(state_)) {
          auto success = GetOverlappedResult(named_pipe_.handle(), &io_overlapped_,
                                             &bytes_transfered, FALSE);
          if (!success) {
            std::cout << " Failed to GetOverlappedResult " << GetLastError()
                      << std::endl;
            state_ =
                ChannelError{std::string("Range error in WaitMultipleObjects"),
                             GetLastError()};
            quit = true;  // No point continuing..
          } else {
            std::cout << "Connection Established Successfully" << std::endl;
            state_ = ChannelConnected();
            // if we are here connection was successful, kick off Read
            ReadComplete(0, 0);
          }
        }
        break;
      }  // case WAIT_OBJECT_0

      case WAIT_IO_COMPLETION: {
        std::cout << "Ravi in IO Completion " << std::endl;
        if (auto ptr = std::get_if<ChannelError>(&state_)) {
          if (ptr->error_code == ERROR_BROKEN_PIPE) {
            std::cout << "Broken Pipe Detected " << std::endl;
          } else {
            std::cout << "Some other Error: " << ptr->message << ":"
                      << ptr->error_code << std::endl;
          }
        }
        break;
      }
      default: {
        std::cout << "Some on signaled (May be exit) ..." << std::endl;
        quit = true;
      }
    }
  }
  std::cout << "IO Thread exit XXXXXXXXXXXXXXX " << std::endl;
}

void ThreadedMessageChannelWin::ReadComplete(DWORD error_code,
                                             DWORD bytes_transferred) noexcept {
  // Check if bytes transferred is <= what we are expecting.
  if (error_code) {
    std::cout << "Ravi ReadComplete Errored out " << error_code;
    state_ = ChannelError{std::string("ReadComplete Error "), error_code};
    DisconnectAndClose();
  } else {
    std::cout << "Ravi ReadComplete Routine, read " << bytes_transferred
              << named_pipe_.handle() << std::endl;
    BOOL success =
        ReadFileEx(named_pipe_.handle(), read_buf2.data(), BUFSIZE,
                   (LPOVERLAPPED)context_.get(), DoReadCompleteRoutine);
    if (!success) {
      std::cout << "Ravi failed to ReadFileEx " << GetLastError() << std::endl;
      state_ = ChannelError{std::string("ReadFileEx Error "), GetLastError()};
      DisconnectAndClose();
    } else {
      std::string msg(read_buf2.data(), bytes_transferred);
      std::cout <<"Received Message : " << msg <<std::endl;
    }
  }
}

void ThreadedMessageChannelWin::ConnectInternal() {
  if (!ConnectToClient()) {
    std::cout << "Error in connecting to client" << std::endl;
    return;
  }
  DoWait();
}

void ThreadedMessageChannelWin::ConnectOnIOThread() {
  if (io_thread_.joinable()) {
    std::cout << "Already started the IO thread " << std::endl;
    return;
  }

  io_thread_ = std::thread([this] { ConnectInternal(); });
}

void ThreadedMessageChannelWin::DisconnectAndClose() {
  named_pipe_.DisconnectAndClose();
}