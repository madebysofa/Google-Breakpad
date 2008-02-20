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

#ifndef CLIENT_WINDOWS_CRASHGENERATION_CRASH_GENERATION_SERVER_H__
#define CLIENT_WINDOWS_CRASHGENERATION_CRASH_GENERATION_SERVER_H__

#include <list>

#include "client/windows/common/ipc_protocol.h"
#include "client/windows/CrashGeneration/client_info.h"
#include "client/windows/CrashGeneration/minidump_generator.h"

using std::list;

namespace google_breakpad {

// Abstraction for server side implementation of out-of-process (OoP)
// crash generation protocol. It generates minidumps (Windows platform)
// for client processes that request dump generation.
class CrashGenerationServer {
 public:
  // Type for a successful client connection callback.
  typedef void (*OnClientConnectedCallback)(
    void* context, const ClientInfo& client_info);

  // Type for a client crash callback.
  typedef void (*OnClientCrashedCallback)(
    void* context, const ClientInfo& client_info);

  // Creates a default instance.
  CrashGenerationServer(const wchar_t* pipe_name);

  // Creates an instance with the given parameters.
  //
  // @param pipe_name: Name of the pipe
  // @param connect_callback: Callback for a new client connection.
  // @param connect_context: Context for client connection callback.
  // @param crash_callback: Callback for a client crash dump request.
  // @param crash_context: Context for client crash dump request callback.
  // @param generate_dumps:
  //   Whether to automatically generate dumps or not. Client code of
  //   this class might want to generate dumps explicitly in the crash
  //   dump request callback. In that case, false can be passed for this
  //   parameter.
  // @parameter dump_path:
  //   Path for generating dumps; required only if true is passed
  //   for generateDumps parameter; NULL can be passed otherwise.
  CrashGenerationServer(const wchar_t* pipe_name,
                        OnClientConnectedCallback connect_callback,
                        void* connect_context,
                        OnClientCrashedCallback crash_callback,
                        void* crash_context,
                        bool generate_dumps,
                        const wstring* dump_path);

  // Destructor.
  ~CrashGenerationServer();

  // Performs initialization steps needed to start listening to clients.
  //
  // @returns True if initialization is successful; false otherwise.
  bool Initialize();

 private:
  // Extended client information.
  struct ClientInfoEx {
    // Client info instance.
    ClientInfo client;

    // CrashGenerationServer instance that did the initial handshake with
    // the client.
    CrashGenerationServer* crash_server;

    // Creates an instance with the given client info and given crash
    // generator insance.
    ClientInfoEx(const ClientInfo& a_client,
                 CrashGenerationServer* a_crash_server)
        : client(a_client), crash_server(a_crash_server) {
    }
  };

  // Various states the client can be in during the handshake with
  // the server.
  enum IPCServerState {
    // Server is in error state and cannot server any clients.
    IPC_SERVER_STATE_ERROR = -1,

    // Server starts in this state.
    IPC_SERVER_STATE_INITIAL = 0,

    // Server has issued an async connect to pipe and is waiting for
    // connection to be established.
    IPC_SERVER_STATE_CONNECTING = 1,

    // Server is connected successfully.
    IPC_SERVER_STATE_CONNECTED = 2,

    // Server has issued an async read from pipe and is waiting for
    // the read to finish.
    IPC_SERVER_STATE_READING = 3,

    // Server is done reading from the pipe.
    IPC_SERVER_STATE_READ_DONE = 4,

    // Server has issued an async write to the pipe and is waiting for
    // the write to finish.
    IPC_SERVER_STATE_WRITING = 5,

    // Server is done writing to the pipe.
    IPC_SERVER_STATE_WRITE_DONE = 6,

    // Server has issued an async read from pipe for the ack from the
    // client and is waiting for the read to finish.
    IPC_SERVER_STATE_READING_ACK = 7,

    // Server is cone writing to the pipe and is now ready to disconnect
    // and reconnect.
    IPC_SERVER_STATE_DISCONNECTING = 8
  };

  // Pipe name
  const wchar_t* pipe_name_;

  // Callback for a successful client connection.
  OnClientConnectedCallback connect_callback_;

  // Context for OnClientConnected callback.
  void* connect_context_;

  // Callback for a client crash.
  OnClientCrashedCallback crash_callback_;

  // Context for OnClientCrash callback.
  void* crash_context_;

  //
  // Helper methods to handle various server IPC states.
  //
  void HandleErrorState();
  void HandleInitialState();
  void HandleConnectingState();
  void HandleConnectedState();
  void HandleReadingState();
  void HandleReadDoneState();
  void HandleWritingState();
  void HandleWriteDoneState();
  void HandleReadingAckState();
  void HandleDisconnectingState();

  // Validates client's request message to make sure there
  // are no errors.
  bool ValidateClientRequest(const IPCMsg& msg);

  // Helper to prepare reply for a client from the given parameters.
  bool PrepareReply(IPCMsg* reply, const ClientInfo& client_info);

  // Service a client's connection request.
  void ServiceConnection();

  // Service a dump request from the client.
  void ServiceDump(ClientInfoEx* client_info);

  // Callback for pipe connected event.
  static void CALLBACK OnPipeConnected(void* context, BOOLEAN timer_or_wait);

  // Callback for a dump request.
  static void CALLBACK OnDumpRequest(void* context, BOOLEAN timer_or_wait);

  // Callback for client process exit event.
  static void CALLBACK OnClientEnd(void* context, BOOLEAN timer_or_wait);

  // Helper to release resources for a client.
  static DWORD WINAPI ReleaseObjects(void* context);

  // Performs cleanup for the given client.
  void DoCleanUp(ClientInfoEx* client_info_ex);

  // Helper to add the given client to the list of registered clients.
  bool AddClient(const ClientInfo& client_info);

  // Whether to generate dumps or not.
  bool generate_dumps_;

  // Instance of a mini dump generator.
  MinidumpGenerator* dump_generator_;

  // Helper to generate dump for the given client.
  bool DoDump(const ClientInfo& client);

  // Sync object for thread-safe access to the shared list of clients.
  CRITICAL_SECTION clients_sync_;

  // List of clients.
  list<ClientInfoEx*> clients_;

  // Handle to the pipe used for handshake with clients.
  HANDLE pipe_;

  // Pipe wait handle.
  HANDLE pipe_wait_handle_;

  // Handle to server-alive mutex.
  HANDLE server_alive_handle_;

  // Overlapped instance for async I/O on the pipe.
  OVERLAPPED overlapped_;

  // State of the server in performing the IPC with the client.
  // NOTE that since we restrict the pipe to one instance, we
  // only need to keep one state of the server. Otherwise, server
  // would have one state per client it is talking to.
  IPCServerState server_state_;

  // Message object used in IPC with the client.
  IPCMsg msg_;

  // Client Info for the client that's connecting to the server.
  ClientInfo client_info_;
};

}  // namespace google_breakpad

#endif  // CLIENT_WINDOWS_CRASHGENERATION_CRASH_GENERATION_SERVER_H__
