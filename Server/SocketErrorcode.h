#pragma once

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketErrorCode Data //

// 아직 에러코드나 함수 리턴값을 받지 않은 상태를 구분하기 위해 내 생일을 넣어놓음
#define NOT_YET 0x20020315

// WSAStartup() -> 성공(0), 실패(!=0), 실패시 WSAGetLastError() 사용
int returnValueWSAStartup = NOT_YET;
int errorCodeWSAStartUp = NOT_YET;

// socket() -> 성공(소켓fd), 실패(INVALID_SOCKET), 실패시 WSAGetLastError() 사용
int errorCodeSocket = NOT_YET;

// bind() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
int returnValueBind = NOT_YET;
int errorCodeBind = NOT_YET;

// listen() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
int returnValueListen = NOT_YET;
int errorCodeListen = NOT_YET;

// accept() -> 성공(소켓fd), 실패(INVALID_SOCKET), 실패시 WSAGetLastError() 사용
int errorCodeAccept = NOT_YET;

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

// setsockopt() -> 성공(0), 실패(SOCKET_ERROR), 실패시 WSAGetLastError() 사용
int returnValueSetSockOpt = NOT_YET;
int errorCodeSetSockOpt = NOT_YET;

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// My RingBuffer ErrorCheck //

int returnValuePush = NOT_YET;
int returnValuePop = NOT_YET;
int returnValuePeek = NOT_YET;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////