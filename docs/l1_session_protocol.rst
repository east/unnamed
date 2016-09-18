========================
Layer 1 Session Protocol
========================

Header
======
``4*8:<session token> 13:<cl/srv magic> 3:<msg type> ...``

- ``cl magic - client -> server (proposal: 0x1e2a)``
- ``srv magic - server -> client (proposal: 0x0e7c)``

Control Types
=============
client -> server
  - requestToken
  - verifyToken

server -> client
  - ignoreRequest
  - Token
  - invalidToken

both <->
  - pingpong
  - msg
  - close
    
Detailed
========
requestToken:
  ``4*8:<tmp client token> 13:<cl magic> 000 4*8:<service id>``
verifyToken:
  ``4*8:<session token> 13:<cl magic> 001``

Token:
  ``4*8:<session token> 13:<srv magic> 000 4*8:<new session token>``

close:
  ``4*8:<session token> 13:<srv/cl magic> 101 [x:<reason>]``

pingpong:
  ``4*8:<session token> 13:<srv/cl magic> 110 8:<ping/pong> 4*8:<tmp token>``

msg:
  ``4*8:<session token> 13:<srv/cl magic> 111 x:<payload>``

Token Handshake
---------------
client -> server
  requestToken: client token
server -> client
  Token: session token, client token
client -> server
  verifyToken: session token


