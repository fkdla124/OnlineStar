#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

// todo
// session 지연삭제 마킹은 했는데 삭제하는 코드를 작성하지 않았음.
// 클라이언트 종료시 서버에서도 session삭제 코드를 작성하지 않았음.
// 맨처음 소켓 만들고 listen 하는 부분, 이동메시지 처리 부분, sendUniCast() 함수를 만들지 않았음. -> 만들면 코드가 굉장히 깔끔할 것으로 예상

// 이번엔 헤더로 나눠보기.
#include "Console.h"
#include "MessageProtocol.h"
#include "Network.h"
#include "SocketErrorcode.h"
#include "GameData.h"

// 함수도 만들어보기.
void Init();
void AcceptClient(SOCKET listenSocket);
void Network(SOCKET listenSocket);
void Render();
void FPS();

int main()
{
	// WSAStartup()
	WSADATA wsa;
	returnValueWSAStartup = WSAStartup(MAKEWORD(2, 2), &wsa);
	if (returnValueWSAStartup != 0)
	{
		errorCodeWSAStartUp = WSAGetLastError();
		printf("WSAStartup() error. %d\n", errorCodeWSAStartUp);
		Sleep(INFINITE);
	}

	// socket()
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		errorCodeSocket = WSAGetLastError();
		printf("socket() error. %d\n", errorCodeSocket);
		Sleep(INFINITE);
	}

	// socket LINGER {1, 0} -> RST로 종료하게끔.
	linger _linger = { 1, 0 };
	returnValueSetSockOpt = setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (char*)&_linger, sizeof(_linger));
	if (returnValueSetSockOpt == SOCKET_ERROR)
	{
		errorCodeSetSockOpt = WSAGetLastError();
		printf("setsockopt() LINGER{0, 0} setting error. %d\n", errorCodeSetSockOpt);
		Sleep(INFINITE);
	}

	// serverAddr
	SOCKADDR_IN serverAddr = { 0, };
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	// bind()
	returnValueBind = bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (returnValueBind == SOCKET_ERROR)
	{
		errorCodeBind = WSAGetLastError();
		printf("bind() error. %d\n", errorCodeBind);
		Sleep(INFINITE);
	}

	// listen()
	returnValueListen = listen(listenSocket, SOMAXCONN);
	if (returnValueListen == SOCKET_ERROR)
	{
		errorCodeListen = WSAGetLastError();
		printf("listen() error. %d\n", errorCodeListen);
		Sleep(INFINITE);
	}

	// nonblocking change
	ULONG on = 1;
	returnValueIoctlSocket = ioctlsocket(listenSocket, FIONBIO, &on);
	if (returnValueIoctlSocket == SOCKET_ERROR)
	{
		errorCodeIoctlSocket = WSAGetLastError();
		printf("ioctlsocket() error. %d\n", errorCodeIoctlSocket);
		Sleep(INFINITE);
	}

	// console Init()
	Init();

	SetConsoleTitle(L"OnlineStar Server");

	// timer
	clock_t startFpsTick = clock();
	clock_t startFrameTick = clock();

	// main loop
	while (1)
	{
		// Logic
		// 로직이 없다.

		// Network()
		Network(listenSocket);

		// Render() -> 서버의 상태를 확인하기 위한 표시. (FPS, 초당 메시지처리량 등..) 지금은 맵도 렌더링 함. 보기위해
		Render();

		// FPS
		FPS();
	}

	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		closesocket((*it)->sock);
	}
	closesocket(listenSocket);
	WSACleanup();

	return 0;
}

void Init()
{
	// WinAPI 콘솔 핸들 얻어서 초기화
	InitConsole();
}

void Network(SOCKET listenSocket)
{
	fd_set rset;
	FD_ZERO(&rset);

	// listenSocket 넣기
	FD_SET(listenSocket, &rset);

	// session에 있는 socket 다 넣기
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		FD_SET((*it)->sock, &rset);
	}

	timeval tv = { 0, 0 }; // 지금은 로직이 따로 없어서 NULL이 맞지만. 렌더링을 하고싶기때문에 {0, 0} 으로 간다.
	returnValueSelect = select(0, &rset, NULL, NULL, &tv);
	if (returnValueSelect == SOCKET_ERROR)
	{
		errorCodeSelect = WSAGetLastError();
		printf("select() error. %d\n", errorCodeSelect);
		Sleep(INFINITE);
	}

	// accept() 처리 -> 이번 프로젝트에서는 1프레임에 한 번만 하기로 결정.
	if (FD_ISSET(listenSocket, &rset))
	{
		AcceptClient(listenSocket);
	}

	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if (FD_ISSET((*it)->sock, &rset))
		{
			returnValueRecv = recv((*it)->sock, (*it)->recvBuf, RECV_BUFSIZE, 0);

			int offset = 0;
			while (offset < returnValueRecv)
			{
				int* msg = (int*)((*it)->recvBuf + offset);
				switch ((MsgType)*msg)
				{
					case MsgType::MOVESTAR:
						MsgMoveStar* ptr = (MsgMoveStar*)msg;

						// 이상한 좌표 요청은 이동 무시
						if (ptr->row < 0 || ptr->row >= SCREEN_ROW || ptr->col < 0 || ptr->col >= SCREEN_COL)
						{
							offset += sizeof(MsgMoveStar);
							break;
						}

						// 이동
						(*it)->row = ptr->row;
						(*it)->col = ptr->col;

						offset += sizeof(MsgMoveStar);
						break;
				}
			}
		}
	}
}

void AcceptClient(SOCKET listenSocket)
{
	SOCKADDR_IN acceptClientAddr = { 0, };
	int acceptClientAddrLen = sizeof(acceptClientAddr);

	SOCKET acceptClientSocket = accept(listenSocket, (sockaddr*)&acceptClientAddr, &acceptClientAddrLen);
	if (acceptClientSocket == INVALID_SOCKET)
	{
		errorCodeAccept = WSAGetLastError();
		if (errorCodeAccept == WSAEWOULDBLOCK)
		{
			// 수락할 연결이 없을때. 또는 무언가 지금 당장은 할 수 없을 때.
			// 지금은 딱히 할 게 없어보인다.
		}
		else
		{
			printf("accept() error. %d\n", errorCodeAccept);
			Sleep(INFINITE);
		}
	}

	// 세션정보 만들기
	Session* acceptClientSession = new Session;
	acceptClientSession->del = false;
	acceptClientSession->id = starId++;
	acceptClientSession->row = SCREEN_ROW / 2;
	acceptClientSession->col = SCREEN_COL / 2;
	acceptClientSession->sock = acceptClientSocket;
	InetNtop(AF_INET, &acceptClientAddr.sin_addr, acceptClientSession->ip, sizeof(acceptClientSession->ip) / sizeof(WCHAR));
	acceptClientSession->port = ntohs(acceptClientAddr.sin_port);

	sessionList.push_back(acceptClientSession);

	// 지금 접속한 클라에게 MsgAllocID 전송
	MsgAllocID msgAllocId;
	msgAllocId.type = (int)MsgType::ALLOCID;
	msgAllocId.id = acceptClientSession->id;
	returnValueSend = send(acceptClientSocket, (char*)&msgAllocId, sizeof(msgAllocId), 0);
	if (returnValueSend == SOCKET_ERROR)
	{
		// 중요한 접속 과정중에 send() 오류가 났다. 이는 오류 타입을 구분하지 않고 연결을 끊어야한다. 링버퍼가 있다면 다음 프레임에 보낼 수 있다.
		// 파일로 로그남기고 연결 끊기.
		errorCodeSend = WSAGetLastError();

		tm t;
		time_t tt = time(NULL);
		localtime_s(&t, &tt);
		char fileName[256];
		sprintf_s(fileName, "[SocketErrorLog]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

		#pragma warning(disable:6387)
		FILE* fp;
		fopen_s(&fp, fileName, "w");
		fwprintf(fp, L"send() error. errorcode=%d, FILE:%s, LINE:%d, acceptClientAddr:%s:%hu, \n", errorCodeSend, __FILEW__, __LINE__, acceptClientSession->ip, acceptClientSession->port);
		fwprintf(fp, L"MsgData -> type = MsgAllocID(%d)\n", msgAllocId.type);
		fwprintf(fp, L"MsgData -> id = %d\n", msgAllocId.id);
		fclose(fp);
		#pragma warning(default:6387)

		// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
		acceptClientSession->del = true;
		closesocket(acceptClientSocket);
		return;
	}

	// 지금 접속한 클라에게 자신을 포함한 모든 별 정보 주기
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		MsgCreateStar msgCreateStar;
		msgCreateStar.type = (int)MsgType::CREATESTAR;
		msgCreateStar.row = (*it)->row;
		msgCreateStar.col = (*it)->col;
		msgCreateStar.id = (*it)->id;

		returnValueSend = send(acceptClientSocket, (char*)&msgCreateStar, sizeof(msgCreateStar), 0);
		if (returnValueSend == SOCKET_ERROR)
		{
			// 중요한 접속 과정중에 send() 오류가 났다. 이는 오류 타입을 구분하지 않고 연결을 끊어야한다. 링버퍼가 있다면 다음 프레임에 보낼 수 있다.
			// 파일로 로그남기고 연결 끊기.
			errorCodeSend = WSAGetLastError();

			tm t;
			time_t tt = time(NULL);
			localtime_s(&t, &tt);
			char fileName[256];
			sprintf_s(fileName, "[SocketErrorLog]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

			#pragma warning(disable:6387)
			FILE* fp;
			fopen_s(&fp, fileName, "w");
			fwprintf(fp, L"send() error. [OtherClientInfo -> AcceptClient] errorcode=%d, FILE:%s, LINE:%d, AcceptacceptClientAddr:%s:%hu, \n", errorCodeSend, __FILEW__, __LINE__, acceptClientSession->ip, acceptClientSession->port);
			fwprintf(fp, L"MsgData -> type = CreateStar(%d)\n", msgCreateStar.type);
			fwprintf(fp, L"MsgData -> id = %d\n", msgCreateStar.id);
			fwprintf(fp, L"MsgData -> col = %d\n", msgCreateStar.col);
			fwprintf(fp, L"MsgData -> row = %d\n", msgCreateStar.row);
			fclose(fp);
			#pragma warning(default:6387)

			// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
			acceptClientSession->del = true;
			closesocket(acceptClientSocket);
			return;
		}
	}

	// 다른 클라들에게 지금 접속한 클라 별 정보 주기
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if ((*it)->id != acceptClientSession->id)
		{
			MsgCreateStar msgCreateStar;
			msgCreateStar.type = (int)MsgType::CREATESTAR;
			msgCreateStar.row = acceptClientSession->row;
			msgCreateStar.col = acceptClientSession->col;
			msgCreateStar.id = acceptClientSession->id;

			returnValueSend = send((*it)->sock, (char*)&msgCreateStar, sizeof(msgCreateStar), 0);
			if (returnValueSend == SOCKET_ERROR)
			{
				// 중요한 접속 과정중에 send() 오류가 났다. 이는 오류 타입을 구분하지 않고 연결을 끊어야한다. 링버퍼가 있다면 다음 프레임에 보낼 수 있다.
				// 파일로 로그남기고 연결 끊기.
				errorCodeSend = WSAGetLastError();

				tm t;
				time_t tt = time(NULL);
				localtime_s(&t, &tt);
				char fileName[256];
				sprintf_s(fileName, "[SocketErrorLog]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

				#pragma warning(disable:6387) // fp가 NULL일 수 있다. (파일 생성이 안되면 뭐 어쩌겠나? 죽어야지)
				FILE* fp;
				fopen_s(&fp, fileName, "w");
				fwprintf(fp, L"send() error. [AcceptClientInfo -> OtherClient] errorcode=%d, FILE:%s, LINE:%d, acceptClientAddr:%s:%hu, \n", errorCodeSend, __FILEW__, __LINE__, acceptClientSession->ip, acceptClientSession->port);
				fwprintf(fp, L"MsgData -> type = CreateStar(%d)\n", msgCreateStar.type);
				fwprintf(fp, L"MsgData -> id = %d\n", msgCreateStar.id);
				fwprintf(fp, L"MsgData -> col = %d\n", msgCreateStar.col);
				fwprintf(fp, L"MsgData -> row = %d\n", msgCreateStar.row);
				fclose(fp);
				#pragma warning(default:6387)

				// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
				acceptClientSession->del = true;
				closesocket(acceptClientSocket);
				return;
			}
		}
	}
}

void Render()
{
	if (renderSkip == false)
	{
		memset(screen, 32, sizeof(screen));
		for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
		{
			screen[(*it)->row][(*it)->col] = '*';
		}

		MoveCursor(0, 0);
		printf("Connect Client: %d / FPS: %d / Send: %d / Recv: %d\n", (int)(sessionList.size()), printfps, sendMsgCountThisFrame, recvMsgCountThisFrame);
		printf("AllocId: %d, CreateStar: %d, DeleteStar: %d, MoveStar(Send): %d, MoveStar(Recv): %d\n", msgCountAllocId, msgCountCreateStar, msgCountDeleteStar, msgCountMoveStarSend, msgCountMoveStarRecv);
		for (int i = 0; i < SCREEN_ROW; i++)
		{
			screen[i][SCREEN_COL] = '\0';
			printf("%s\n", screen[i]);
		}
	}
}

void FPS()
{
	// 프레임 카운트
	fps++;

	// FPS 저장
	clock_t elapsedFpsTick = clock() - startFpsTick;
	if (elapsedFpsTick >= 1000)
	{
		printfps = fps;
		fps = 0;
		startFpsTick += 1000;
	}

	// 프레임 제어
	clock_t elapsedFrameTick = clock() - startFrameTick;
	startFrameTick += 20;

	// 렌더스킵 (까먹지 않기 위해 만들었음)
	if (elapsedFrameTick >= 40)
		renderSkip = true;
	else
		renderSkip = false;

	clock_t remainFrameTick = 20 - elapsedFrameTick;
	if (remainFrameTick > 0)
	{
		Sleep(remainFrameTick);
	}
}