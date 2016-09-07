gcc -o srvtest \
    -I src/ \
    -I src/external/clib \
    src/tests/srvtest.c \
    src/external/clib/evloop.c \
    src/external/clib/list.c \
    src/external/clib/buf.c \
    src/external/clib/system.c \
    src/external/clib/mem.c \
    src/external/clib/net.c \
    src/external/clib/net_udp.c \
    src/external/md5/md5.c \
    src/network/layer1.c \
    -g \
    -Wall \
    -Wno-unused-function \
    -Wno-unused-variable

