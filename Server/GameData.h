#pragma once

// 맵
#define SCREEN_ROW 23
#define SCREEN_COL 80

char screen[SCREEN_ROW][SCREEN_COL + 1];

// 접속한 별들에게 부여할 ID
int starId = 0;

// fps 제어
clock_t startFrameTick;
bool renderSkip = false;

// fps 카운팅
int printfps = 0;
int fps = 0;
clock_t startFpsTick;

// 메시지 카운팅
int sendMsgCountThisFrame = 0;
int recvMsgCountThisFrame = 0;

int msgCountAllocId = 0;
int msgCountCreateStar = 0;
int msgCountDeleteStar = 0;
int msgCountMoveStarSend = 0;
int msgCountMoveStarRecv = 0;