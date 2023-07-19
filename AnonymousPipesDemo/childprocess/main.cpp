#include <windows.h>
#include <stdio.h>
#include <string>

#define BUFSIZE 4096 
 
int main(int argc, char** argv) 
{ 
   CHAR chBuf[BUFSIZE]; 
   DWORD dwRead, dwWritten; 
   HANDLE hStdin, hStdout; 
   BOOL bSuccess; 
   
 
   //hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
   //hStdin = GetStdHandle(STD_INPUT_HANDLE); 
   //if ( 
   //    (hStdout == INVALID_HANDLE_VALUE) || 
   //    (hStdin == INVALID_HANDLE_VALUE) 
   //   ) 
   //   ExitProcess(1); 

   LPSTR pStr = GetCommandLine();
 
   if (argc < 3) {
       return 1;
   }
   std::string strWriteHandle;
   strWriteHandle.assign(argv[1], strlen(argv[1])); // writeHandle=284 

   std::string strReadHandle = argv[2];             // readHandle=288

   size_t offset = strWriteHandle.find_last_of("=");
   strWriteHandle = strWriteHandle.substr(offset + 1); // 284

   offset = strReadHandle.find_last_of("=");
   strReadHandle = strReadHandle.substr(offset + 1); // 284

   hStdout = (HANDLE)atoi(strWriteHandle.c_str());
   hStdin = (HANDLE)atoi(strReadHandle.c_str());
   if (
       (hStdout == INVALID_HANDLE_VALUE) ||
       (hStdin == INVALID_HANDLE_VALUE)
       )
       ExitProcess(1);


   // Send something to this process's stdout using printf.
//   printf("\n ** This is a message from the child process.  %s %s %s  ** \n", pStr, strWriteHandle.c_str(), strReadHandle.c_str());
   
// WriteFile(hStdout, pStr, sizeof(pStr), &dwWritten, NULL);

   WriteFile(hStdout, "123\n", 4, &dwWritten, NULL);

   // This simple algorithm uses the existence of the pipes to control execution.
   // It relies on the pipe buffers to ensure that no data is lost.
   // Larger applications would use more advanced process control.

   for (;;) 
   { 
   // Read from standard input and stop on error or no data.
      bSuccess = ReadFile(hStdin, chBuf, BUFSIZE, &dwRead, NULL); 
      
      if (! bSuccess || dwRead == 0) 
         break; 
 
      //LPSTR pStr = GetCommandLine();
      //bSuccess = WriteFile(hStdout, pStr, strlen(pStr), &dwWritten, NULL);
   // Write to standard output and stop on error.
      bSuccess = WriteFile(hStdout, chBuf, dwRead, &dwWritten, NULL); 
      
      if (! bSuccess) 
         break; 
   } 
   return 0;
}