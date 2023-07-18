#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <iostream>

#define BUFSIZE 512
 
int _tmain(int argc, TCHAR *argv[]) 
{ 
   HANDLE hPipe; 
   LPTSTR lpvMessage=TEXT("Default message from client."); 
   TCHAR  chBuf[BUFSIZE]; 
   BOOL   fSuccess = FALSE; 
   DWORD  cbRead, cbToWrite, cbWritten, dwMode; 
   LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe"); 
   TCHAR sendBuf[BUFSIZE] = { 0 };
   _stprintf_s(sendBuf, BUFSIZE, TEXT("%s from process %d"), lpvMessage, GetCurrentProcessId());

   if( argc > 1 )
      lpvMessage = argv[1];
 
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
         _tprintf( TEXT("Could not open pipe. GLE=%d\n"), GetLastError() ); 
         return -1;
      }
 
      // All pipe instances are busy, so wait for 20 seconds. 
 
      if ( ! WaitNamedPipe(lpszPipename, 20000)) 
      { 
         printf("Could not open pipe: 20 second wait timed out."); 
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
      _tprintf( TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError() ); 
      return -1;
   }
 ////test//////////////////////////////////////////////////////////////
   std::cout << "please input message to server.(input \"exit\" to exit)." << std::endl;
   do {
       // Send a message to the pipe server. 
       ZeroMemory(sendBuf, BUFSIZE);
       char  ch;
       int i = 0;
       while (std::cin >> std::noskipws >> ch) {
           if (ch == '\n') {
               break;
           }
           sendBuf[i++] = ch;
       }
       cbToWrite = (lstrlen(sendBuf) + 1) * sizeof(TCHAR);
       _tprintf(TEXT("process %d Sending %d byte message: \"%s\"\n"), GetCurrentProcessId(), cbToWrite, sendBuf);

       int cmpResult =  strcmp(sendBuf, "exit");
       if (cmpResult == 0) {

           FlushFileBuffers(hPipe);
           CloseHandle(hPipe);
           break;
       }

       fSuccess = WriteFile(
           hPipe,                  // pipe handle 
           sendBuf,             // message 
           cbToWrite,              // message length 
           &cbWritten,             // bytes written 
           NULL);                  // not overlapped 

       if (!fSuccess)
       {
           _tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
           return -1;
       }

       printf("\nMessage sent to server, receiving reply as follows:\n");

       do
       {
           // Read from the pipe. 

           fSuccess = ReadFile(
               hPipe,    // pipe handle 
               chBuf,    // buffer to receive reply 
               BUFSIZE * sizeof(TCHAR),  // size of buffer 
               &cbRead,  // number of bytes read 
               NULL);    // not overlapped 

           if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
               break;

           _tprintf(TEXT("\"%s\"\n"), chBuf);
       } while (!fSuccess);  // repeat loop if ERROR_MORE_DATA 

       if (!fSuccess)
       {
           _tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
           return -1;
       }

   } while (true);

   printf("\n<End of message, press ENTER to terminate connection and exit>");
   _getch();
 
   CloseHandle(hPipe); 
 
   return 0; 
}