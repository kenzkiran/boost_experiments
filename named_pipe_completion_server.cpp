#include <iostream>
#include <thread>
#include "pch.h"

#include "test_list.h"

#define PIPE_TIMEOUT 5000
enum class ConnectionStatus { kConnecting, kConnected, kReading, kError };

constexpr int BUFSIZE = 512;
HANDLE hPipe;
HANDLE hConnectEvent;
HANDLE hPipeEvents;
OVERLAPPED oConnect;
DWORD dwWait, cbRet;
std::vector<char> read_buf(BUFSIZE);

std::thread io_thread;
ConnectionStatus connect_status{ConnectionStatus::kConnecting};

void DisconnectAndClose();
BOOL CreateAndConnectInstance();

int StartCompletionRoutineServer() {
  hPipeEvents = CreateEvent(NULL,   // default security attribute
                            FALSE,  // manual reset event
                            FALSE,  // initial state = not signaled
                            NULL);  // unnamed event object

  bool quit = false;
  CreateAndConnectInstance();
  while (!quit) {
    std::cout << "Wait for Events " << std::endl;
    DWORD result = WaitForSingleObjectEx(hPipeEvents, INFINITE, TRUE);
    switch (result) {
      case WAIT_OBJECT_0:
        std::cout << "Got some event: " << std::endl;
        if (connect_status == ConnectionStatus::kError) {
          std::cout << "Some operation failed see logs ";
          quit = true;
        } else if (connect_status == ConnectionStatus::kConnected) {
          std::cout << "Connection established...";
        }
        break;
      case WAIT_FAILED:
        std::cout << "Error waiting GLE: " << GetLastError();
        quit = true;
        return 0;
    }
  }
  if (io_thread.joinable()) {
    io_thread.join();
  }
  std::cout << "Ravi Exiting server ..." << std::endl;
  return 0;
}

void ReadCompleteRoutine(DWORD dwErrorCode,
                         DWORD dwNumberOfBytesTransfered,
                         LPOVERLAPPED lpOverlapped) {
  if (dwErrorCode) {
    std::cout << "Error in Read " << dwErrorCode;
    connect_status = ConnectionStatus::kError;
    DisconnectAndClose();
  } else {
    std::cout << "Ravi ReadComplete Routine, read " << dwNumberOfBytesTransfered
              << std::endl;
    BOOL success = ReadFileEx(hPipe, read_buf.data(), BUFSIZE, lpOverlapped,
                              ReadCompleteRoutine);
    if (!success) {
      connect_status = ConnectionStatus::kError;
      DisconnectAndClose();
    }
  }
}

// run on a IO thread.
bool ConnectToNewClient() {
  std::cout << " Ravi ConnectToNewClient on a new thread " << std::endl;
  // Create one event object for the connect operation.
  hConnectEvent = CreateEvent(NULL,   // default security attribute
                              FALSE,  // manual reset event
                              FALSE,  // initial state = signaled
                              NULL);  // unnamed event object

  if (hConnectEvent == NULL) {
    printf("CreateEvent failed with %d.\n", GetLastError());
    connect_status = ConnectionStatus::kError;
    return 0;
  }
  oConnect.hEvent = hConnectEvent;

  std::cout << "Ravi Connecting to Named Pipe " << std::this_thread::get_id()
            << std::endl;
  BOOL connect_result = ConnectNamedPipe(hPipe, &oConnect);
  if (connect_result) {
    return false;
  }

  if (GetLastError() == ERROR_PIPE_CONNECTED) {
    connect_status = ConnectionStatus::kConnected;
    // Check return value
    SetEvent(hPipeEvents);
  }

  bool exit_thread{false};
  DWORD dwWait, cbRet;
  while (!exit_thread) {
    std::cout << "Ravi Waiting on Receiver thread \n";
    DWORD dwWait = WaitForSingleObjectEx(hConnectEvent, INFINITE, TRUE);

    switch (dwWait) {
      case WAIT_OBJECT_0: {
        if (connect_status == ConnectionStatus::kConnecting) {
          BOOL fSuccess =
              GetOverlappedResult(hPipe,      // pipe handle
                                  &oConnect,  // OVERLAPPED structure
                                  &cbRet,     // bytes transferred
                                  FALSE);     // does not wait
          if (!fSuccess) {
            printf("ConnectNamedPipe (%d)\n", GetLastError());
            SetEvent(hPipeEvents);
            connect_status = ConnectionStatus::kError;
            exit_thread = true;
            break;
          } else {
            std::cout << " Connected to a client successfully " << std::endl;
            connect_status = ConnectionStatus::kConnected;
            ReadCompleteRoutine(0, 0, &oConnect);
            SetEvent(hPipeEvents);
          }
        }
      }

      case WAIT_IO_COMPLETION:
        std::cout << "Ravi wait for IO completion " << std::endl;
        if (connect_status == ConnectionStatus::kError) {
          std::cout << "Error in read, quiting receiver thread...\n";
          SetEvent(hPipeEvents);
          exit_thread = true;
        }
        break;

      default:
        std::cout << "Ravi connection error detected, exiting..." << std::endl;
        connect_status = ConnectionStatus::kError;
        exit_thread = true;
        break;
    }
  }
  return true;
}

BOOL CreateAndConnectInstance() {
  LPCWSTR lpszPipename = L"\\\\.\\pipe\\mynamedpipe";

  hPipe = CreateNamedPipe(lpszPipename,                // pipe name
                          PIPE_ACCESS_DUPLEX |         // read/write access
                              FILE_FLAG_OVERLAPPED,    // overlapped mode
                          PIPE_TYPE_MESSAGE |          // message-type pipe
                              PIPE_READMODE_MESSAGE |  // message read mode
                              PIPE_WAIT,               // blocking mode
                          1,                           // only one instances
                          BUFSIZE * sizeof(TCHAR),     // output buffer size
                          BUFSIZE * sizeof(TCHAR),     // input buffer size
                          PIPE_TIMEOUT,                // client time-out
                          NULL);  // default security attributes
  if (hPipe == INVALID_HANDLE_VALUE) {
    printf("CreateNamedPipe failed with %d.\n", GetLastError());
    return 0;
  }

  // Call a subroutine to connect to the new client.

  io_thread = std::thread(ConnectToNewClient);
  return true;
}

void DisconnectAndClose() {
  // Disconnect the pipe instance.
  if (!DisconnectNamedPipe(hPipe)) {
    printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
  }

  // Close the handle to the pipe instance.
  CloseHandle(hPipe);
}
