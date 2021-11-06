#include <stdio.h>

#include <SDL2/SDL.h>
#include "rhc/rhc_impl.h"

int main(int argc, char **argv) {
    SocketServer server = socketserver_new("0.0.0.0", 10000);
    for(;;) {
        Socket *client = socketserver_accept(&server);
        
        Stream_i s = socket_get_stream(client);
        
        stream_write_msg(s, "Hi", 3);
        
        socket_kill(&client);
    }
}
