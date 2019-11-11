#pragma once
#include <winsock2.h>
#include <MSWSock.h>

#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "mswsock.lib" )

/*
서버 정보 구조체
서버 프로그램 전체에서 사용하는 서버 정보를 위한 전역 구조체 입니다.
이 구조체의 변수에 각각의 타입별로 일정한 양만큼씩 할당된 메모리 포인터를 저장함으로 유저 구조체나 채널, 룸 등의 객체로의 직접 접근이 가능합니다.
첫 번째 변수인 sockListener 는 유저의 접속을 기다리기 위한 소켓이다.
*/
typedef struct
{
	SOCKET sockListener;//유저의 접속을 기다리는 리슨 소켓이다.
	HANDLE hIocpWorkTcp,//IOCP의 핸들로서 보통의 읽기 쓰기 용도
		hIocpAcept;//IOCP의 핸들로서 유저의 접속을 받아들일 때 사용한다.

	SOCKETCONTET* sc;	// 유저 구조체에 대한 시작 포인터이다. 이것을 이용하면 전체 유저에 대한 정보를 탐색할 수 있습니다.

	OBJECTNODE* pn,		
		* rn;

	// 각자 시작 포인터로 이것을 이용해서 직접 탐색이 가능
	CProcess* ps;	//프로세스의 시작점
	CChannel* ch;	//채널의 시작점
	CRoom* rm;	//룸의 시작점
	CDataBase* db;	//데이터베이스의 시작점

	// 셋팅 파일에서 설정 정보
	int						iMaxUserNum,
		iCurUserNum,
		iInWorkerTNum,
		iDataBaseTNum,
		iPortNum,

		iMaxProcess, /* Purely Process Number */
		iMaxChannelInProcess,
		iMaxChannel, /* Purely Channel Number */
		iMaxRoomInChannel,
		iMaxRoom, /* Purely Room Number */
		iMaxUserInRoom;
}SERVERCONTEXT, * LPSERVERCONTEXT;