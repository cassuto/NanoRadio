#ifndef TCP_SOCKET_H_
#define TCP_SOCKET_H_

#include "portable.h"

#if USING(MBEDTLS)
# include <mbedtls/ssl.h>
# include <mbedtls/net_sockets.h>
# include <mbedtls/entropy.h>
# include <mbedtls/ctr_drbg.h>
#endif

typedef struct tcp_socket_s {
  int fd;
  char sslconn;
#if USING(MBEDTLS)
  mbedtls_net_context net;
  mbedtls_ssl_config conf;
  mbedtls_ssl_context ssl;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
#endif
} tcp_socket_t;

int NC_P(ws_socket_conn) (tcp_socket_t *socket, const char *host, int port, int retry);
int NC_P(ws_socket_recv) (tcp_socket_t *socket,  char *buffer, size_t size, int flags);
int NC_P(ws_socket_send) (tcp_socket_t *socket,  const char *buffer, size_t size, int flags);
void NC_P(ws_socket_close) (tcp_socket_t *socket);

#endif //!defined(TCP_SOCKET_H_)
