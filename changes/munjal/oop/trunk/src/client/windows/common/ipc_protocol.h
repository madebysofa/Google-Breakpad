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

#ifndef CLIENT_WINDOWS_COMMON_IPC_PROTOCOL_H__
#define CLIENT_WINDOWS_COMMON_IPC_PROTOCOL_H__

#include <Windows.h>

#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

// Constants for the protocol between client and the server.

// Tags sent with each message indicating the purpose of
// the message.
enum MessageTag {
  MESSAGE_TAG_NONE = 0,
  MESSAGE_TAG_REGISTRATION_REQUEST = 1,
  MESSAGE_TAG_REGISTRATION_RESPONSE = 2,
  MESSAGE_TAG_REGISTRATION_ACK = 3
};

// Message structure for IPC between crash client and crash server.
struct IPCMsg {
  // Tag in the message.
  MessageTag tag;

  // Process id.
  DWORD  pid;

  // Client thread id pointer.
  DWORD* thread_id;

  // Exception information.
  PEXCEPTION_POINTERS* exception_pointers;

  // Assert information in case of an invalid parameter or
  // pure call failure.
  MDRawAssertionInfo* assert_info;

  // Handle to signal the crash event.
  HANDLE dump_request_handle;

  // Handle to check if server is done generating crash.
  HANDLE dump_generated_handle;

  // Handle to a mutex that becomes signaled (WAIT_ABANDONED)
  // if server process goes down.
  HANDLE server_alive_handle;

  // Creates an instance.
  IPCMsg() {
    IPCMsg(MESSAGE_TAG_NONE, 0, 0, NULL, NULL, NULL, NULL, NULL);
  }

  // Creates an insance with the given parameters.
  IPCMsg(MessageTag a_tag,
         DWORD a_pid,
         DWORD* a_thread_id,
         PEXCEPTION_POINTERS* a_exception_pointers,
         MDRawAssertionInfo* a_assert_info,
         HANDLE a_dump_request_handle,
         HANDLE a_dump_generated_handle,
         HANDLE a_server_alive)
    : pid(a_pid),
      tag(a_tag),
      thread_id(a_thread_id),
      exception_pointers(a_exception_pointers),
      assert_info(a_assert_info),
      dump_request_handle(a_dump_request_handle),
      dump_generated_handle(a_dump_generated_handle),
      server_alive_handle(a_server_alive) {
  }

};

}  // namespace google_breakpad

#endif  // CLIENT_WINDOWS_COMMON_IPC_PROTOCOL_H__
