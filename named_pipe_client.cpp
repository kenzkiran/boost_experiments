#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <iostream>
#include <thread>
#include <chrono>

namespace win_named_pipe {
#define BUFSIZE 512
 
int StartNamedPipeClient() 
{ 
   HANDLE hPipe; 
   LPCWSTR lpvMessage = TEXT("Default message from client."); 
   TCHAR  chBuf[BUFSIZE]; 
   BOOL   fSuccess = FALSE; 
   DWORD  cbRead, cbToWrite, cbWritten, dwMode; 
   LPCWSTR lpszPipename = L"\\\\.\\pipe\\mynamedpipe";

  std::cout << "Ravi Starting Client "<<std::endl;
  //  if( argc > 1 )
  //     lpvMessage = argv[1];
 
// Try to open a named pipe; wait for it, if necessary. 
 
   while (1) 
   { 
      hPipe = CreateFile( 
         lpszPipename,   // pipe name 
         GENERIC_READ |  // read and write access 
         GENERIC_WRITE, 
         0,              // no sharing 
         NULL,           // default security attributes
         OPEN_EXISTING,  // opens existing pipe 
         0,              // default attributes 
         NULL);          // no template file 
 
   // Break if the pipe handle is valid. 
 
      if (hPipe != INVALID_HANDLE_VALUE) 
         break; 
 
      // Exit if an error other than ERROR_PIPE_BUSY occurs. 
 
      if (GetLastError() != ERROR_PIPE_BUSY) 
      {
         std::cout <<"Error in CreateFile : GLE: "<< GetLastError() <<std::endl; 
         return -1;
      }
 
      // All pipe instances are busy, so wait for 20 seconds. 
 
      if ( ! WaitNamedPipe(lpszPipename, 20000)) 
      { 
         std::cout << "Could not open pipe: 20 second wait timed out." <<std::endl; 
         return -1;
      } 
   } 
 
// The pipe connected; change to message-read mode. 
 
   dwMode = PIPE_READMODE_MESSAGE; 
   fSuccess = SetNamedPipeHandleState( 
      hPipe,    // pipe handle 
      &dwMode,  // new pipe mode 
      NULL,     // don't set maximum bytes 
      NULL);    // don't set maximum time 
   if ( ! fSuccess) 
   {
       std::cout <<"Error in SetNamedPipeHandleState GLE: "<<GetLastError() << std::endl; 
      return -1;
   }
 
// Send a message to the pipe server. 
 for (int i = 0; i < 5; ++i) {
   cbToWrite = (lstrlen(lpvMessage)+1)*sizeof(TCHAR);
   _tprintf( TEXT("Sending %d byte message: \"%s\"\n"), cbToWrite, lpvMessage); 

   fSuccess = WriteFile( 
      hPipe,                  // pipe handle 
      lpvMessage,             // message 
      cbToWrite,              // message length 
      &cbWritten,             // bytes written 
      NULL);                  // not overlapped 

   if (!fSuccess) 
   {
     std::cout <<"Error : WriteFile to pipe failed. GLE"<< GetLastError() << std::endl; 
      return -1;
   }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
   printf("\nMessage sent to server, receiving reply as follows:\n");
 
   do 
   { 
   // Read from the pipe. 
 
      fSuccess = ReadFile( 
         hPipe,    // pipe handle 
         chBuf,    // buffer to receive reply 
         BUFSIZE*sizeof(TCHAR),  // size of buffer 
         &cbRead,  // number of bytes read 
         NULL);    // not overlapped 
 
      if (!fSuccess && GetLastError() != ERROR_MORE_DATA) {
        std::cout << "ReadFile errored out : GLE " << GetLastError();
        break;
      }
 

      _tprintf( TEXT("\"%s\"\n"), chBuf ); 
   } while (1);  // repeat loop if ERROR_MORE_DATA 

   if ( ! fSuccess)
   {
      _tprintf( TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError() );
      return -1;
   }

   printf("\n<End of message, press ENTER to terminate connection and exit>");
   _getch();
 
   CloseHandle(hPipe); 
 
   return 0; 
}

}