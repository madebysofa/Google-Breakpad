// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CLIENT_WINDOWS_CRASHGENERATION_CRASH_GENERATION_CLIENT_H__
#define CLIENT_WINDOWS_CRASHGENERATION_CRASH_GENERATION_CLIENT_H__

#include "client/windows/common/ipc_protocol.h"

namespace google_breakpad {

// Abstraction of client-side implementation of Out-of-Process
// crash generation.
//
// The process that desires to have out-of-process crash dump
// generation service can use this class in the following way:
//
// * Create an instance
// * Call Register method so that the client tries to register
//   with the server process and check the return value. If
//   registration is not successful, out-of-process crash dump
//   generation will not be available
// * Request dump generation by calling either of the two
//   overloaded RequestDump methods - one in case of exceptions
//   and the other in case of assertion failures
//
// NOTE that it is the responsibility of the client code of
// this class to register for unhandled exceptions with the
// system and the client code should explicitly request dump
// generation.
class CrashGenerationClient {
 public:
  CrashGenerationClient(const wchar_t* pipe_name);

  ~CrashGenerationClient();

  // Registers the client process with the crash server.
  //
  // @returns True if the registration is successful; false otherwise.
  bool Register();

  // Requests the crash server to generate a dump with the given
  // exception information.
  //
  // @returns
  // True if the dump was successful; false otherwise. NOTE that
  // if registration step was not performed or was not successful,
  // false will be returned.
  bool RequestDump(EXCEPTION_POINTERS* ex_info);

  // Requests the crash server to generate a dump with the given
  // assertion information.
  //
  // @returns
  // True if the dump was successful; false otherwise. NOTE that
  // if registration step was not performed or was not successful,
  // false will be returned.
  bool RequestDump(MDRawAssertionInfo* assert_info);

 private:
  // Helper to write request to the pipe and then read a reply
  // from the pipe.
  bool TransactNamedPipeHelper(HANDLE pipe,
                               const void* in_buffer,
                               DWORD in_size,
                               void* out_buffer,
                               DWORD out_size,
                               DWORD* bytes_count);

#if _DEBUG
  // Implementation of TransactNamedPipe helper for debug mode. This
  // implementation splits TransactNamedPipe into a separate Write and
  // a Read. That allows in putting Sleeps at various points in code to
  // repro some scenarios when debugging.
  bool TransactNamedPipeDebugHelper(HANDLE pipe,
                                    const void* in_buffer,
                                    DWORD in_size,
                                    void* out_buffer,
                                    DWORD out_size,
                                    DWORD* bytes_count);
#endif

  // Connects to the server with the given message and stores the reply
  // in the given object. NOTE that reply object is not changed if false
  // is returned.
  //
  // @returns True if connection to server went okay; false otherwise.
  bool ConnectToServer(const IPCMsg& msg, IPCMsg& reply);

  // Validates the given server response.
  bool ValidateResponse(const IPCMsg& msg) const;

  // Returns whether registration step succeeded or not.
  bool RegistrationSucceeded() const;

  // Connect to the given named pipe with given parameters.
  //
  // @returns True if the connection is successful; false otherwise.
  HANDLE CrashGenerationClient::ConnectToPipe(const wchar_t* pipe_name,
                                              DWORD pipe_access,
                                              DWORD flags_attrs);

  // Helper to signal the crash event and wait for the server to
  // generate crash.
  bool SignalCrashEventAndWait();

  // Pipe name to use to talk to server
  const wchar_t* pipe_name_;

  // Event to signal in case of a crash.
  HANDLE crash_event_;

  // Handle to wait on after signaling a crash for the server
  // to finish generating crash dump.
  HANDLE crash_generated_;

  // Handle to a mutex that will become signaled with WAIT_ABANDONED
  // if hte server process goes down.
  HANDLE server_alive_;

  // Server process id.
  DWORD server_process_id_;

  // Id of the thread that caused the crash.
  DWORD thread_id_;

  // Exception pointers for an exception crash.
  EXCEPTION_POINTERS* exception_pointers_;

  // Assertion info for an invalid parameter or pure call crash.
  MDRawAssertionInfo assert_info_;
};

}  // namespace google_breakpad

#endif
