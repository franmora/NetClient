v3
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <signal.h>

#define PRINT	printf

bool Terminate = false;
const int Timeout = 10;

static void SignalHandler(int sig)
{
	if (!Terminate) {
		Terminate = true;
		PRINT("%s: Terminate\n", __FUNCTION__);
	}
}

static bool ShouldTerminate(void)
{
	if (_kbhit()) {
		int c = _getch();
		if (c == 0x1b) Terminate = true;
	}

	return Terminate;
}

bool Connect(SOCKET s, const sockaddr_in* sa)
{
	if (!Terminate && connect(s, (SOCKADDR*)sa, sizeof(sockaddr_in)) == 0) // OK
		return true;

	for (int i = 0; i < Timeout && !Terminate; i++) {
		fd_set writeFds;
		struct timeval timeout;

		FD_ZERO(&writeFds);
		FD_SET(s, &writeFds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int result = select(0, NULL, &writeFds, NULL, &timeout); // timeout == 0
		if (result > 0) { // FDs are set
			if (FD_ISSET(s, &writeFds)) { // OK
				int err;
				int len = sizeof(err);
				if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &len) < 0) {
					PRINT("%s: getsockopt %s\n", __FUNCTION__, strerror(errno));
					return false;
				}
				else if (err != 0) {
					PRINT("%s: getsockopt %s\n", __FUNCTION__, strerror(err));
					return false;
				}
				else return true;
			}
			else { // should never happen
				PRINT("%s: unknown error\n", __FUNCTION__);
				return false;
			}
		}
		else if (result < 0) { // WSAGetLastError
			PRINT("%s: select %s\n", __FUNCTION__, strerror(errno));
			return false;
		}
	}
	PRINT("%s: timeout\n", __FUNCTION__);
	return false; // timeout or terminate
}

int Recv(SOCKET s, char* buf, int len)
{
	for (int i = 0; i < Timeout && !Terminate; i++) {
		struct timeval timeout;
		fd_set readFds;

		FD_ZERO(&readFds);

		FD_SET(s, &readFds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int ret = select(0, &readFds, NULL, NULL, &timeout); // timeout == 0
		if (ret > 0 && FD_ISSET(s, &readFds)) {
			return recv(s, buf, len, 0);
		}
		else if (ret < 0) {
			PRINT("%s: select %d\n", __FUNCTION__);
			return -1;
		}
	}
	PRINT("%s: timeout\n", __FUNCTION__);
	return -1;
}

int Send(SOCKET s, char* buf, int len)
{
	for (int i = 0; i < Timeout && !Terminate; i++) {
		struct timeval timeout;
		fd_set writeFds;

		FD_ZERO(&writeFds);

		FD_SET(s, &writeFds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		int ret = select(0, NULL, &writeFds, NULL, &timeout); // timeout == 0
		if (ret > 0 && FD_ISSET(s, &writeFds)) {
			return send(s, buf, len, 0);
		}
		else if (ret < 0) {
			PRINT("%s: select %d\n", __FUNCTION__);
			return -1;
		}
	}
	PRINT("%s: timeout\n", __FUNCTION__);
	return -1;
}

int main(void)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	signal(SIGINT, SignalHandler);

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s != INVALID_SOCKET) {
		u_long iMode = 1;
		ioctlsocket(s, FIONBIO, &iMode);

		sockaddr_in sa;
		sa.sin_family = AF_INET;
		//sa.sin_addr.s_addr = inet_addr( "87.143.9.149" );
		sa.sin_addr.s_addr = inet_addr( "127.0.0.1" );
		sa.sin_port = htons(19);

		if (Connect(s, &sa)) {
			while (!ShouldTerminate()) {
				char buf[1024];
				int len = Recv(s, buf, 74);
				buf[72] = 0;
				printf("%s\n", buf);
				Sleep(250);
			}
		}
		closesocket(s);
	}
	WSACleanup();

	return 0;
}
