#ifndef TCP_SOCKET_H_
#define TCP_SOCKET_H_

#include "portable.h"

#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

typedef struct tcp_socket_s {
  int fd;
  char sslconn;
  mbedtls_net_context net;
  mbedtls_ssl_config conf;
  mbedtls_ssl_context ssl;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
} tcp_socket_t;

int ws_socket_conn (tcp_socket_t *socket, const char *host, int port, int retry);
int ws_socket_recv (tcp_socket_t *socket,  char *buffer, size_t size, int flags);
int ws_socket_send (tcp_socket_t *socket,  const char *buffer, size_t size, int flags);
void ws_socket_close(tcp_socket_t *socket);

#endif //!defined(TCP_SOCKET_H_)
