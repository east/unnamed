gcc -o srvtest \
    -I src/ \
    -I src/external/clib \
    src/tests/srvtest.c \
    src/external/clib/evloop.c \
    src/external/clib/list.c \
    src/external/clib/buf.c \
    src/external/clib/system.c \
    src/external/clib/mem.c \
    src/network/layer1.c
