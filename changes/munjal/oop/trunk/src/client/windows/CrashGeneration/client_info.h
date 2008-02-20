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

#ifndef CLIENT_WINDOWS_CRASHGENERATION_CLIENT_INFO_H__
#define CLIENT_WINDOWS_CRASHGENERATION_CLIENT_INFO_H__

#include <Windows.h>

#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

// Abstraction for a crash client process
class ClientInfo {
 public:
  ClientInfo();

  ~ClientInfo();

  // Performs initialization with the given values. Note that
  // this method will get the process handle for the given
  // process id and it will also create necessary event objects.
  // But it will not be able to register waits for the crash event
  // and the client process exit event and hence will not be able to
  // initialize the wait handles.
  //
  // @returns
  // True if all the steps are successful. If one of the steps
  // fails during initialization, then the cleanup work is
  // performed to properly clean partially initialized state
  // and false is returned.
  bool Initialize(DWORD a_pid,
                  DWORD* a_thread_id,
                  PEXCEPTION_POINTERS* a_ex_info,
                  MDRawAssertionInfo* a_assert_info);

  // Cleans up properly by closing and unregistering waits on
  // all the handles appropriately.
  //
  // @returns
  // True if the cleanup is successful; false otherwise. NOTE that
  // even if one step in cleanup fails, other steps are attempted
  // and return value indicates the overall success. Also, if
  // cleanup fails for one resource that handle retains its value
  // (is not assigned NULL); for successful steps, the corresponding
  // handle is assigned a NULL value.
  bool CleanUp();

  // Client process ID.
  DWORD pid;

  // Address, in the client process address space, of a
  // EXCEPTION_POINTERS* variable that will point to an
  // instance of EXCEPTION_POINTERS object containing
  // information about crash.
  PEXCEPTION_POINTERS* ex_info;

  // Address, in the client process address space, of an instance
  // of MDRawAssertionInfo that will contain information about
  // non-exception related crashes like invalid parameter assertion
  // failures and pure calls.
  MDRawAssertionInfo* assert_info;

  // Address, in the client process address space, of the variable
  // that will contain the thread id of the crashing client thread.
  DWORD* thread_id;

  // Client process handle.
  HANDLE process;

  // Dump request event handle.
  HANDLE dump_requested_handle;

  // Dump generated event handle.
  HANDLE dump_generated_handle;

  // Wait handle for dump request event.
  HANDLE dump_request_wait_handle;

  // Wait handle for process exit event.
  HANDLE process_exit_wait_handle;
};

}  // namespace google_breakpad

#endif  // CLIENT_WINDOWS_CRASHGENERATION_CLIENT_INFO_H__
