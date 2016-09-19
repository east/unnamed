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
Header
------
  ``4*8:<tmp client token> 16:(13:<srv/cl magic> 3:<ctrl type>)``

Control Type Sub Headers
--------------------
requestToken (0x0):
  ``4*8:<service id>``
verifyToken (0x1):
  no additional headers

Token (0x0):
  ``4*8:<new session token>``

close (0x5):
  ``[x:<reason>]``

pingpong (0x6):
  ``8:<ping/pong> 4*8:<tmp token>``

msg (0x7):
  ``x:<payload>``

Token Handshake
---------------
client -> server
  requestToken: client token
server -> client
  Token: session token, client token
client -> server
  verifyToken: session token


