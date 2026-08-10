# Connection lifecycle

> Status: outline

This chapter will describe the complete lifetime of a `ts-cli` connection after the bootstrap itself.

## Planned sections

1. Endpoint resolution and UDP socket setup
2. [Handshake and session bootstrap](handshake.md)
3. Login completion and assigned client ID
4. Initial channel/client state
5. Continuous receive loop
6. Client-originated actions
7. Ping/Pong keepalive
8. Reliable-command retransmission
9. Timeouts and connection-loss detection
10. Graceful disconnect
11. Reconnect policy

Related chapters: [Reliability and sequencing](reliability.md), [Server state](state.md), [Runtime model](../client/runtime.md).
