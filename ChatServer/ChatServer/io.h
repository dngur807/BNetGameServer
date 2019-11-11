#pragma once
#include <winsock2.h>
#include <MSWSock.h>

#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "mswsock.lib" )

#define MAXSENDPACKETSIZE	5120
#define MAXRECVPACKETSIZE	5120
#define RINGBUFSIZE			65536 //32768
#define MAXTRANSFUNC		256
#define HEADERSIZE			4

/*
	유저 구조체 : 이것은 접속된 유저당 하나씩 할당되는 것으로 유저에 모든 정보를 갖고 있는 구조체
	유저가 접속할때마다 할당 , 해제 하는것이 아니라, 서버가 시작될 때 미리 일정한 수만큼을 할당하는 방식 -> 메모리 단편화의 방지와 안정성이 이유입니다.

	수백 수만 바이트를 할당하는 것이 아니라 작은 구조체를 동적으로 할당하거나 해제하게 되면 메모리 단편화는 반드시 나타난다.

	메모리 할당과 해제에서 오버헤드는 성능에 안좋다.
	메모리 값이 싸다는 점을 생각하면 그것을 낭비해서 좀더 높은 성능을 내도록 하는 것이 낫다!!
	또한 동적인 메모리의 할당과 해제시에는 메모리의 타당성을 언제나 검사해야 하며 그러한 부분을 고려하지 않았을 경우 메모리 침범 에러 등이 발생할 위험이 있다.

	이러한 점때문에 메모리를 미리 할당받아 놓고 사용함으로서 예방할 수 있는 것이다.
*/

typedef struct
{
	OVERLAPPED	ovl;
	// 상태모드는 클라에서 넘어온 데이터가 읽기인지 쓰기인 구분하기 위한 플래그
	int			mode;
}EOVERLAPPED ,*LPEOVERLAPPED;

/* 유저 구조체 */
typedef struct
{
	// 확장된 오버랩
	EOVERLAPPED					eovRecvTcp,
								eovSendTcp;
	// 버퍼 운영에 필요한 것들이다.
	char						cRecvTcpRingBuf[RINGBUFSIZE + MAXRECVPACKETSIZE],	// 읽기에서 실제로 데이터가 저장될 버퍼이다.
								* cpRTBegin,	//  링 버퍼의 시작
								* cpRTEnd,		//	링 버퍼의 끝
								* cpRTMark, /* For GameBufEnqueue Start Position */

								cSendTcpRingBuf[RINGBUFSIZE + MAXSENDPACKETSIZE], // 쓰기에서 실제로 데이터가 저장될 버퍼입니다.
								* cpSTBegin,	// 링 버퍼의 시작
								* cpSTEnd;		//링 버퍼의 끝

	int							iSTRestCnt;// 클라이언트에 보내야 하는 데이터가 얼마나 남았는지
	SOCKET						sockTcp;		//실제 통신을 위한 소켓
	CRITICAL_SECTION			csSTcp; // 쓰기 버퍼에서의 동기화를 위한 임계 영역 객체
	sockaddr_in					remoteAddr;		//접속한 클라이언트의 주소를 나타낸다.
	int							index;			//각각의 클라이언트가 가지는 고유 인덱스이다.

	char					cID[32];		//구조체에 할당된 클라이언트의 ID
	int						idLen;			//아이디의 길이

	int						iProcess,		//현재 유저가 위치하고 있는 곳의 인덱스, 최초 접속시 Process와 Channel은 0번 인덱스에서 시작한다.
							iChannel,
							iRoom,

}SOCKETCONTEXT, *LPSOCKETCONTEXT;