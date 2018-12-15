#ifndef TCP_SOCKET_H_
#define TCP_SOCKET_H_

int ws_socket_conn (const char *host, int port, int retry);
int ws_socket_recv (int socket, char *buffer, size_t size, int flags);
int ws_socket_send (int socket, const char *buffer, size_t size, int flags);
int ws_socket_close(int socket);

#endif //!defined(TCP_SOCKET_H_)
