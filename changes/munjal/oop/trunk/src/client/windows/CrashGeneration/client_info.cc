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

#include "client/windows/CrashGeneration/client_info.h"

#include <Windows.h>

namespace google_breakpad {

ClientInfo::ClientInfo()
    : pid(0),
      ex_info(NULL),
      assert_info(NULL),
      process(NULL),
      dump_requested_handle(NULL),
      dump_generated_handle(NULL),
      dump_request_wait_handle(NULL),
      process_exit_wait_handle(NULL) {
}

bool ClientInfo::Initialize(DWORD a_pid,
                            DWORD* a_thread_id,
                            PEXCEPTION_POINTERS* a_ex_info,
                            MDRawAssertionInfo* a_assert_info) {
  process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, a_pid);
  if (process == NULL) {
    process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, a_pid);
  }

  if (process == NULL) {
    CleanUp();
    return false;
  }

  dump_requested_handle = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (dump_requested_handle == NULL) {
    CleanUp();
    return false;
  }

  dump_generated_handle = CreateSemaphore(NULL, 0, 1, NULL);
  if (dump_generated_handle == NULL) {
    CleanUp();
    return false;
  }

  pid = a_pid;
  thread_id = a_thread_id;
  ex_info = a_ex_info;
  assert_info = a_assert_info;

  return true;
}

bool ClientInfo::CleanUp() {
  bool success = true;
  bool result;

  if (process != NULL) {
    result = (CloseHandle(process) != FALSE);
    if (result) process = NULL;
    success = success && result;
  }

  if (dump_requested_handle != NULL) {
    result = (CloseHandle(dump_requested_handle) != FALSE);
    if (result) dump_requested_handle = NULL;
    success = success && result;
  }

  if (dump_generated_handle != NULL) {
    result = (CloseHandle(dump_generated_handle) != FALSE);
    if (result) dump_generated_handle = NULL;
    success = success && result;
  }

  if (dump_request_wait_handle != NULL) {
    result = (UnregisterWait(dump_request_wait_handle) != FALSE);
    if (result) dump_request_wait_handle = NULL;
    success = success && result;
  }

  if (process_exit_wait_handle != NULL) {
    result = (UnregisterWait(process_exit_wait_handle) != FALSE);
    if (result) process_exit_wait_handle = NULL;
    success = success && result;
  }

  return success;
}

}  // namepsace google_breakpad
