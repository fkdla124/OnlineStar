#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")


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

SOCKET StartServer(linger _linger, ULONG nonBlocking);
void StopServer(SOCKET listenSocket);

void SendUniCast(Session* session, char* msg, int msglen);
void SendBroadCast(Session* excludedSession, char* msg, int msglen);

void FlushSendRingBuffer(Session* session);

void SweepMarkedSession();

int main()
{
	timeBeginPeriod(1);

	// Start Server
	linger _linger = { 1, 0 };
	ULONG nonBlocking = 1;
	SOCKET listenSocket = StartServer(_linger, nonBlocking);

	// Console Init()
	Init();
	SetConsoleTitle(L"OnlineStar Server");

	// Timer Init()
	startFpsTick = clock();
	startFrameTick = clock();

	// Main Loop
	while (1)
	{
		// Network()
		Network(listenSocket);

		// Render() -> 서버의 상태를 확인하기 위한 표시. (FPS, 초당 메시지처리량 등..) 지금은 맵도 렌더링 함. 보기위해
		Render();

		// FPS
		FPS();
	}

	StopServer(listenSocket);

	return 0;
}

void Init()
{
	// WinAPI 콘솔 핸들 얻어서 초기화
	InitConsole();
}

void Network(SOCKET listenSocket)
{
	recvMsgCountThisFrame = 0;
	sendMsgCountThisFrame = 0;

	fd_set rset;
	fd_set wset;
	FD_ZERO(&rset);
	FD_ZERO(&wset);

	FD_SET(listenSocket, &rset);
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		FD_SET((*it)->sock, &rset);
		
		if ((*it)->sendRingBuf.empty() == false)
		{
			FD_SET((*it)->sock, &wset);
		}
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
		AcceptClient(listenSocket);

	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if (FD_ISSET((*it)->sock, &rset))
		{
			char recvBuf[RECV_BUFSIZE];
			returnValueRecv = recv((*it)->sock, recvBuf, RECV_BUFSIZE, 0);
			if (returnValueRecv == SOCKET_ERROR)
			{
				errorCodeRecv = WSAGetLastError();

				if (errorCodeRecv == WSAEWOULDBLOCK)
				{
					continue;
					// 지금은 뭘 할 수 있는 게 없어보인다.
				}
				else if (errorCodeRecv == WSAECONNRESET) // 클라 RST
				{
					(*it)->del = true;
					continue;
				}
			}
			else if (returnValueRecv == 0)
			{
				// 클라도 서버도 링거때문에 RST를 쏘게 만들었는데 이 경우가 있을까 모르겠다. 
				// 아마도 shutdown()? 이건 링거 {1, 0} 걸어도 FIN이 오는 걸로 기억한다.
				(*it)->del = true;
				continue;
			}

			// 우선 로컬버퍼 -> 링버퍼 순으로 진행해보기
			returnValuePush = (*it)->recvRingBuf.push(recvBuf, returnValueRecv);
			if (returnValuePush != returnValueRecv)
			{
				printf("recvRingBuf.push() Error\n");
				Sleep(INFINITE);
			}
			
			// 정석적인 처리는
			// 1. 메시지 헤더크기만큼 잘 왔는지 사이즈체크
			// 2. 헤더가 왔다면 헤더에서 len 뽑아서 페이로드까지도 잘 왔는지 체크.
			// 3. 헤더 + 페이로드 크기가 정상적이라면 처리.
			// 4. 1.로 돌아가서 다음 메시지 처리.

			while (1)
			{
				// 우리게임은 16바이트 메시지고정 + 헤더 개념이 따로 없음.
				// 그래서 16바이트냐만 체크하기.
				// 만약 잘려오면 다음프레임까지 남아있다가 합쳐질 예정.
				if ((*it)->recvRingBuf.size() < 16)
					break;

				char msg[16];
				(*it)->recvRingBuf.pop(msg, 16);

				switch ((MsgType)(*(int*)msg))
				{
					case MsgType::MOVESTAR:
					{
						recvMsgCountThisFrame++;
						msgCountMoveStarRecv++;

						MsgMoveStar* ptr = (MsgMoveStar*)msg;

						// 본인 이동이 아니면 무시
						if (ptr->id != (*it)->id)
							break;

						// 이상한 좌표 요청은 이동 무시
						if (ptr->row < 0 || ptr->row >= SCREEN_ROW || ptr->col < 0 || ptr->col >= SCREEN_COL)
							break;

						// 이동
						(*it)->row = ptr->row;
						(*it)->col = ptr->col;

						// 내 이동을 브로드캐스트
						MsgMoveStar msgMoveStar;
						msgMoveStar.type = (int)MsgType::MOVESTAR;
						msgMoveStar.id = (*it)->id;
						msgMoveStar.row = (*it)->row;
						msgMoveStar.col = (*it)->col;

						SendBroadCast(*it, (char*)&msgMoveStar, sizeof(msgMoveStar));
						sendMsgCountThisFrame += sessionList.size() - 1;
						msgCountMoveStarSend += sessionList.size() - 1;

						break;
					}
					default:
						// 이상한 타입의 메시지가 왔을 때의 처리가 필요함.
						// 만약 우리 클라와 서버가 사용하는 메시지 프로토콜로 이동메시지외에 메시지가 왔다면 이는 클라 조작이다.
						// 그런 경우에는 그 세션을 추적해서 행동들을 로깅해야한다. 아예 GM이 감시하는 것도 방법이다.
						(*it)->del = true;
						break;
				}
			}
		}
		
		if (FD_ISSET((*it)->sock, &wset))
		{
			FlushSendRingBuffer(*it);
		}
	}

	// 삭제마킹된애들 실제 삭제
	SweepMarkedSession();
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

	if (sessionList.size() >= MAXSESSION)
	{
		closesocket(acceptClientSocket);
		return;
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

	SendUniCast(acceptClientSession, (char*)&msgAllocId, sizeof(msgAllocId));
	sendMsgCountThisFrame++;
	msgCountAllocId++;

	// 지금 접속한 클라에게 자신을 포함한 모든 별 정보 주기
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		MsgCreateStar msgCreateStar;
		msgCreateStar.type = (int)MsgType::CREATESTAR;
		msgCreateStar.row = (*it)->row;
		msgCreateStar.col = (*it)->col;
		msgCreateStar.id = (*it)->id;

		SendUniCast(acceptClientSession, (char*)&msgCreateStar, sizeof(msgCreateStar));
		sendMsgCountThisFrame++;
		msgCountCreateStar++;
	}

	// 다른 클라들에게 지금 접속한 클라 별 정보 주기
	MsgCreateStar msgCreateStar;
	msgCreateStar.type = (int)MsgType::CREATESTAR;
	msgCreateStar.row = acceptClientSession->row;
	msgCreateStar.col = acceptClientSession->col;
	msgCreateStar.id = acceptClientSession->id;

	SendBroadCast(acceptClientSession, (char*)&msgCreateStar, sizeof(msgCreateStar));
	sendMsgCountThisFrame += sessionList.size() - 1;
	msgCountCreateStar += sessionList.size() - 1;
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
		printf("Connect Client: %d / FPS: %d / Send: %3d / Recv: %3d\n", (int)(sessionList.size()), printfps, sendMsgCountThisFrame, recvMsgCountThisFrame);
		printf("AllocId(Send): %d, CreateStar(Send): %d, DeleteStar(Send): %d, MoveStar(Send): %d, MoveStar(Recv): %d\n", msgCountAllocId, msgCountCreateStar, msgCountDeleteStar, msgCountMoveStarSend, msgCountMoveStarRecv);
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
	startFrameTick += 5;

	// 렌더스킵 (까먹지 않기 위해 만들었음)
	if (elapsedFrameTick >= 10)
		renderSkip = true;
	else
		renderSkip = false;

	clock_t remainFrameTick = 5 - elapsedFrameTick;
	if (remainFrameTick > 0)
	{
		Sleep(remainFrameTick);
	}
}

SOCKET StartServer(linger _linger, ULONG nonBlocking)
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

	// linger set
	returnValueSetSockOpt = setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (char*)&_linger, sizeof(_linger));
	if (returnValueSetSockOpt == SOCKET_ERROR)
	{
		errorCodeSetSockOpt = WSAGetLastError();
		printf("setsockopt() LINGER{0, 0} setting error. %d\n", errorCodeSetSockOpt);
		Sleep(INFINITE);
	}

	//DWORD tcpNodelay = true;
	//returnValueSetSockOpt = setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&tcpNodelay, sizeof(tcpNodelay));
	//if (returnValueSetSockOpt == SOCKET_ERROR)
	//{
	//	errorCodeSetSockOpt = WSAGetLastError();
	//	printf("setsockopt() TCP_NODELAY setting error. %d\n", errorCodeSetSockOpt);
	//	Sleep(INFINITE);
	//}

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
	returnValueIoctlSocket = ioctlsocket(listenSocket, FIONBIO, &nonBlocking);
	if (returnValueIoctlSocket == SOCKET_ERROR)
	{
		errorCodeIoctlSocket = WSAGetLastError();
		printf("ioctlsocket() error. %d\n", errorCodeIoctlSocket);
		Sleep(INFINITE);
	}

	return listenSocket;
}

void StopServer(SOCKET listenSocket)
{
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		closesocket((*it)->sock);
	}
	closesocket(listenSocket);
	WSACleanup();

	sessionList.clear();
}

void SendBroadCast(Session* excludedSession, char* msg, int msglen)
{
	for (auto it = sessionList.begin(); it != sessionList.end(); ++it)
	{
		if ((*it)->id != excludedSession->id)
		{
			SendUniCast(*it, msg, msglen);
		}
	}
}

void SendUniCast(Session* session, char* msg, int msglen)
{
	returnValuePush = session->sendRingBuf.push(msg, msglen);
	if (returnValuePush != msglen)
	{
		// 송신링버퍼가 꽉찼음.	
		// 파일로 로그 남기고 연결 끊기.
		tm t;
		time_t tt = time(NULL);
		localtime_s(&t, &tt);
		char fileName[256];
		sprintf_s(fileName, "[SendRIngBuffer Full Error]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

#pragma warning(disable:6387) // fp가 NULL일 수 있다.
		FILE* fp;
		fopen_s(&fp, fileName, "wb");

		// UTF-16 LE BOM
		unsigned char bom[] = { 0xFF, 0xFE };
		fwrite(bom, 1, 2, fp);

		fwprintf(fp, L"RingBuffer Full error. FILE: %s, LINE: %d, recvSessionID: %d \n", __FILEW__, __LINE__, session->id);
		fwprintf(fp, L"RingBuffer Capacity: %d\n", session->sendRingBuf.capacity());
		fwprintf(fp, L"RingBuffer Size: %d\n", session->sendRingBuf.size());
		fwprintf(fp, L"Push MsgLen: %d\n", msglen);
		fwprintf(fp, L"returnValuePush: %d\n", returnValuePush);
		fwprintf(fp, L"MsgType = %d\n", *((int*)msg));
		fclose(fp);
#pragma warning(default:6387)

		// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
		session->del = true;
	}
}

void FlushSendRingBuffer(Session* session)
{
	// 링버퍼 안정성 테스트용 임시버퍼. 
	// 내 링버퍼가 문제없음이 확실시되면 삭제예정
	char buf[1024];

	returnValuePeek = session->sendRingBuf.peek(buf, 1024);
	if (returnValuePeek != session->sendRingBuf.size())
	{
		printf("sendRingBuf.peek() Error\n");
		Sleep(INFINITE);
	}

	returnValueSend = send(session->sock, buf, returnValuePeek, 0);
	if (returnValueSend == SOCKET_ERROR)
	{
		errorCodeSend = WSAGetLastError();

		// 실패시 파일로 로그 남기기
		tm t;
		time_t tt = time(NULL);
		localtime_s(&t, &tt);
		char fileName[256];
		sprintf_s(fileName, "[SocketErrorLog]%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

#pragma warning(disable:6387) // fp가 NULL일 수 있다.
		FILE* fp;
		fopen_s(&fp, fileName, "wb");

		// UTF-16 LE BOM
		unsigned char bom[] = { 0xFF, 0xFE };
		fwrite(bom, 1, 2, fp);

		fwprintf(fp, L"send() error. errorcode: %d, FILE: %s, LINE: %d, recvSessionID: %d \n", errorCodeSend, __FILEW__, __LINE__, session->id);
		fclose(fp);
#pragma warning(default:6387)

		// 지연삭제 마킹 및 연결끊기. SO_LINGER {0, 0} RST 전송
		session->del = true;
	}

	session->sendRingBuf.MoveFront(returnValueSend);
}

void SweepMarkedSession()
{
	for (auto it = sessionList.begin(); it != sessionList.end();)
	{
		if ((*it)->del == true)
		{
			MsgDeleteStar msgDeleteStar;
			msgDeleteStar.type = (int)MsgType::DELETESTAR;
			msgDeleteStar.id = (*it)->id;

			SendBroadCast(*it, (char*)&msgDeleteStar, sizeof(msgDeleteStar));
			msgCountDeleteStar += sessionList.size() - 1;

			closesocket((*it)->sock);
			it = sessionList.erase(it);
		}
		else
			++it;
	}
}