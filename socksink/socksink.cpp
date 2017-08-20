#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <stdio.h>
#include <string.h>
#pragma comment(lib, "ws2_32.lib")

unsigned short g_listenPort{ htons(23455) };

int SocketError();

struct WSAGuard {
	WSAGuard(WSADATA& data) { WSAStartup(MAKEWORD(2, 2), &data); }
	~WSAGuard() { WSACleanup(); }
};

struct DualSocket {
	SOCKET sock;
	
	DualSocket() {
		DWORD dwZero{ 0 };
		sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (sock == INVALID_SOCKET) throw INVALID_SOCKET;
		int result = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&dwZero, sizeof(DWORD));
		if (result) throw result;
	}

	~DualSocket() {
		closesocket(sock);
	}
};

struct DualSocketListener : DualSocket {
	DualSocketListener(unsigned short nPort) {
		SOCKADDR_IN6 addr{ AF_INET6, nPort, 0, IN6ADDR_ANY_INIT };
		if (bind(sock, (sockaddr*)&addr, sizeof(SOCKADDR_IN6)))
			throw SocketError();
		if (listen(sock, SOMAXCONN))
			throw SocketError();
	}

	SOCKET Accept(SOCKADDR_IN6* pAddr)
	{
		fd_set listenset;
		FD_ZERO(&listenset);
		FD_SET(sock, &listenset);

		if (select(0, &listenset, NULL, NULL, NULL) == SOCKET_ERROR) throw SocketError();

		int remoteAddrLength{ sizeof(SOCKADDR_IN6) };
		if (!pAddr) remoteAddrLength = 0;

		SOCKET s = accept(sock, (sockaddr*)pAddr, &remoteAddrLength);
		if (s == INVALID_SOCKET) throw SocketError();
		return s;
	}
};

int SocketError()
{
	int sockErr = WSAGetLastError();
	DWORD size{ 4095 };
	TCHAR* buffer = new TCHAR[size + 1];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, sockErr, 0, buffer, size, NULL);
	printf("Winsock error: %d\n\t%S\n", sockErr, buffer);
	delete [] buffer;
	return sockErr;
}

int main()
{
	WSADATA wsaData;
	WSAGuard wsaGuard{ wsaData };

	try 
	{
		DualSocketListener ListenSocket(g_listenPort);
		unsigned nPort = ntohs(g_listenPort);
		//printf("Listening on port: %u\n", nPort);

		for (;;)
		{
			printf(">>> Listening on port: %u\n", nPort);
			int namelen { sizeof(SOCKADDR_IN6) };
			SOCKADDR_IN6 remoteAddr;
			SOCKADDR_IN6 socketName;
			SOCKET readSocket = ListenSocket.Accept(&remoteAddr);
			getsockname(readSocket, (sockaddr*)&socketName, &namelen);

			unsigned localPort{ ntohs(socketName.sin6_port) }, remotePort{ ntohs(remoteAddr.sin6_port) };
			char remoteHostIpStr[64];
			char localHostIpStr[64];
			inet_ntop(remoteAddr.sin6_family, &remoteAddr.sin6_addr, remoteHostIpStr, 64);
			inet_ntop(socketName.sin6_family, &socketName.sin6_addr, localHostIpStr, 64);
			printf(">>> Connected on (%s, %u) to remote host (%s, %u)\n", localHostIpStr, localPort, remoteHostIpStr, remotePort);

			fd_set readfds;
			while (readSocket != INVALID_SOCKET)
			{
				try {
					FD_ZERO(&readfds);
					FD_SET(readSocket, &readfds);
					int nfds = select(0, &readfds, NULL, NULL, NULL);
					if (nfds == SOCKET_ERROR)
						throw SocketError();
					
					unsigned long nToRead{ 0 };
					ioctlsocket(readSocket, FIONREAD, &nToRead);
					char* buffer = new char[nToRead + 1];
					buffer[nToRead] = 0;
					int amountRead = recv(readSocket, buffer, nToRead, 0);
					if (amountRead == SOCKET_ERROR)
						throw SocketError();
					if (!amountRead)
						throw amountRead;

					for (char* p = buffer; p < buffer + nToRead; ++p) { if (!*p) *p = ' '; }
					printf("%s", buffer);
				}
				catch (...)
				{ // Just close the socket and report the error
					printf(">>> Connection closed\n");
					closesocket(readSocket);
					readSocket = INVALID_SOCKET;
				}
			}
		}
	}
	catch (...)
	{
	}
}
