=========================
Layer 2 Transfer Protocol
=========================

Transfer Modes
==============
L2 has three different transfer modes:

- Unreliable Transfer
- Reliable Sequenced Transfer
- Reliable Block Transfer

Each L2 frame consists of a list of chunks, while
each chunk type (one type per transfer mode) has its
own sub header.

Protocol
========
Chunk Header
------------

``3:<chunk type> 13:<chunk size>``

(chunk size -> sub header + data)

Sub Headers
-----------
Unreliable Transfer
  ``8:<seq> x:<data>``

Reliable Sequenced Transfer
  ``3:<ctrl type> 5:<channel> 8:<ack> 8:<seq> [x:<data>]``

Reliable Block Transfer
  ``3:<ctrl type> 5:<channel> 8:<rate>``

    ``[32:<block chunk id> x:<data>]``

    ``[32:<from> 32:<num>]``

    ``[8:<num ids> (compressed uint30 list)]``

