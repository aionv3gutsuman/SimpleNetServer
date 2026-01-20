#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define CLIENTNUM 2

unsigned __stdcall MultiThreadFunc(void* pArguments);

int main()
{
	HANDLE handles[CLIENTNUM];
	unsigned threadID[CLIENTNUM];

	for (int i = 0; i < CLIENTNUM; ++i)
	{
		handles[i] = (HANDLE)_beginthreadex(NULL, 0, &MultiThreadFunc, (void*)i, 0, &threadID[i]);
		Sleep(1);
	}

	for (int i = 0; i < CLIENTNUM; ++i)
	{
		WaitForSingleObject(handles[i], INFINITE);
		CloseHandle(handles[i]);
	}

	return 0;
}

unsigned __stdcall MultiThreadFunc(void* pArguments)
{
	printf("ThreadNo.%d\n", (int)pArguments);

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);

	SOCKET listenSock[CLIENTNUM];
	SOCKET clientSock[CLIENTNUM] = { INVALID_SOCKET, INVALID_SOCKET };
	struct sockaddr_in addr[CLIENTNUM];
	struct sockaddr_in clientAddr;
	int clientLen = sizeof(clientAddr);

	char buffersend[256] = "FROM SERVER";
	char bufferrecv[256];

	// --- リッスンソケット作成 ---
	for (int i = 0; i < CLIENTNUM; i++)
	{
		listenSock[i] = socket(AF_INET, SOCK_STREAM, 0);

		u_long mode = 1;
		ioctlsocket(listenSock[i], FIONBIO, &mode);

		addr[i].sin_family = AF_INET;
		addr[i].sin_addr.s_addr = INADDR_ANY;
		addr[i].sin_port = htons(i == 0 ? 5000 : 6000);

		bind(listenSock[i], (struct sockaddr*)&addr[i], sizeof(addr[i]));
		listen(listenSock[i], 5);
	}

	printf("Waiting for 2 clients...\n");

	// --- 2台のクライアントが接続されるまで待つ ---
	// --- 非ブロッキング connect ---
	int connectedCount = 0;

	while (connectedCount < CLIENTNUM)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(listenSock[0], &rfds);
		FD_SET(listenSock[1], &rfds);

		select(0, &rfds, NULL, NULL, NULL);

		for (int i = 0; i < CLIENTNUM; i++)
		{
			if (FD_ISSET(listenSock[i], &rfds))
			{
				SOCKET cs = accept(listenSock[i], (struct sockaddr*)&clientAddr, &clientLen);
				if (cs != INVALID_SOCKET)
				{
					clientSock[i] = cs;
					connectedCount++;

					printf("Client connected on port %d\n", (i == 0 ? 5000 : 6000));
				}
			}
		}
	}

	printf("Both clients connected!\n");

	// --- メインループ（送受信を同時監視） ---
	while (1)
	{
		fd_set rfds, wfds;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		for (int i = 0; i < CLIENTNUM; i++)
		{
			if (clientSock[i] != INVALID_SOCKET)
			{
				FD_SET(clientSock[i], &rfds);
				FD_SET(clientSock[i], &wfds);
			}
		}

		TIMEVAL tv = { 1, 0 };
		int ret = select(0, &rfds, &wfds, NULL, &tv);

		if (ret > 0)
		{
			for (int i = 0; i < CLIENTNUM; i++)
			{
				// --- 送信可能なら送信 ---
				if (FD_ISSET(clientSock[i], &wfds))
				{
					send(clientSock[i], buffersend, strlen(buffersend), 0);
				}

				// --- 受信可能なら受信 ---
				if (FD_ISSET(clientSock[i], &rfds))
				{
					int r = recv(clientSock[i], bufferrecv, sizeof(bufferrecv)-1, 0);

					if (r > 0)
					{
						bufferrecv[r] = '\0';
						printf("Client %d RECV: %s\n", i, bufferrecv);
					}
					else if (r == 0)
					{
						printf("Client %d disconnected\n", i);
						closesocket(clientSock[i]);
						clientSock[i] = INVALID_SOCKET;
					}
				}
			}
		}

		Sleep(1);
	}

	WSACleanup();
	return 0;
}