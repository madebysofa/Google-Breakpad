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

#include "client/windows/CrashGeneration/crash_generation_client.h"

#include <cassert>

#include "client/windows/common/ipc_protocol.h"

namespace google_breakpad {

const int kTimeout = 2000;

#ifdef _DEBUG
const int kWaitForServerTimeout = INFINITE;
#else
const int kWaitForServerTimeout = 60000;
#endif

const int kPipeConnectTotalAttempts = 2;

const DWORD kPipeAccess = FILE_READ_DATA |
                          FILE_WRITE_DATA |
                          FILE_WRITE_ATTRIBUTES;

const DWORD kPipeFlagsAttrs = SECURITY_IDENTIFICATION |
                              SECURITY_SQOS_PRESENT;

const DWORD kPipeMode = PIPE_READMODE_MESSAGE;

const DWORD kWaitEventCount = 2;

CrashGenerationClient::CrashGenerationClient(const wchar_t* pipe_name)
    : pipe_name_(pipe_name),
      thread_id_(0),
      server_process_id_(0),
      crash_event_(NULL),
      crash_generated_(NULL),
      server_alive_(NULL),
      exception_pointers_(NULL) {
}

CrashGenerationClient::~CrashGenerationClient() {
  if (crash_event_ != NULL) {
    CloseHandle(crash_event_);
  }

  if (crash_generated_ != NULL) {
    CloseHandle(crash_generated_);
  }

  if (server_alive_ != NULL) {
    CloseHandle(server_alive_);
  }
}

// Performs the registration step with the server process.
// The registration step involves communicating with the server
// via a known named pipe. The client sends the following pieces
// of data to the server:
//
// * Message tag indicating the client is requesting registration.
// * Process id of the client process.
// * Address of a DWORD variable in the client address space
//   that will contain the thread id of the client thread that
//   caused the crash.
// * Address of a EXCEPTION_POINTERS* variable in the client
//   address space that will point to an instance of EXCEPTION_POINTERS
//   when the crash happens.
// * Address of an instance of MDRawAssertionInfo that will contain
//   relevant information in case of non-exception crashes like assertion
//   failures and pure calls.
//
// In return the client expects the following information from the server:
//
// * Message tag indicating successful registration.
// * Server process id.
// * Handle to an object that client can signal to request dump
//   generation from the server.
// * Handle to an object that client can wait on after requesting
//   dump generation for the server to finish dump generation.
// * handle to a mutex object that client can wait on to make sure
//    server is still alive.
//
// If any step of the expected behavior mentioned above fails, the
// registration step is not considered successful and hence OoP dump
// generation service is not available.
//
// @returns
// True if the registration is successful; false otherwise.
bool CrashGenerationClient::Register() {
  DWORD pid = GetCurrentProcessId();

  IPCMsg msg(MESSAGE_TAG_REGISTRATION_REQUEST,
             pid,
             &thread_id_,
             &exception_pointers_,
             &assert_info_,
             NULL,
             NULL,
             NULL);

  IPCMsg reply;

  if (!ConnectToServer(msg, reply))  return false;

  crash_event_ = reply.dump_request_handle;
  crash_generated_ = reply.dump_generated_handle;
  server_alive_ = reply.server_alive_handle;
  server_process_id_ = reply.pid;

  return true;
}

bool CrashGenerationClient::TransactNamedPipeHelper(HANDLE pipe,
                                                    const void* in_buffer,
                                                    DWORD in_size,
                                                    void* out_buffer,
                                                    DWORD out_size,
                                                    DWORD* bytes_count) {
#if _DEBUG
  // In DEBUG mode, try to do a separate write and then
  // a separate read to help reproduce some scenarios
  // where the client may be slow.
  return TransactNamedPipeDebugHelper(pipe,
                                      in_buffer,
                                      in_size,
                                      out_buffer,
                                      out_size,
                                      bytes_count);

#else
  return (TransactNamedPipe(
    pipe,
    in_buffer,
    in_size,
    out_buffer,
    out_size,
    bytes_count, NULL) != FALSE);
#endif
}

#if _DEBUG

bool CrashGenerationClient::TransactNamedPipeDebugHelper(HANDLE pipe,
                                                         const void* in_buffer,
                                                         DWORD in_size,
                                                         void* out_buffer,
                                                         DWORD out_size,
                                                         DWORD* bytes_count) {
  BOOL success;
  DWORD error_code;

  // Uncomment the next sleep to create a gap before writing
  // to pipe.
  // Sleep(5000);

  success = WriteFile(pipe,
                      in_buffer,
                      in_size,
                      bytes_count,
                      NULL);
  error_code = GetLastError();

  if (!success) return false;

  // Uncomment the next sleep to create a gap between write
  // and read.
  // Sleep(5000);

  success = ReadFile(pipe,
                     out_buffer,
                     out_size,
                     bytes_count,
                     NULL);
  error_code = GetLastError();

  return (success != FALSE);
}

#endif

bool CrashGenerationClient::ConnectToServer(const IPCMsg& msg,
                                            IPCMsg& reply) {
  DWORD bytes_count;

  HANDLE pipe = ConnectToPipe(pipe_name_, kPipeAccess, kPipeFlagsAttrs);

  if (pipe == NULL) return false;

  DWORD mode = kPipeMode;
  if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
    CloseHandle(pipe);
    return false;
  }

  if (!TransactNamedPipeHelper(pipe,
                               &msg,
                               sizeof(msg),
                               &reply,
                               sizeof(reply),
                               &bytes_count)) {
    CloseHandle(pipe);
    return false;
  }

  // Check for an appropriate reply message.
  if (!ValidateResponse(reply)) {
    CloseHandle(pipe);
    return false;
  }

  IPCMsg ack_msg;
  ack_msg.tag = MESSAGE_TAG_REGISTRATION_ACK;

  // Send an ack to the server that the response is successfully read.
  BOOL success = WriteFile(pipe,
                           &ack_msg,
                           sizeof(ack_msg),
                           &bytes_count,
                           NULL);

  CloseHandle(pipe);
  return (success != FALSE);
}

HANDLE CrashGenerationClient::ConnectToPipe(const wchar_t* pipe_name,
                                            DWORD pipe_access,
                                            DWORD flags_attrs) {
  bool success = false;
  HANDLE pipe = NULL;

  for (int i = 0; i < kPipeConnectTotalAttempts; ++i) {
    pipe = CreateFileW(pipe_name,
                       pipe_access,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       flags_attrs,
                       NULL);

    success = (pipe != INVALID_HANDLE_VALUE);
    // No need to retry if connection succeeded, or if it failed
    // due to any other reason than the pipe being busy.
    if (success || (GetLastError() != ERROR_PIPE_BUSY)) {
      break;
    }

    if (!WaitNamedPipeW(pipe_name, kTimeout)) {
      break;
    }
  }

  return (success ? pipe : NULL);
}

bool CrashGenerationClient::ValidateResponse(const IPCMsg& msg) const {
  return (msg.tag == MESSAGE_TAG_REGISTRATION_RESPONSE) &&
         (msg.pid != 0) &&
         (msg.dump_request_handle != NULL) &&
         (msg.dump_generated_handle != NULL) &&
         (msg.server_alive_handle != NULL);
}

bool CrashGenerationClient::RegistrationSucceeded() const {
  return (crash_event_ != NULL);
}

bool CrashGenerationClient::RequestDump(EXCEPTION_POINTERS* ex_info) {
  // Return false if registration is not performed or has not succeeded.
  if (!RegistrationSucceeded()) return false;

  exception_pointers_ = ex_info;
  thread_id_ = GetCurrentThreadId();

  assert_info_.line = -1;
  assert_info_.type = 0;
  assert_info_.expression[0] = 0;
  assert_info_.file[0] = 0;
  assert_info_.function[0] = 0;

  return SignalCrashEventAndWait();
}

bool CrashGenerationClient::RequestDump(MDRawAssertionInfo* assert_info) {
  // Return false if registration is not performed or has not succeeded.
  if (!RegistrationSucceeded()) return false;

  exception_pointers_ = NULL;

  if (assert_info != NULL) {
    memcpy(&assert_info_, assert_info, sizeof(assert_info_));
  } else {
    memset(&assert_info_, 0, sizeof(assert_info_));
  }

  thread_id_ = GetCurrentThreadId();

  return SignalCrashEventAndWait();
}

bool CrashGenerationClient::SignalCrashEventAndWait() {
  assert(crash_event_ != NULL);
  assert(crash_generated_ != NULL);
  assert(server_alive_ != NULL);

  SetEvent(crash_event_);

  HANDLE wait_handles[kWaitEventCount];
  wait_handles[0] = crash_generated_;
  wait_handles[1] = server_alive_;

  DWORD result = WaitForMultipleObjects(kWaitEventCount,
                                        wait_handles,
                                        FALSE,
                                        kWaitForServerTimeout);

  // Crash dump was successfully generated only if the server
  // signaled the crash generated event.
  return result == WAIT_OBJECT_0;
}

}  // namespace google_breakpad
