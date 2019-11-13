#include "framework.h"
#include "server.h"


int InitIoLayer()
{
	WSADATA wd = { 0 };
	SOCKADDR_IN addr;
	HANDLE hThread;
	int dummy, iN;


	// 소켓 사용 IOCP 와 윈도우의 특정적인 확장 소켓함수를 사용하기 위해 2.2 버전으로 초기화
	dummy = WSAStartup(MAKEWORD(2, 2), &wd);
	if (dummy != 0)
		return 0;
	// 클라이언트의 접속을 받기 위한 리슨 소켓 생성
	Server.sockListener = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (Server.sockListener == INVALID_SOCKET)
		return 0;

	// 리슨 포트와 바인딩
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((short)Server.iPortNum);

	dummy = bind(Server.sockListener, (sockaddr*)&addr, sizeof(addr));
	if (dummy == SOCKET_ERROR)
		return 0;

	// 접속 대기
	dummy = listen(Server.sockListener, 512);
	if (dummy == SOCKET_ERROR) return 0;

	/*
	IOCP를 사용하는 데에는 크게 두 가지의 단계를 필요로 합니다.
	1. IOCP의 핸들을 생성하는 것이다. 인자에 INVLID_HANDLE_VALUE , NULL을 넘겨줘서 새로운 IOCP 핸들을 얻을 수 있다.
	2. 소켓 객체와 IOCP와 연결하는 단계이다. 이런 과정을 거침으로서 그 소켓에서 일어난 이벤트에 대하여 IOCP에서 이벤트가 발생할 수 있다.
	*/

	// 클라이언트 접속을 받기 위한 IOCP 생성
	Server.hIocpAcept = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Server.hIocpWorkTcp == NULL)
		return 0;


	// 클라이언트와 실제 통신을 위한 IOCP 생성
	Server.hIocpWorkTcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Server.hIocpWorkTcp == NULL)
		return 0;

	// 접속을 받기 위한 IOCP 와 리슨 소켓을 연결
	CreateIoCompletionPort((HANDLE)Server.sockListener, Server.hIocpAcept, 0, 0);

	// 설정된 최대 유저 수만큼 소켓 컨텍스트 메모리 할당
	Server.sc = new SOCKETCONTEXT[Server.iMaxUserNum];
	if (Server.sc == NULL)
		return 0;

	// 생성된 소켓 컨텍스트 초기화 -> 유저 구조체를 초기화
	if (!InitSocketContext(Server.iMaxUserNum)) return 0;

	// 접속을 받기위한 접속 쓰레드 생성
	hThread = (HANDLE)_beginthreadex(NULL, 0, AcceptTProc, NULL, 0, (unsigned int*)&dummy);

	if (hThread == NULL)
		return 0;

	// IOCP 에서 사용하며 커널이 풀링을 할 스레드.
	// 클라이언트와의 통신을 위한 작업쓰레드 생성
	for (iN = 0; iN < Server.iInWorkerTNum; iN++)	//사용자와의 데이터 통신을 위한 쓰레드는 WORKERTHREAD 개수 만큼을 생성한다.
	{
		hThread = (HANDLE)_beginthreadex(NULL, 0, InWorkerTcpTProc, (void*)iN, 0, (unsigned int*)&dummy);
		if (hThread == NULL) return 0;
	}

	return 1;
}


// 유저 구조체의 초기화
int InitSocketContext(int maxUser)
{
	int				dummy;
	DWORD			dwReceived;
	LPSOCKETCONTEXT	sc = Server.sc;


	
	// 소켓 컨텍스트 구조체 전체를 0으로 초기화 한다.
	ZeroMemory(sc, sizeof(SOCKETCONTEXT) * maxUser);

	for (int iN = 0; iN < maxUser; ++iN)
	{
		// 소켓 컨텐스트 별로 인덱스 부여
		// 인덱스 초기화 순차적으로 주어지는 그 값이 바로 서버 내에서 유저를 구분하는데 사용되는 중요한 값이다.
		sc[iN].index = iN;

		// 소켓 컨텍스트에 할당된 버퍼 초기화
		sc[iN].iSTRestCnt = 0;
		sc[iN].eovSendTcp.mode = SENDEOVTCP;// 오버랩 구조체 쓰기
		sc[iN].eovRecvTcp.mode = RECVEOVTCP;// 읽기 설저

		// 링버퍼에 해당 시작 점과 끝점을 초기화
		sc[iN].cpSTBegin = sc[iN].cSendTcpRingBuf;
		sc[iN].cpRTMark = sc[iN].cpRTBegin = sc[iN].cpRTEnd = sc[iN].cRecvTcpRingBuf;



		// 클라이언트와의 통신을 위한 소켓 생성
		sc[iN].sockTcp = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (sc[iN].sockTcp == INVALID_SOCKET)
			return 0;

		// 클라이언트 당 하나씩 붙게 되는 소켓을 생성한 후에 그것들을 AcceptEX로 셋팅
		// 생성된 소켓을 대기시킴
		dummy = AcceptEx(Server.sockListener, sc[iN].sockTcp, sc[iN].cpRTEnd,
			MAXRECVPACKETSIZE, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			&dwReceived, (OVERLAPPED*)&sc[iN].eovRecvTcp);

		if (dummy == FALSE && GetLastError() != ERROR_IO_PENDING)
			return 0;

		// 쓰기 작업에서 필요한 임계영역
		// SendBuffer를 위한 임계영역을 생성합니다.
		InitializeCriticalSection(&sc[iN].csSTcp);

		/*For Game Processing*/
		// 운영상에서 사용되는
		// 기본 프로세스 , 채널, 룸 셋팅
		sc[iN].iProcess = DEFAULTPROCESS;
		sc[iN].iChannel = DEFAULTCHANNEL;
		sc[iN].iRoom = NOTLINKED;

	}
}

// 유저 구조체는 어떤 유저가 A라는 유저 구조체를 사용하다가 종료하면 또 다른
// 유저가 A라는 유저 구조체를 사용할 수 있는 것이므로 누군가 종료하였을 때는
// 똑같은 방법으로 초기화 해주어야 한다. 그 부분이 ReInitSocketContext;
int ReInitSocketContext(LPSOCKETCONTEXT lpSockContext)
{
}


/*
접속허가 쓰레드

테스트 서버에서 IO 계층의 핵심으로 클라이언트와 반응하는 모든 것을 작동할 수 있는 시작점입니다.
핵심이라는 것이 거창하게 느껴질 수도 있지만 상당히 간단한 형태로 보면 쉽게 이해할 수 있는 구조라는 것을 느낄 수 있다.
*/

unsigned int __stdcall AcceptTProc(void* pParam)
{
	DWORD					dummy, dwTransferred;
	LPEOVERLAPPED			lpEov;
	LPSOCKETCONTEXT			lpSockContext;
	BOOL					bResult;
	sockaddr_in				* pLocal, * pRemote;
	int						localLen, remoteLen;

	// while로 이루어진 무한 루프문을 돌게 되는데
	// 이때 어떠한 정지된 시간이 없다면 시스템은 현저하게 무리가 갈 것이다.
	// 정지된 시간을 만드는 것이 GetQueuedCompletionStatus 이다.

	// 이 API는 IOCP로 부터 입력 이나 출력 의 완료에 따른 이벤트가 없다면 그러한 신호를 기다리면서 블록킹되는 함수 이다.
	// 가지고 갈 수 있는 데이터가 있을 경우에만 쓰레드가 사용된다는 것을 보장받을 수 있다.
	while (1)
	{
		
		// 첫번째 CP 인자와 마지막의 블록될 시간을 지정하는 인자 
		// 그외의 인자는 얻는 (Get)인자이다.
		// 이것은 스레드가 블록되어 있을 때 발생된 신호가 어떤 상태에서 발생한 것인가를 넘겨주는 것이다.
		// 이렇게 넘겨주는 것은 첫번째로 정수의 포인터형인 전송된 바이트 수이다. 
		// 첫번째는 전송된 바이트 수이다. 이것은 읽기 (WSARecv) 에 의하여 발생한 것이라면 읽어들인 바이트 수, 쓰기 (WSASend) 에 의하여 발생한 것이면 전송한 바이트 수를 가리킨다.
		// 두번째 더블 워크 포인터 형으로 CP Key라는 것이다. 앞에 CreateIOCompletionPort를 이용하여 소켓과 IOCP를 연결할 때 바로 Completion Key라는 것을 전달한다.

		// 그 후 어떤 입출력이 발생하였을 때 이벤트에 의해 GetQueued 가 블록 해제 되었을 때는 그 Completion Key로 앞에서의 연결 과정에 전달하였을 때 넘겼던 값이 전달된다.
		// 따라서 이것을 하나의 구분값으로서 사용할 수 있다. 사실 Accept 쓰레드에서는 이 갓이 무의미 하다 전달될 값이 존재하지 않기 때문입니다.

		// 세번째 변수는 오버 랩 구조체의 포인터 형이다. 이것은 바로 입출력에 사용되었던 오버랩 구조체의 주소를 넘겨주는 부분이다.
		// 모든 것이 비동기로 작동하게 되므로 어떠한 작업이 일어날 때는 항상 오버랩 구조체를 사용하며 그에 따라 주소가 전달됩니다.
		bResult = GetQueuedCompletionStatus(Server.hIocpAcept, &dwTransferred, (LPDWORD)&dummy, (LPOVERLAPPED*)&lpEov, INFINITE);


		// 이벤트 발생! 거의 접속 이벤트 일 것이다.
		// 앞서 설명한 AcceptEx는 클라이언트가 접속한 후에 첫번째 데이터 블록이 전송되어야만 서버에서 접속을 인식한다고 하였다.
		// 중요한 것은 이렇게 첫번째 블록이 넘어오는 것 또한 네트워크 전송이므로 AcceptEx를 호출할 당시에도 오버랩 구조체를 사용한다는 것이다.

		// 따라서 GetQueue 에서 얻는 것은 첫번째 읽어들인 첫번째 데이터 블록의 양, 두번째 CP Key  ,세번째 AcceptEx에 인자로 주었던 그 확장 오버랩 구조체 주소이다.

		// 그 확장된 오버랩 구조체의 관계가 모두 하나의 유저 구조체안에 속하는 것들로 이루어진다. AcceptEx를 호출할 때 모두 같은 유저 구조체 안의 변수들로만 사용되었기 때문입니다.
		// 이점은 현재 접속한 클라가 어떠한 유저 구조체를 할당받았는지를 알 수 있는 근거가 됩니다.

		// 이벤트가 발생하여 GetQueue가 블록해제된 시점에서 접속한 클라는 자기와 통신을 위한 하나의 소켓과 또한 확장된 오버랩 구조체를 이미 할당받은 시점
		// 이기 때문에 또한 그들이 같은 유저 구조체 안에 포함되어 있기 때문에 그 확장된 오버랩 구조체의 주소만을 알게 된다면 접속한 클라가 어떠한 유저 구조체를 사용하는지 
		// 알 수 있다. 따라서
		// lpSockContext = (LPSOCKETCONTEXT)lpEov;

		// 에러가 없을 경우
		if (lpEov != NULL && dwTransferred != 0)
		{
			// --- increase current user number ---
			Server.iCurUserNum++;

			// 이벤트가 발생한 소켓 컨텍스트를 얻음
			// --- get socketcontext ---
			lpSockContext = (LPSOCKETCONTEXT)lpEov;


			// 상대방 주소를 알아내고 클라에 할당된 유저 구조체의 멤버에 복사해 넣는 부분
			// 접속받은 곳의 주소를 얻어 저장
			// --- connect realize ----
			GetAcceptExSockaddrs(lpSockContext->cpRTEnd, MAXRECVPACKETSIZE, sizeof(sockaddr_in) + 16,
				sizeof(sockaddr_in) + 16, (sockaddr**)&pLocal, &localLen, (sockaddr**)&pRemote, &remoteLen);

			// --- store remoteAddr ---
			CopyMemory(&lpSockContext->remoteAddr, pRemote, sizeof(sockaddr_in));

			// 읽기 링버퍼 운영을 위하여 아주 중요한 부분을 차지하고 있다.
			// 읽기 버퍼 위치 조정
			// --- setting end pointer ---
			RecvTcpBufEnqueue(lpSockContext, dwTransferred);


			// Accept 쓰레드의 일을 모두 마쳤으므로 Worker 쓰레드로 접속 된
			// 클라 유저 구조체에 할당된 소켓을 클라 Work에 연결하는 작업을 한다.


			// 접속된 클라이언트와의 통신을 위해
			// 작업용 IOCP와 연결
			// -- tcp socket association ---
			CreateIoCompletionPort((HANDLE)lpSockContext->sockTcp, Server.hIocpWorkTcp, (DWORD)lpSockContext, 0);

			// 비동기 소켓이다.
			// 블록킹 소켓에서는 recv에서 어떠한 크기일지라도 반드시
			// 읽은 후에 블록이 해제되는 반면에 비동기 소켓은 일단 무조건 리턴을 하며 후에 어떠한 이벤트 발생하였을 때 처리하는 방식
			// 다음 번 읽기를 위해 비동기 읽기를 시도
			// tcp socket postrecv
			PostTcpRecv(lpSockContext);

			// 날아온 데이터를 해석하도록 요청을 행하여 주는 부분 큐에 쌓고 처리는 프로세스에서 진행한다.
			// 패킷 처리를 요청
			GameBufEnqueueTcp(lpSockContext);
		}
	}
}


/*
작업 쓰레드
	작업 쓰레드가 실제로 유저와의 응답을 처리하는 부분이다.
	유저가 보낸 데이터를 버퍼에 저장하고 그것을 처리하도록 요구하며
	또한 유저에게 보내야 할 데이터를 보내는 과정을 그 쓰레드에서 하는 것이다.

	실제로는 쓰레드들이 되며 그것을 풀링하는 작업은 커널에서 한다.
	적정 수만큼 쓰레드를 미리 생성하여 놓은 후 보통의 경우 대기 했다 작업 요청을 받으면
	그 중 하나가 활성화되어 그 작업을 처리하는 것이다.

	이미 모든 스레드가 활성화되 있으면 기다리다 사용가능해지면 동작
*/
unsigned int __stdcall InWorkerTcpTProc(void* pParam)
{
	DWORD				dwTransferred;
	LPEOVERLAPPED		lpEov;
	LPSOCKETCONTEXT		lpSockContext;
	BOOL				bResult;

	while (1)
	{
		// 유저로부터 데이터를 받거나 서버에서 유저에게 
		// 보낸 데이터의 완료가 일어날 때 까지 대기
		// 넘어온 오버랩 구조체의 포인터는 어떠한 작업(WSASend, WSARecv)을 요청할 때 인자로 주어지는 것이다. 이런 요청은 각각의 유저에 대하여 
		// 인자로 주어지는 것이다. 이러한 요청은 각각의 유저에 대하여 하게 되므로 앞에서 본 것과 같이 유저 구조체 안에 멤버로 포함되어 있는 것이다.
		// 실제로 사용한 것은 오버랩 구조체가 어떠한 작업의 결과인지를 나타내기 위하여 상태 값을 가지고 있는 형태로 사용
		// 결과는 모두 작업 쓰레드로 넘어오기 떄문에 발생한 이벤트가 어떤 것에 대한 결과인가를 구분하기 위하여 사용된다. 
		bResult = GetQueuedCompletionStatus(Server.hIocpWorkTcp, &dwTransferred, (LPDWORD)&lpSockContext, (LPOVERLAPPED*)&lpEov, INFINITE);

		if (lpEov != NULL)
		{
			// 만약 넘겨진 오버랩 구조체가 NULL 이 아닌데 처리된 바이트 수가 0이면
			// 서버나 클라에 의해 연결이 끊긴 상태 그때 EnqueueClose 라는 함수를 이용해서 처리
			
			// 접속이 끊김
			if (dwTransferred == 0)
			{
				// LOGOUT
				EnqueueClose(lpSockContext);
			}
		}
		else
		{
			// 읽기용의 오버랩 구조체에서 신호
			if (lpEov->mode == RECVEOVTCP)
			{
				// 그 데이터를 큐에 넣는 작업
				// 읽기 버퍼위치 조정
				RecvTcpBufEnqueue(lpSockContext, dwTransferred);

				// 데이터를 해석하도록 요청
				// 패킷 처리를 요청
				GameBufEnqueueTcp(lpSockContext);

				// 다시 데이터를 받을 수 있도록
				// 다음번 읽기를 위해 비동기 읽기를 시도
				PostTcpRecv(lpSockContext);
			}
			// 쓰기용의 오버랩 구조체에서 신호
			else if (lpEov->mode == SENDEOVTCP)
			{
				// TCP 전송 모습은 전송의 안정성이나 순차성을 보장한다.
				// 이에 반해 하나의 패킷 단위라는 것을 보장하지 않는다.
				// ex) 100 바이트를 보내기 위하여 WSASend(100)이라 한다고 하더라고 이것은
				// 한번에 100바이트가 모두 전송된 결과로 나타날 수 있는 것과 함께
				// 그것이 일정 부분만 성공할 수 있는 결과를 나타낼 수 있는 것이다.
				// 만약 보내진 바이트 수가 30바이트면 나머지 70 바이트는 전송되지 않는 부분이다. 이것을 재전송해주는 것은 유저에게 책임이 있다.
				// 이러한 이유로 보내기를 원하는 바이트 수와 함께 보내진 바이트 수를 체크하는 부분이 필요!
				// 이것은 데이터를 받을 때도 나타난다.

				// 보내야 할 것이 더 있다면 나머지도 요청
				PostTcpSendRest(lpSockContext, dwTransferred);
			}
		}
	}
	return 1;

}

/*
버퍼 관리

이 부분이 IO 계층에서 가장 중요한 부붑능ㄹ 차지하고 있는 곳이다.
이 버퍼들은 요청된 데이터 또는 그것에 대한 응답으로 전송될 데이터들이 중간에 거쳐가는 부분으로 
이것의 관리방법을 보고자 하는 단락이다.,

복사의 최소화를 중점적으로 본다. 보통 전달받은 데이터들은 각 프로토콜 별로 해당 하는 양만큼을 하나의 뭉치로서 처리한다.
가령 채널에 입장하기라는 프로토콜이 100바이트의 데이터라면 그것을 하나의 단위로 처리하는 것이다.

그러한 과정에서 보통 하나의 패킷을 버퍼에 복사하는 식으로 사용한다.
하지만 서버라는 것이 유저들의 요구에 해당하는 패킷을 적절하게 해석하는 것이 대부분이므로
이런 식의 복사라는 과정은 많은 CPU 시간을 사용하는 부분 중의 하나가 될 것이다.
이러한 이유로 여기서는 그것을 최대한으로 줄일 수 있는 방향ㅇ르 찾아보려 한 것이다.

유저로 부터 전송된 데이터를 읽을 수 있도록 비동기 함수를 호출하는 부분이다.
*/


void PostTcpRecv(LPSOCKETCONTEXT lpSockContext)
{
	WSABUF			wsaBuf;
	DWORD			dwReceived, dwFlags;
	int				iResult;

	// WSARecv 함수를 위한 버퍼 설정
	dwFlags = 0;
	wsaBuf.buf = lpSockContext->cpRTEnd; // 링 버퍼의 끝에서 부터 받는다.
	wsaBuf.len = MAXRECVPACKETSIZE; // 이곳에서는 받아들일 수 있는 최대 사이즈를 MAXRECVPACKETSIZE 로 한다.
	// 또한 데이터를 받는 위치를 해당 받기 버퍼의 사용 가능한 끝(cpRTEnd)로 위치시키는 것이다.
	// 데이터를 받기에 해당하는 오버랩 구조체(eovRecvTcp)나 버퍼에 대한 표시변수 등은 모두 각각의 유저들에게
	// 있는 것을 사용함으로서 서로 독립적인 처리를 만들게 한다.
	// WSARecv가 비동기 함수이므로 제대로 된 상태에서 이 호출은 언제나 바로 넘어가며
	// 데이터가 전송되어 왔을 경우에는 이벤트가 발생하는 것이다.
	// 그 경우에는 읽어들인 데이터는 해당 구조체의 cpRTEnd를 시작으로 하여 전달된 바이트 수만큼이 저장된다.
	// 이렇게 한 후에 이 읽어들인 데이터에 대하여 버퍼를 정리하는 부분이 RecvTcpBufEnqueue이다.

	iResult = WSARecv(lpSockContext->sockTcp, &wsaBuf, 1, &dwReceived, &dwFlags,
		(OVERLAPPED*)&lpSockContext->eovRecvTcp, NULL);

	//PENDING을 제외한 나머지 에러
	if (iResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// never comes
		DConsole.Output("***** PostTcpRecv error : %d, %d\n", lpSockContext->index, WSAGetLastError());
	}

}


/*
WSARECV가 비동기 함수이므로 제대로 된 상태에서
이 호출은 언제나 바로 넘어가며 데이터가 전송되어 왔을 경우에는
이벤트가 발생하는 것이다. 그 경우에서 읽어들인 데이터는 해당 유저 구조체의 cpRTEnd를 시작으로 하여 전달된 바이트 수만큼 저장
이 후에 이 읽어들인 데이터에  대하여 버퍼를 정리하는 부분이 바로 RecvTcpBufEnqueue 이다
*/

void RecvTcpBufEnqueue(LPSOCKETCONTEXT lpSockContext, int iPacketSize)
{
	int		iExtra;


	//  전송받은 패킷으로 인하여 받은 버퍼를 초과하는 지를 검사 (포인터의 뺄셈 (주소값 연산))
	iExtra = lpSockContext->cpRTEnd + iPacketSize - lpSockContext->cRecvTcpRingBuf - RINGBUFSIZE;

	if (iExtra >= 0)
	{
		// 받기 버퍼를 초과한다면 그 부분을 앞으로 이동
		CopyMemory(lpSockContext->cRecvTcpRingBuf, lpSockContext->crEcvTcpRingBuf + RINGBUFSIZe, iExtra);

		// 큐가 쌓일 위치 조정
		lpSockContext->cpRTEnd = lpSockContext->cRecvTcpRingBuf + iExtra;
	}
	else
	{
		// 큐가 쌓일 위치 조정
		lpSockContext->cpRTEnd += iPacketSize;
	}

#ifdef _LOGLEVEL1_
	DConsole.Output( "RecvTcpBufEnqueue : %d, %d byte\n", lpSockContext->index, iPacketSize );
#endif

}