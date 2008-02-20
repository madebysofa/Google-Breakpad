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

#include "client/windows/CrashGeneration/crash_generation_server.h"

#include <Windows.h>

#include <cassert>
#include <list>

#include "client/windows/common/ipc_protocol.h"
#include "client/windows/CrashGeneration/auto_lock.h"

using std::list;

namespace google_breakpad {

// Output buffer size.
const size_t kOutBuffSize = 64;
// Input buffer size.
const size_t kInBuffSize = 64;
// Access flags for the client on the event.
const DWORD kEventAccess = EVENT_MODIFY_STATE;
// Access flags for the client on the semaphore.
const DWORD kSemaphoreAccess = SYNCHRONIZE;
// Access flags for the client on the mutex.
const DWORD kMutexAccess = SYNCHRONIZE;

// Attribute flags for the pipe.
const DWORD kPipeAttr = FILE_FLAG_FIRST_PIPE_INSTANCE |
                        PIPE_ACCESS_DUPLEX |
                        FILE_FLAG_OVERLAPPED;

// Mode for the pipe.
const DWORD kPipeMode = PIPE_TYPE_MESSAGE |
                        PIPE_READMODE_MESSAGE |
                        PIPE_WAIT;

CrashGenerationServer::CrashGenerationServer(const wchar_t* pipe_name) {
  CrashGenerationServer(pipe_name, NULL, NULL, NULL, NULL, false, NULL);
}

CrashGenerationServer::CrashGenerationServer(
  const wchar_t* pipe_name,
  OnClientConnectedCallback connect_callback,
  void* connect_context,
  OnClientCrashedCallback crash_callback,
  void* crash_context,
  bool generate_dumps,
  const wstring* dump_path)
    : pipe_name_(pipe_name),
      pipe_(NULL),
      pipe_wait_handle_(NULL),
      server_alive_handle_(NULL),
      connect_callback_(connect_callback),
      connect_context_(connect_context),
      crash_callback_(crash_callback),
      crash_context_(crash_context),
      generate_dumps_(generate_dumps),
      dump_generator_(NULL),
      server_state_(IPC_SERVER_STATE_INITIAL) {
  assert(pipe_name_ != NULL);
  memset(&overlapped_, 0, sizeof(overlapped_));
  InitializeCriticalSection(&clients_sync_);

  if (dump_path) {
    dump_generator_ = new MinidumpGenerator(*dump_path);
  }
}

CrashGenerationServer::~CrashGenerationServer() {
  // Unregister from thread pool, close handles and delete
  // other resources.
  if (dump_generator_ != NULL) {
    delete dump_generator_;
  }

  // New scope to release critical section automatically.
  {
    AutoLock lock(&clients_sync_);

    list<ClientInfoEx*>::iterator iter;

    // Clean up all ClientInfo objects.
    for (iter = clients_.begin(); iter != clients_.end(); ++iter) {
      ClientInfoEx* client_info_ex = *iter;
      client_info_ex->client.CleanUp();
      delete client_info_ex;
    }

    // Clear the list of clients.
    clients_.clear();
  }

  UnregisterWait(pipe_wait_handle_);
  CloseHandle(pipe_wait_handle_);
  DeleteCriticalSection(&clients_sync_);
}

bool CrashGenerationServer::Initialize() {
  server_state_ = IPC_SERVER_STATE_INITIAL;

  // Mutex to indicate to clients if server is still alive or not.
  server_alive_handle_ = CreateMutex(NULL, TRUE, NULL);
  if (server_alive_handle_ == NULL) return false;

  // Event to signal the client connection and other pipe activity.
  overlapped_.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  if (overlapped_.hEvent == NULL) return false;

  // Register a callback with the thread pool for client connection.
  RegisterWaitForSingleObject(&pipe_wait_handle_,
                              overlapped_.hEvent,
                              OnPipeConnected,
                              this,
                              INFINITE,
                              WT_EXECUTEINWAITTHREAD);

  pipe_ = CreateNamedPipeW(pipe_name_,
                           kPipeAttr,
                           kPipeMode,
                           1,
                           kOutBuffSize,
                           kInBuffSize,
                           0,
                           NULL);

  if (pipe_ == NULL) return false;

  // Signal the event to start a separate thread to handle
  // client connections.
  return (SetEvent(overlapped_.hEvent) != FALSE);
}

// If the server thread serving clients ever gets into a an
// ERROR state, reset the event, close the pipe and remain
// in error state forever. Error state means something that
// we didn't accoutn for happened, and it's dangerous to do
// anything unknowingly.
void CrashGenerationServer::HandleErrorState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_ERROR);

  if (pipe_wait_handle_ != NULL) {
    UnregisterWait(pipe_wait_handle_);
    pipe_wait_handle_ = NULL;
  }

  if (pipe_ != NULL) {
    CloseHandle(pipe_);
    pipe_ = NULL;
  }

  if (overlapped_.hEvent != NULL) {
    ResetEvent(overlapped_.hEvent);
    CloseHandle(overlapped_.hEvent);
    overlapped_.hEvent = NULL;
  }
}

// When server thread serviing clients is in INITIAL state,
// try to connect to pipe asynchronously. If the connection
// finishes synchronously, directly get into CONNECTED state;
// otherwise get into CONNECTING state. For any issues, get
// into DISCONNECTING state.
void CrashGenerationServer::HandleInitialState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_INITIAL);

  ResetEvent(overlapped_.hEvent);

  BOOL success = ConnectNamedPipe(pipe_, &overlapped_);

  // From MSDN, it is not clear when TRUE is returned.
  assert(!success);

  DWORD error_code = GetLastError();

  switch (error_code) {
  case ERROR_IO_PENDING:
    server_state_ = IPC_SERVER_STATE_CONNECTING;
    break;

  case ERROR_PIPE_CONNECTED:
    server_state_ = IPC_SERVER_STATE_CONNECTED;
    break;

  default:
    server_state_ = IPC_SERVER_STATE_ERROR;
  }
}

// When server thread serving the clients is in CONNECTING state,
// try to get the result of asynchronous connection request using
// the OVERLAPPED object. If result indicates connection is done,
// get into CONNECTED state. If the result indicates IO is still
// INCOMPLETE, remain in CONNECTING state. For any other issues,
// get into DISCONNECTING state.
void CrashGenerationServer::HandleConnectingState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_CONNECTING);

  DWORD bytes_count;
  BOOL success = GetOverlappedResult(pipe_,
                                     &overlapped_,
                                     &bytes_count,
                                     FALSE);
  if (success) {
    server_state_ = IPC_SERVER_STATE_CONNECTED;
    return;
  }

  DWORD error_code = GetLastError();
  if (error_code == ERROR_IO_INCOMPLETE) return;

  server_state_ = IPC_SERVER_STATE_DISCONNECTING;
}

// When server thread serving the clients is in CONNECTED state, try to issue
// an asynchronous read from the pipe. If read completss sync'ly or if IO is
// pending then get into READING state. For any issues, get into the
// DISCONNECTING state.
void CrashGenerationServer::HandleConnectedState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_CONNECTED);

  server_state_ = IPC_SERVER_STATE_READING;

  DWORD bytes_count;
  BOOL success = ReadFile(pipe_,
                          &msg_,
                          sizeof(msg_),
                          &bytes_count,
                          &overlapped_);
  DWORD error_code = GetLastError();

  if (success || (error_code == ERROR_IO_PENDING)) return;
  server_state_ = IPC_SERVER_STATE_DISCONNECTING;
}

// When server thread serving the clients is in READING state, try to get
// result of the async read. If async read is done, get into READ_DONE
// state. For any issues, get into the DISCONNECTING state.
void CrashGenerationServer::HandleReadingState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_READING);

  DWORD bytes_count;
  BOOL success = GetOverlappedResult(pipe_,
                                     &overlapped_,
                                     &bytes_count,
                                     FALSE);

  if (success) {
    server_state_ = IPC_SERVER_STATE_READ_DONE;
  } else {
    DWORD error_code = GetLastError();

    // We should never get an I/O incomplete since we should not execute this
    // unless the Read has finished and the overlapped event is signaled. If
    // we do get INCOMPLETE, we have a bug in our code.
    assert(error_code != ERROR_IO_INCOMPLETE);

    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
  }
}

// When server thread serving the client is in READ_DONE state, validate
// the client's request message, register the client by creating appropriate
// objects and prepare the response. If any of that fails, get into the
// DISCONNECTING state. Then try to write the response to the pipe async'ly.
// If that succeeds, get into WRITING state. For any issues, get into the
// DISCONNECTING state.
void CrashGenerationServer::HandleReadDoneState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_READ_DONE);

  if (!ValidateClientRequest(msg_)) {
    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
    return;
  }

  if (!client_info_.Initialize(msg_.pid,
                               msg_.thread_id,
                               msg_.exception_pointers,
                               msg_.assert_info)) {
    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
    return;
  }

  DWORD server_process_id = GetCurrentProcessId();

  IPCMsg reply;
  reply.tag = MESSAGE_TAG_REGISTRATION_RESPONSE;
  reply.pid = server_process_id;

  if (!PrepareReply(&reply, client_info_)) {
    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
    return;
  }

  if (!AddClient(client_info_)) {
    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
    return;
  }

  server_state_ = IPC_SERVER_STATE_WRITING;

  DWORD bytes_count;
  BOOL result = WriteFile(pipe_,
                          &reply,
                          sizeof(reply),
                          &bytes_count,
                          &overlapped_);
  DWORD error_code = GetLastError();

  if (!result && (error_code == ERROR_IO_PENDING)) {
    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
  }
}

// When server thread serving the clients is in WRITING state, try to get the
// result of the async write. If async write is done, get into the WRITE_DONE
// state. For any issues, get into the DISONNECTING state.
void CrashGenerationServer::HandleWritingState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_WRITING);

  DWORD bytes_count;
  BOOL success = GetOverlappedResult(pipe_,
                                     &overlapped_,
                                     &bytes_count,
                                     FALSE);

  if (success) {
    server_state_ = IPC_SERVER_STATE_WRITE_DONE;
  } else {
    DWORD error_code = GetLastError();
    // We should never get an I/O incomplete since we should not execute this
    // unless the Write has finished and the overlapped event is signaled. If
    // we do get INCOMPLETE, we have a bug in our code.
    assert(error_code != ERROR_IO_INCOMPLETE);
    server_state_ = IPC_SERVER_STATE_DISCONNECTING;
  }
}

// When server thread serving the clients is in WRITE_DONE state, try to issue
// an async read on the pipe. If the read completes sync'ly or if I/O is still
// pending then get into READING_ACK state. For any issues, get into the
// DISOCNNECTING state.
void CrashGenerationServer::HandleWriteDoneState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_WRITE_DONE);

  server_state_ = IPC_SERVER_STATE_READING_ACK;

  DWORD bytes_count;
  BOOL success = ReadFile(pipe_,
                          &msg_,
                          sizeof(msg_),
                          &bytes_count,
                          &overlapped_);
  DWORD error_code = GetLastError();

  if (success || (error_code == ERROR_IO_PENDING)) return;
  server_state_ = IPC_SERVER_STATE_DISCONNECTING;
}

// When server thread serving the clients is in READING_ACK state, try to get
// result of async read. If I/O is done, or in case of any issues, get into
// the DISCONNECTING state. Otherwise, if I/O is incomplete, remain in the
// READING_ACK state.
void CrashGenerationServer::HandleReadingAckState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_READING_ACK);

  DWORD bytes_count;
  BOOL success = GetOverlappedResult(pipe_,
                                     &overlapped_,
                                     &bytes_count,
                                     FALSE);
  DWORD error_code = GetLastError();

  if (!success && error_code == ERROR_IO_INCOMPLETE)  return;

  // In case of a successful connection completion, perform the callback
  if (success) {
    if (connect_callback_ != NULL) {
      connect_callback_(connect_context_, client_info_);
    }
  }

  server_state_ = IPC_SERVER_STATE_DISCONNECTING;
}

// When server thread serving the client is in DISCONNECTING state,
// disconnect from the pipe and reset the event. If anything fails,
// get into the ERROR state. If it goes well, get into INITIAL
// state and set the event to start all over again.
void CrashGenerationServer::HandleDisconnectingState() {
  // Make sure the helper is not called in a wrong state.
  assert(server_state_ == IPC_SERVER_STATE_DISCONNECTING);

  if (!DisconnectNamedPipe(pipe_)) {
    server_state_ = IPC_SERVER_STATE_ERROR;
    return;
  }

  if (!ResetEvent(overlapped_.hEvent)) {
    server_state_ = IPC_SERVER_STATE_ERROR;
    return;
  }

  server_state_ = IPC_SERVER_STATE_INITIAL;
  if (!SetEvent(overlapped_.hEvent)) {
    server_state_ = IPC_SERVER_STATE_ERROR;
  }
}

bool CrashGenerationServer::ValidateClientRequest(const IPCMsg& msg) {
  return (msg_.tag == MESSAGE_TAG_REGISTRATION_REQUEST &&
          msg_.pid != 0 &&
          msg.thread_id != NULL &&
          msg.exception_pointers != NULL &&
          msg.assert_info != NULL);
}

bool CrashGenerationServer::PrepareReply(IPCMsg* reply,
                                         const ClientInfo& client_info) {
  HANDLE client_dump_requested = NULL;
  if (!DuplicateHandle(GetCurrentProcess(),
                       client_info.dump_requested_handle,
                       client_info.process,
                       &client_dump_requested,
                       kEventAccess,
                       FALSE,
                       0)) {
    return false;
  }

  HANDLE client_dump_generated = NULL;
  if (!DuplicateHandle(GetCurrentProcess(),
                       client_info.dump_generated_handle,
                       client_info.process,
                       &client_dump_generated,
                       kSemaphoreAccess,
                       FALSE,
                        0)) {
    return false;
  }

  HANDLE server_alive = NULL;
  if (!DuplicateHandle(GetCurrentProcess(),
                       server_alive_handle_,
                       client_info.process, 
                       &server_alive,
                       kMutexAccess,
                       FALSE,
                       0)) {
    return false;
  }

  reply->dump_request_handle = client_dump_requested;
  reply->dump_generated_handle = client_dump_generated;
  reply->server_alive_handle = server_alive;
  return true;
}

// Server thread servicing the clients runs this method
// continously. The method implements hte state machine
// described in ReadMe.txt along with the helper methods
// HandleXXXState. Please go through ReadMe.txt to understand
// varoius states the server thread can be in and the actions
// in each state.
void CrashGenerationServer::ServiceConnection() {
  // Implement the state machine logic for async pipe I/O server.
  // NOTE that most of the code is in helper functions HandleXXXState
  // and the switch/case below just calls them at the right time.
  switch (server_state_) {

  case IPC_SERVER_STATE_ERROR:
    HandleErrorState();
    break;

  case IPC_SERVER_STATE_INITIAL:

    HandleInitialState();
    break;

  case IPC_SERVER_STATE_CONNECTING:
    HandleConnectingState();
    break;

  case IPC_SERVER_STATE_CONNECTED:
    HandleConnectedState();
    break;

  case IPC_SERVER_STATE_READING:
    HandleReadingState();
    break;

  case IPC_SERVER_STATE_READ_DONE:
    HandleReadDoneState();
    break;

  case IPC_SERVER_STATE_WRITING:
    HandleWritingState();
    break;

  case IPC_SERVER_STATE_WRITE_DONE:
    HandleWriteDoneState();
    break;

  case IPC_SERVER_STATE_READING_ACK:
    HandleReadingAckState();
    break;

  case IPC_SERVER_STATE_DISCONNECTING:
    HandleDisconnectingState();
    break;

  default:
    // If we reach here, it indicates that we added one more
    // state without adding handling code!!
    server_state_ = IPC_SERVER_STATE_ERROR;
    break;
  }
}

bool CrashGenerationServer::AddClient(const ClientInfo& client_info) {
  HANDLE wait_handle;

  ClientInfoEx* client_info_ex = new ClientInfoEx(client_info, this);

  // OnDumpRequest will be called when dumpRequested is signalled.
  if (!RegisterWaitForSingleObject(&wait_handle,
                                   client_info.dump_requested_handle, 
                                   OnDumpRequest,
                                   client_info_ex,
                                   INFINITE,
                                   WT_EXECUTEONLYONCE)) {
    delete client_info_ex;
    return false;
  }

  client_info_ex->client.dump_request_wait_handle = wait_handle;

  // OnClientEnd will be called when the client process terminates.
  if (!RegisterWaitForSingleObject(&wait_handle,
                                   client_info.process,
                                   OnClientEnd,
                                   client_info_ex,
                                   INFINITE,
                                   WT_EXECUTEONLYONCE)) {
    delete client_info_ex;
    return false;
  }

  client_info_ex->client.process_exit_wait_handle = wait_handle;

  // New scope to acquire and release lock automatically.
  {
    AutoLock lock(&clients_sync_);
    clients_.push_back(client_info_ex);
  }

  return true;
}


// static
void CALLBACK CrashGenerationServer::OnPipeConnected(
  void* context,
  BOOLEAN /* timer_or_wait */) {
  assert (context != NULL);

  CrashGenerationServer* obj =
    reinterpret_cast<CrashGenerationServer*>(context);
  obj->ServiceConnection();
}

// static
void CALLBACK CrashGenerationServer::OnDumpRequest(
  void* context,
  BOOLEAN /* timer_or_wait */) {
  assert(context != NULL);
  ClientInfoEx* client_info_ex = reinterpret_cast<ClientInfoEx*>(context);

  assert(client_info_ex->crash_server != NULL);
  client_info_ex->crash_server->ServiceDump(client_info_ex);
}

// static
void CALLBACK CrashGenerationServer::OnClientEnd(
  void* context,
  BOOLEAN /* timer_or_wait */) {
  assert(context != NULL);

  QueueUserWorkItem(ReleaseObjects, context, WT_EXECUTEDEFAULT);
}

DWORD WINAPI CrashGenerationServer::ReleaseObjects(void* context) {
  ClientInfoEx* client_info_ex = reinterpret_cast<ClientInfoEx*>(context);

  assert(client_info_ex->crash_server != NULL);

  client_info_ex->crash_server->DoCleanUp(client_info_ex);
  return 0;
}

void CrashGenerationServer::DoCleanUp(ClientInfoEx* client_info_ex) {
  assert(client_info_ex != NULL);

  client_info_ex->client.CleanUp();

  // Start a new scope to release lock automatically.
  {
    AutoLock lock(&clients_sync_);
    clients_.remove(client_info_ex);
  }

  delete client_info_ex;
}

void CrashGenerationServer::ServiceDump(ClientInfoEx* client_info) {
  assert(client_info != NULL);

  // Generate the dump only if it's explicitly requested by the
  // server application; otherwise the server might want to generate
  // dump in the callback.
  if (generate_dumps_) {
    bool result = DoDump(client_info->client);
    assert(result);
  }

  // Invoke the registered callback.
  if (crash_callback_ != NULL) {
    crash_callback_(crash_context_, client_info->client);
  }

  HANDLE handle = client_info->client.dump_generated_handle;
  BOOL success = ReleaseSemaphore(handle, 1, NULL);
  assert(success == TRUE);
}

bool CrashGenerationServer::DoDump(const ClientInfo& client) {
  assert(client.pid != 0);
  assert(client.process != NULL);

  // We have to get the address of EXCEPTION_INFORMATION from
  // the client process address space.
  PEXCEPTION_POINTERS client_ex_info;
  SIZE_T bytes_read;

  if (!ReadProcessMemory(client.process,
                         client.ex_info,
                         &client_ex_info,
                         sizeof(client_ex_info),
                         &bytes_read)) {
    return false;
  }

  if (bytes_read != sizeof(client_ex_info)) return false;

  DWORD client_thread_id;

  if (!ReadProcessMemory(client.process,
                         client.thread_id,
                         &client_thread_id,
                         sizeof(client_thread_id),
                         &bytes_read)) {
    return false;
  }

  if (bytes_read != sizeof(client_thread_id)) return false;

  return dump_generator_->WriteMinidump(client.process,
                                        client.pid,
                                        client_thread_id,
                                        GetCurrentThreadId(),
                                        client_ex_info,
                                        client.assert_info,
                                        MiniDumpNormal,
                                        true);
}

}  // namespace google_breakpad
