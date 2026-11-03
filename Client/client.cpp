#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#include "Console.h"
#include "List.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Message Protocol //

#pragma pack(push, 1)
struct MsgAllocID
{
	int type;	// 0       메시지헤더, 데이터를 나눠야 하지만 지금은 그냥 붙여서 일괄로 보낸다.
	int id;
	int dummy1;	// 필요없으면 원래 안보내야하지만 학습용이니 메시지의 사이즈를 16바이트로 통일한다.
	int dummy2;
};

struct MsgCreateStar
{
	int type;	// 1
	int id;
	int col;	// x
	int row;	// y
};

struct MsgDeleteStar
{
	int type;	// 2
	int id;
	int dummy1;
	int dummy2;
};

struct MsgMoveStar
{
	int type;	// 3
	int id;		// 클라 -> 서버 내가 이동한다는 패킷에서는 id를 줄 필요가 없다. 서버는 나를 알고있다.
	int col;	// x
	int row;	// y
};
#pragma pack(pop)

enum class MsgType
{
	ALLOCID = 0,
	CREATESTAR = 1,
	DELETESTAR = 2,
	MOVESTAR = 3
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GameData //
struct Star
{
	int id;
	int row;
	int col;
};

Star* myStar;
List<Star*> starList;

#define SCREEN_ROW 23
#define SCREEN_COL 80

char screen[SCREEN_ROW][SCREEN_COL + 1];
//////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Network Data //

// serverAddr
WCHAR serverIp[16];
#define SERVER_PORT 3000

// 수신버퍼 
// [한 프레임에 16바이트 메시지 * 10개 수용가능]
#define RECV_BUFSIZE 160
char recvBuf[RECV_BUFSIZE];
//////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketErrorCode Data //

// 아직 에러코드나 함수 리턴값을 받지 않은 상태를 구분하기 위해 내 생일을 넣어놓음
#define NOT_YET 0x20020315

// WSAStartup() -> 성공(0), 실패(!=0), 실패시 WSAGetLastError() 사용
int returnValueWSAStartup = NOT_YET;
int errorCodeWSAStartUp = NOT_YET;

// socket() -> 성공(소켓fd), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
int errorCodeSocket = NOT_YET;

// InetPton() -> 성공(1), 잘못된IP문자열형식(0), 실패(-1), 실패시 WSAGetLastError() 사용
int returnValueInetPton = NOT_YET;
int errorCodeInetPton = NOT_YET;

// connect() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
// 논블로킹 SOCKET_ERROR -> WSAEWOULDBLOCK 체크
int returnValueConnect = NOT_YET;
int errorCodeConnect = NOT_YET;

// ioctlsocket() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
int returnValueIoctlSocket = NOT_YET;
int errorCodeIoctlSocket = NOT_YET;

// select() -> 성공(소켓개수), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
int returnValueSelect = NOT_YET;
int errorCodeSelect = NOT_YET;

// recv() -> 성공(읽은 바이트수), 실패(SOCKET_ERROR), 종료(0), 실패시 WSAGetLastError() 사용
// 논블로킹 SOCKET_ERROR -> WSAEWOULDBLOCK 체크
int returnValueRecv = NOT_YET;
int errorCodeRecv = NOT_YET;

// send() -> 성공(송신버퍼에 복제한 바이트수), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
// 논블로킹 SOCKET_ERROR -> WSAEWOULDBLOCK 체크
int returnValueSend = NOT_YET;
int errorCodeSend = NOT_YET;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	timeBeginPeriod(1);

	printf("접속할 서버 IP : ");
	wscanf_s(L"%s", serverIp, (unsigned int)(sizeof(serverIp) / sizeof(WCHAR)));

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
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		errorCodeSocket = WSAGetLastError();
		printf("socket() error. %d\n", errorCodeSocket);
		Sleep(INFINITE);
	}

	// SOCKADDR
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	returnValueInetPton = InetPton(AF_INET, serverIp, &serverAddr.sin_addr);
	if (returnValueInetPton != 1)
	{
		if (returnValueInetPton == 0)
		{
			printf("InetPton() %d 반환. IP문자열 형식이 잘못되었습니다.\n", returnValueInetPton);
			Sleep(INFINITE);
		}
		else if (returnValueInetPton == -1) // SOCKET_ERROR = -1
		{
			errorCodeInetPton = WSAGetLastError();
			printf("InetPton() error. %d\n", errorCodeInetPton);
			Sleep(INFINITE);
		}
	}

	// blocking connect()
	returnValueConnect = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (returnValueConnect == SOCKET_ERROR)
	{
		errorCodeConnect = WSAGetLastError();
		printf("connect() error %d\n", errorCodeConnect);
		Sleep(INFINITE);
	}

	// socket nonblocking change
	ULONG on = 1;
	returnValueIoctlSocket = ioctlsocket(sock, FIONBIO, &on);
	if (returnValueIoctlSocket == SOCKET_ERROR)
	{
		errorCodeIoctlSocket = WSAGetLastError();
		printf("ioctlsocket() error %d\n", errorCodeIoctlSocket);
		Sleep(INFINITE);
	}

	// 내 별 만들기
	myStar = new Star;
	memset(myStar, 0, sizeof(Star));
	starList.push_back(myStar);
	
	// WinAPI 콘솔 핸들 얻어서 초기화
	InitConsole();

	// fps 제어
	clock_t startFrameTick = clock();
	bool renderSkip = false;

	// fps 카운팅
	int printfps = 0;
	int fps = 0;
	clock_t startFpsTick = clock();
	
	// 메시지 제어
	bool sendMoveMsgThisFrame = false;

	// 메시지 카운팅
	int sendMsgCountThisFrame = 0;
	int recvMsgCountThisFrame = 0;

	while (1)
	{
		// Input
		{
			if (GetAsyncKeyState(VK_LEFT))
			{
				if ((myStar->col - 1) >= 0)
				{
					sendMoveMsgThisFrame = true;
					myStar->col--;
				}
			}
			if (GetAsyncKeyState(VK_RIGHT))
			{
				if ((myStar->col + 1) < SCREEN_COL)
				{
					sendMoveMsgThisFrame = true;
					myStar->col++;
				}
			}
			if (GetAsyncKeyState(VK_UP))
			{
				if ((myStar->row - 1) >= 0)
				{
					sendMoveMsgThisFrame = true;
					myStar->row--;
				}
			}
			if (GetAsyncKeyState(VK_DOWN))
			{
				if ((myStar->row + 1) < SCREEN_ROW)
				{
					sendMoveMsgThisFrame = true;
					myStar->row++;
				}
			}
		}

		// network
		{
			sendMsgCountThisFrame = 0;
			recvMsgCountThisFrame = 0;

			// L7단 링버퍼가 없기에 wset은 쓰지 않는다.
			// 링버퍼가 있다면 평소에 링버퍼에 메시지를 누적하다가 wset을 사용하여 가능할때 한 번에 보낸다.
			// 송신관점에서 링버퍼의 존재 이유: 미쳐 보내지 못한 데이터가 있을때 다음에 보내기 위해 보관용도가 있다.
			// 수신관점에서 링버퍼의 존재 이유: 메시지가 잘려왔을때 다음에 온 나머지를 합치기 위해 보관용도가 있다.
			// 
			// 1. 메시지가 잘려오지 않는다고 가정한다. 
			// 2. 뭉쳐오는 것은 가능하다고 가정한다.
			// 3. 송신버퍼가 꽉차지 않는다고 가정한다. 워낙 작은 메시지를 주고받기에 네트워크가 문제가 생기지 않는 한 그럴 가능성이 0에 가깝다.
			// 
			// 지금은 rset에 하나밖에 안넣으니 그냥 논블로킹 recv() 하는 것이 더 빠르지만 학습을 위해 select를 사용한다.

			fd_set rset;
			fd_set wset;

			FD_ZERO(&rset);
			FD_ZERO(&wset);
			FD_SET(sock, &rset);
			FD_SET(sock, &wset);

			// 클라의 입력을 받기 위해 프레임을 굴려야한다. 따라서 논블로킹 select
			timeval t = { 0, 0 };
			returnValueSelect = select(0, &rset, &wset, NULL, &t);
			if (returnValueSelect == SOCKET_ERROR)
			{
				errorCodeSelect = WSAGetLastError();
				printf("select() error. %d\n", errorCodeSelect);
				Sleep(INFINITE);
			}

			if (returnValueSelect > 0)
			{
				if (FD_ISSET(sock, &rset))
				{
					// recv()
					returnValueRecv = recv(sock, recvBuf, RECV_BUFSIZE, 0);
					if (returnValueRecv == SOCKET_ERROR)
					{
						errorCodeRecv = WSAGetLastError();
						if (errorCodeRecv == WSAEWOULDBLOCK)
						{
							// 지금 만드는 이 프로그램에서는 뭘 할 게 있을까?
						}
						else
						{
							printf("recv() error. %d\n", errorCodeRecv);
							Sleep(INFINITE);
						}
					}
					else if (returnValueRecv == 0)
					{
						printf("연결을 종료합니다.\n");
						break;
					}
					
					recvMsgCountThisFrame = returnValueRecv / 16;

					int offset = 0;
					while (offset < returnValueRecv)
					{
						int* msg = (int*)(recvBuf + offset);

						switch ((MsgType)*msg)
						{
							case MsgType::ALLOCID:
							{
								MsgAllocID* ptr = (MsgAllocID*)msg;
								myStar->id = ptr->id;

								offset += sizeof(MsgAllocID);
								break;
							}
							case MsgType::CREATESTAR:
							{
								bool starAlreadyExist = false;

								MsgCreateStar* ptr = (MsgCreateStar*)msg;

								// 이동보정 해야할까 고민이다. 서버쪽에서 주는 좌표정보가 이상할까?
								// 하지만 원장님 서버가 어떻게 만들어졌는지 모르고
								// 다른사람 서버도 어떻게 만들지 모르니 일단 해본다.
								if (ptr->row >= SCREEN_ROW)
									ptr->row = SCREEN_ROW - 1;
								else if (ptr->row < 0)
									ptr->row = 0;

								if (ptr->col >= SCREEN_COL)
									ptr->col = SCREEN_COL - 1;
								else if (ptr->col < 0)
									ptr->col = 0;

								for (List<Star*>::Iterator it = starList.begin(); it != starList.end(); ++it)
								{
									// 이미 존재하는 별에대한 건 위치만 바꾸기.
									// 내 별이 이미 존재함.
									if ((*it)->id == ptr->id)
									{
										starAlreadyExist = true;
										(*it)->row = ptr->row;
										(*it)->col = ptr->col;
										break;
									}
								}

								// 없는 별이면 만들기.
								if (starAlreadyExist == false)
								{
									Star* star = new Star;
									star->id = ptr->id;
									star->row = ptr->row;
									star->col = ptr->col;
									starList.push_back(star);
								}

								offset += sizeof(MsgCreateStar);
								break;
							}
							case MsgType::DELETESTAR:
							{
								MsgDeleteStar* ptr = (MsgDeleteStar*)msg;

								// 만약 내 별을 지워라? 연결을 끊을 것도 아니고 뜬금없이 지우라는 건 이상하다. 무시한다.
								if (ptr->id == myStar->id)
									break;

								// 별 찾아서 지우기
								for (List<Star*>::Iterator it = starList.begin(); it != starList.end(); ++it)
								{
									if ((*it)->id == ptr->id)
									{
										starList.erase(it);
										break;
									}
								}

								offset += sizeof(MsgDeleteStar);
								break;
							}
							case MsgType::MOVESTAR:
							{
								MsgMoveStar* ptr = (MsgMoveStar*)msg;

								// 이동보정 해야할까 고민이다. 서버쪽에서 주는 좌표정보가 이상할까?
								// 하지만 원장님 서버가 어떻게 만들어졌는지 모르고
								// 다른사람 서버도 어떻게 만들지 모르니 일단 해본다.
								if (ptr->row >= SCREEN_ROW)
									ptr->row = SCREEN_ROW - 1;
								else if (ptr->row < 0)
									ptr->row = 0;

								if (ptr->col >= SCREEN_COL)
									ptr->col = SCREEN_COL - 1;
								else if (ptr->col < 0)
									ptr->col = 0;

								for (List<Star*>::Iterator it = starList.begin(); it != starList.end(); ++it)
								{
									if ((*it)->id == ptr->id)
									{
										(*it)->row = ptr->row;
										(*it)->col = ptr->col;
										break;
									}
								}

								offset += sizeof(MsgMoveStar);
								break;
							}
						}
					}
				}
				if (FD_ISSET(sock, &wset))
				{
					if (sendMoveMsgThisFrame)
					{
						// 스택에서 메시지를 직접 만드는 이유는
						// 이번 프로그램에서 다루는 메시지가 16바이트로 굉장히 작고, 
						// 서버가 다루는 연결의 개수도 상당히 적다.
						// 따라서 내쪽 송신큐나 서버쪽 수신큐가 가득차는 상황은 사실상 존재할 수 없다.
						// 만약 그런 상황이 나온다면 그냥 네트워크가 맛이간 상황이기에 신경쓰지않는다.
						// send()가 당장 불가능하여 다음 프레임으로 넘어가는 상황이 거의 없다.
						// 그렇기에 따로 송신링버퍼에 저장하는 구조도 만들지 않았다.
						// 그래서 그냥 스택에서 만들어서 바로보낸다.
						MsgMoveStar msg;
						msg.type = 3;
						msg.id = myStar->id;
						msg.row = myStar->row;
						msg.col = myStar->col;

						// 링버퍼가 있다면 SendUnicast() 함수를 만들어 L7 송신 링버퍼에 복사해두고
						// 네트워크처리 쓰레드가 따로돌면서 직접 send()를 호출하는 그림이 나올 것 같다.
						returnValueSend = send(sock, (char*)&msg, sizeof(MsgMoveStar), 0);
						if (returnValueSend == SOCKET_ERROR)
						{
							errorCodeSend = WSAGetLastError();
							if (errorCodeSend == WSAEWOULDBLOCK)
							{
								// 이 프로그램에선 할 일이 없다.
							}
							else
							{
								printf("send() error. %d\n", errorCodeSend);
								Sleep(INFINITE);
							}
						}

						sendMsgCountThisFrame++;
						
						// 보내고 나서 초기화. 이번 프레임에 보내지 못한 것은 고려하지 않음.
						sendMoveMsgThisFrame = false;
					}
				}
			}


		}

		// Render
		{
			if (renderSkip == false)
			{
				memset(screen, 32, sizeof(screen));
				for (auto it = starList.begin(); it != starList.end(); ++it)
				{
					screen[(*it)->row][(*it)->col] = '*';
				}

				MoveCursor(0, 0);
				printf("Connect Client: %d / FPS: %d / Send: %d / Recv: %d\n",(int)(starList.size()), printfps, sendMsgCountThisFrame, recvMsgCountThisFrame);
				for (int i = 0; i < SCREEN_ROW; i++)
				{
					screen[i][SCREEN_COL] = '\0';
					printf("%s\n", screen[i]);
				}
			}
		}

		// 프레임 관련
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
			startFrameTick += 40;

			// 렌더스킵
			//if (elapsedFrameTick >= 80)
			//	renderSkip = true;
			//else
			//	renderSkip = false;

			clock_t remainFrameTick = 40 - elapsedFrameTick;
			if (remainFrameTick > 0)
			{
				Sleep(remainFrameTick);
			}
		}
	}

	closesocket(sock);
	WSACleanup();

	Sleep(INFINITE);

	return 0;
}