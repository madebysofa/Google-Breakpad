=========================================================================
 State machine transitions for the Crash Generation Server
=========================================================================

=========================================================================
               |
 STATE         | ACTIONS
               |
=========================================================================
 ERROR         | Clean up resources used to serve clients.
               | Always remain in ERROR state.
-------------------------------------------------------------------------
 INITIAL       | Connect to the pipe asynchronously.
               | If connection is successfully queued up asynchronously,
               | get into CONNECTING state.
               | If connection is done synchronously, get into CONNECTED
               | state.
               | For any issues, get into ERROR state.
-------------------------------------------------------------------------
 CONNECTING    | Get the result of async connection request.
               | If I/O is still incomplete, remain in the CONNECTING
               | state.
               | If connection is complete, get into CONNECTED state.
               | For any issues, get into DISCONNECTING state.
-------------------------------------------------------------------------
 CONNECTED     | Read from the pipe asynchronously.
               | If read request is successfully queued up asynchronously,
               | get into READING state.
               | For any issues, get into DISCONNECTING state.
-------------------------------------------------------------------------
 READING       | Get the result fo async read request.
               | If read is done, get into READ_DONE state.
               | For any issues, get into DISCONNECTING state.
-------------------------------------------------------------------------
 READ_DONE     | Register the client and prepare the reply. If any of
               | that fails, get in the DISCONNECTING state.
               | Write the reply to the pipe asynchronously.
               | If write request is successfully queued up asynchronously,
               | get into WRITING state.
               | For any issues, get into DISCONNECTING state.
-------------------------------------------------------------------------
 WRITING       | Get the result of the async write request.
               | If write is done, get into WRITE_DONE state.
               | For any issues, get into the DISCONNECTING state.
-------------------------------------------------------------------------
 WRITE_DONE    | Read from the pipe asynchronously (for an ACK).
               | If read request is successfully queued up asynchonously,
               | get into READING_ACK state.
               | For any issues, get into the DISCONNECTING state.
-------------------------------------------------------------------------
 READING_ACK   | Get the result of the async read request.
               | If read is done, perform action for successful client
               | connection.
               | Get into the DISCONNECTING state.
-------------------------------------------------------------------------
 DISCONNECTING | Disconnect from the pipe and reset the event. If
               | anything fails, get in the ERROR state.
               | Get into the INITIAL state and signal the event again.
=========================================================================
