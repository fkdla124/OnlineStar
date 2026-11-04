#pragma once

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