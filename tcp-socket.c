/*
 *  NanoRadio (Open source IoT hardware)
 *
 *  This project is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License(GPL)
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This project is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */
#define TRACE_UNIT "socket"

#define NO_RTL
#include "portable.h"

#if PORT (SOCKET)
# include <netdb.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <unistd.h>
# define closesocket close

#elif PORT (LWIP)
# include "include/lwip/lwip/sockets.h"
# include "include/lwip/lwip/dns.h"
# include "include/lwip/lwip/netdb.h"

#else
#error no socket interface defined!
#endif

#undef NO_RTL
#include "portable.h"
#include "util-logtrace.h"
#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/debug.h>

#include "tcp-socket.h"

#define DEBUG_TCP_SOCKET 0

static const unsigned char entropy[4] = {1};
static size_t entropy_len = sizeof(entropy);

static int
NC_P(parsehost)(const char *host, struct sockaddr_in *ip)
{
  struct hostent *hp;
  struct in_addr **addr_list;

  hp = gethostbyname (host);
  if (!hp)
    {
      trace_error (("failed to parse host IP address\n"));
      return -1;
    }
  addr_list = (struct in_addr **)hp->h_addr_list;
  if (addr_list[0])
    {
      ip->sin_family = AF_INET;
      ws_memcpy (&ip->sin_addr, addr_list[0], sizeof(ip->sin_addr));
      return 0;
    }
  return -WERR_CONN_FATAL;
}

static void
tcp_socket_reset(tcp_socket_t *socket)
{
  ws_bzero(socket, sizeof(*socket));
  socket->fd = -1;
  socket->sslconn = 0;
}

static void
ssl_debug(void *ctx, int level, const char *file, int line, const char *str )
{
  WS_UNUSED(level);
  fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
  fflush(  (FILE *) ctx  );
}

/**
 * @brief Connect to a server using a TCP connection
 * @return -2 for fatal error, like unable to resolve name, connection timeout...
 * @return -1 is unable to connect to a particular port
 */
int
NC_P(ws_socket_conn)(tcp_socket_t *tsock, const char *host, int port, int retry)
{
  int rc;
  struct sockaddr_in remote_ip;
  const char *host_dispname;
  
  static char inited = 0;
  if (!inited)
    {
#if PORT (WINSOCK2)
      WSADATA wsdata;
      WSAStartup(0x0202, &wsdata); /* there might be a better place for this (-> later) */
#endif
      inited = 1;
    }
  tcp_socket_reset(tsock);

  if( (tsock->sslconn = ( port == 443 )) )
    {

    }
  
  while( retry )
    {
      if( tsock->sslconn )
        {
          mbedtls_net_init(&(tsock->net));
          mbedtls_ssl_init(&(tsock->ssl));
          mbedtls_ssl_config_init(&tsock->conf);
          mbedtls_ctr_drbg_init(&tsock->ctr_drbg);
          mbedtls_entropy_init(&tsock->entropy);
          mbedtls_ctr_drbg_seed(&tsock->ctr_drbg, mbedtls_entropy_func, &tsock->entropy, entropy, entropy_len);

          /* Create SSL for data transmission */
          if( ( rc = mbedtls_net_connect( &tsock->net, host, "443", MBEDTLS_NET_PROTO_TCP ) ) )
            {
              closesocket(tsock->fd);
              trace_info (("ssl connection failed rc = %d\n", rc));
              retry--;
              continue;
            }

          if( ( rc = mbedtls_ssl_config_defaults( &tsock->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
            {
              trace_error(("mbedtls_ssl_config_defaults() returned %d\n\n", rc));
              return -WERR_CONN_FATAL;
            }
          mbedtls_ssl_conf_authmode( &tsock->conf, MBEDTLS_SSL_VERIFY_NONE );
          mbedtls_ssl_conf_rng( &tsock->conf, mbedtls_ctr_drbg_random, &tsock->ctr_drbg );
          mbedtls_ssl_conf_dbg( &tsock->conf, ssl_debug, stdout );

          mbedtls_ssl_set_bio( &tsock->ssl, &tsock->net, mbedtls_net_send, mbedtls_net_recv, NULL );

          if( (rc = mbedtls_ssl_setup(&tsock->ssl, &tsock->conf)) )
            {
              trace_error(( "mbedtls_ssl_setup() returned %d\n\n", rc ));
              return -WERR_CONN_FATAL;
            }

          while( (rc = mbedtls_ssl_handshake(&tsock->ssl)) )
            {
              if( rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE )
                {
                  trace_error(( "mbedtls_ssl_handshake() returned %d\n\n", rc ));
                  return -WERR_CONN_FATAL;
                }
            }
          trace_info(("SSL Connected\n"));
        }
      else
        {
          ws_bzero (&remote_ip, sizeof(struct sockaddr_in));
          if (parsehost (host, &remote_ip))
            {
              retry--;
              continue;
            }
          tsock->fd = socket(PF_INET, SOCK_STREAM, 0);
          if (tsock->fd == -1)
            {
              trace_error (("create a socket\n"));
              continue;
            }

          remote_ip.sin_port = htons (port);

    #if PORT (LWIP)
          host_dispname = ipaddr_ntoa((const ip_addr_t*)&remote_ip.sin_addr.s_addr);
    #else
          host_dispname = host;
    #endif

          trace_info (("connecting to server %s...\n", host_dispname));
          if( connect(tsock->fd, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr))!=00 )
            {
              closesocket(tsock->fd);
              trace_info (("connection failed\n"));
              retry--;
              continue;
            }
        }
      return 0;
    }
  return -WERR_CONN_FATAL;
}

int
NC_P(ws_socket_recv)(tcp_socket_t *tsock, char *buffer, size_t size, int flags)
{
  if( tsock->sslconn )
    {
      for(;;)
        {
          int rc = mbedtls_ssl_read(&tsock->ssl, (unsigned char *)buffer, size);
          if( rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE )  /* retry when required */
            continue;
          else if( rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY )
            {
#if DEBUG_TCP_SOCKET > 1
              trace_error(( "mbedtls_ssl_read() returned %d\n\n", rc ));
#endif
              mbedtls_ssl_close_notify( &tsock->ssl );
              return -WERR_FAILED;
            }
          else if( rc < 0 )
            {
#if DEBUG_TCP_SOCKET > 1
              trace_error(( "mbedtls_ssl_read() returned %d\n\n", rc ));
#endif
              return -WERR_FAILED;
            }
          return rc;
        }
    }
  else
    return recv(tsock->fd, buffer, size, flags);
}

int
NC_P(ws_socket_send)(tcp_socket_t *tsock, const char *buffer, size_t size, int flags)
{
  if( tsock->sslconn )
    {
      int rc;
      while( ( rc = mbedtls_ssl_write( &tsock->ssl, (unsigned char *)buffer, size ) ) <= 0 ) /* retry when required */
        {
          if( rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE )
            {
#if DEBUG_TCP_SOCKET > 1
              trace_error(( "mbedtls_ssl_write() returned %d\n\n", rc ));
#endif
              return -WERR_FAILED;
            }
        }
      return rc;
    }
  else
    return send(tsock->fd, buffer, size, flags);
}

void
NC_P(ws_socket_close)(tcp_socket_t *tsock)
{
  if( tsock->sslconn )
    {
      mbedtls_net_free( &tsock->net );
      mbedtls_ssl_free( &tsock->ssl );
      mbedtls_ssl_config_free( &tsock->conf );
      mbedtls_ctr_drbg_free( &tsock->ctr_drbg );
      mbedtls_entropy_free( &tsock->entropy );
    }
  else
    {
      if( tsock->fd >= 0 )
        closesocket(tsock->fd);
    }
  tcp_socket_reset(tsock);
}
