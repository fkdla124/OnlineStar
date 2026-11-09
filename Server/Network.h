#pragma once

#include <list>
#include "RingBuffer.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Network Data //

#define MAXSESSION 60

#define SERVER_PORT 3000

// 수신버퍼 
// [한 프레임에 16바이트 메시지 * 10개 수용가능]
#define RECV_BUFSIZE 160

struct Session
{
	bool del;	// 지연 삭제 마킹.

	int id;
	int row;
	int col;

	SOCKET sock;
	WCHAR ip[16];	// IPv4
	USHORT port;

	RingBuffer sendRingBuf;	// 송신 링버퍼. 지금 인원제한이 60명이라 기본값 1024바이트로 충분.
	RingBuffer recvRingBuf;	// 수신 링버퍼
};

std::list<Session*> sessionList; // STL list 사용해보기.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////