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

#define IN_TCP_SOCKET
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

#undef IN_TCP_SOCKET
#include "portable.h"

#define TRACE_UNIT "socket"

#include "util-logtrace.h"
#include "tcp-socket.h"

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

/**
 * @brief Connect to a server using a TCP connection
 * @return -2 for fatal error, like unable to resolve name, connection timeout...
 * @return -1 is unable to connect to a particular port
 */
int
NC_P(ws_socket_conn)(const char *host, int port, int retry)
{
  int sock;
  struct sockaddr_in remote_ip;
  const char *host_dispname;

#if PORT (WINSOCK2)
  static char inited = 0;
  if (!inited) {
      WSADATA wsdata;
      WSAStartup(0x0202, &wsdata); // there might be a better place for this (-> later)
      inited = 1;
  }
#endif

  while(retry)
    {
      ws_bzero (&remote_ip, sizeof(struct sockaddr_in));
      if (parsehost (host, &remote_ip))
        {
          retry--;
          continue;
        }
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
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
      if (connect(sock, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr))!=00)
        {
          closesocket(sock);
          trace_info (("connection failed\n"));
          retry--;
          continue;
        }
      return sock;
    }
  return -WERR_CONN_FATAL;
}

int
NC_P(ws_socket_recv)(int socket, char *buffer, size_t size, int flags)
{ return recv(socket, buffer, size, flags); }

int
NC_P(ws_socket_send)(int socket, const char *buffer, size_t size, int flags)
{ return send(socket, buffer, size, flags); }

int
NC_P(ws_socket_close)(int socket)
{ return closesocket(socket); }
