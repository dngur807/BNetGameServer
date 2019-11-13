#include "framework.h"
#include "server.h"


int InitIoLayer()
{
	WSADATA wd = { 0 };
	SOCKADDR_IN addr;
	HANDLE hThread;
	int dummy, iN;


	// ���� ��� IOCP �� �������� Ư������ Ȯ�� �����Լ��� ����ϱ� ���� 2.2 �������� �ʱ�ȭ
	dummy = WSAStartup(MAKEWORD(2, 2), &wd);
	if (dummy != 0)
		return 0;
	// Ŭ���̾�Ʈ�� ������ �ޱ� ���� ���� ���� ����
	Server.sockListener = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (Server.sockListener == INVALID_SOCKET)
		return 0;

	// ���� ��Ʈ�� ���ε�
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((short)Server.iPortNum);

	dummy = bind(Server.sockListener, (sockaddr*)&addr, sizeof(addr));
	if (dummy == SOCKET_ERROR)
		return 0;

	// ���� ���
	dummy = listen(Server.sockListener, 512);
	if (dummy == SOCKET_ERROR) return 0;

	/*
	IOCP�� ����ϴ� ������ ũ�� �� ������ �ܰ踦 �ʿ�� �մϴ�.
	1. IOCP�� �ڵ��� �����ϴ� ���̴�. ���ڿ� INVLID_HANDLE_VALUE , NULL�� �Ѱ��༭ ���ο� IOCP �ڵ��� ���� �� �ִ�.
	2. ���� ��ü�� IOCP�� �����ϴ� �ܰ��̴�. �̷� ������ ��ħ���μ� �� ���Ͽ��� �Ͼ �̺�Ʈ�� ���Ͽ� IOCP���� �̺�Ʈ�� �߻��� �� �ִ�.
	*/

	// Ŭ���̾�Ʈ ������ �ޱ� ���� IOCP ����
	Server.hIocpAcept = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Server.hIocpWorkTcp == NULL)
		return 0;


	// Ŭ���̾�Ʈ�� ���� ����� ���� IOCP ����
	Server.hIocpWorkTcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Server.hIocpWorkTcp == NULL)
		return 0;

	// ������ �ޱ� ���� IOCP �� ���� ������ ����
	CreateIoCompletionPort((HANDLE)Server.sockListener, Server.hIocpAcept, 0, 0);

	// ������ �ִ� ���� ����ŭ ���� ���ؽ�Ʈ �޸� �Ҵ�
	Server.sc = new SOCKETCONTEXT[Server.iMaxUserNum];
	if (Server.sc == NULL)
		return 0;

	// ������ ���� ���ؽ�Ʈ �ʱ�ȭ -> ���� ����ü�� �ʱ�ȭ
	if (!InitSocketContext(Server.iMaxUserNum)) return 0;

	// ������ �ޱ����� ���� ������ ����
	hThread = (HANDLE)_beginthreadex(NULL, 0, AcceptTProc, NULL, 0, (unsigned int*)&dummy);

	if (hThread == NULL)
		return 0;

	// IOCP ���� ����ϸ� Ŀ���� Ǯ���� �� ������.
	// Ŭ���̾�Ʈ���� ����� ���� �۾������� ����
	for (iN = 0; iN < Server.iInWorkerTNum; iN++)	//����ڿ��� ������ ����� ���� ������� WORKERTHREAD ���� ��ŭ�� �����Ѵ�.
	{
		hThread = (HANDLE)_beginthreadex(NULL, 0, InWorkerTcpTProc, (void*)iN, 0, (unsigned int*)&dummy);
		if (hThread == NULL) return 0;
	}

	return 1;
}


// ���� ����ü�� �ʱ�ȭ
int InitSocketContext(int maxUser)
{
	int				dummy;
	DWORD			dwReceived;
	LPSOCKETCONTEXT	sc = Server.sc;


	
	// ���� ���ؽ�Ʈ ����ü ��ü�� 0���� �ʱ�ȭ �Ѵ�.
	ZeroMemory(sc, sizeof(SOCKETCONTEXT) * maxUser);

	for (int iN = 0; iN < maxUser; ++iN)
	{
		// ���� ���ٽ�Ʈ ���� �ε��� �ο�
		// �ε��� �ʱ�ȭ ���������� �־����� �� ���� �ٷ� ���� ������ ������ �����ϴµ� ���Ǵ� �߿��� ���̴�.
		sc[iN].index = iN;

		// ���� ���ؽ�Ʈ�� �Ҵ�� ���� �ʱ�ȭ
		sc[iN].iSTRestCnt = 0;
		sc[iN].eovSendTcp.mode = SENDEOVTCP;// ������ ����ü ����
		sc[iN].eovRecvTcp.mode = RECVEOVTCP;// �б� ����

		// �����ۿ� �ش� ���� ���� ������ �ʱ�ȭ
		sc[iN].cpSTBegin = sc[iN].cSendTcpRingBuf;
		sc[iN].cpRTMark = sc[iN].cpRTBegin = sc[iN].cpRTEnd = sc[iN].cRecvTcpRingBuf;



		// Ŭ���̾�Ʈ���� ����� ���� ���� ����
		sc[iN].sockTcp = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (sc[iN].sockTcp == INVALID_SOCKET)
			return 0;

		// Ŭ���̾�Ʈ �� �ϳ��� �ٰ� �Ǵ� ������ ������ �Ŀ� �װ͵��� AcceptEX�� ����
		// ������ ������ ����Ŵ
		dummy = AcceptEx(Server.sockListener, sc[iN].sockTcp, sc[iN].cpRTEnd,
			MAXRECVPACKETSIZE, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			&dwReceived, (OVERLAPPED*)&sc[iN].eovRecvTcp);

		if (dummy == FALSE && GetLastError() != ERROR_IO_PENDING)
			return 0;

		// ���� �۾����� �ʿ��� �Ӱ迵��
		// SendBuffer�� ���� �Ӱ迵���� �����մϴ�.
		InitializeCriticalSection(&sc[iN].csSTcp);

		/*For Game Processing*/
		// ��󿡼� ���Ǵ�
		// �⺻ ���μ��� , ä��, �� ����
		sc[iN].iProcess = DEFAULTPROCESS;
		sc[iN].iChannel = DEFAULTCHANNEL;
		sc[iN].iRoom = NOTLINKED;

	}
}

// ���� ����ü�� � ������ A��� ���� ����ü�� ����ϴٰ� �����ϸ� �� �ٸ�
// ������ A��� ���� ����ü�� ����� �� �ִ� ���̹Ƿ� ������ �����Ͽ��� ����
// �Ȱ��� ������� �ʱ�ȭ ���־�� �Ѵ�. �� �κ��� ReInitSocketContext;
int ReInitSocketContext(LPSOCKETCONTEXT lpSockContext)
{
}


/*
�����㰡 ������

�׽�Ʈ �������� IO ������ �ٽ����� Ŭ���̾�Ʈ�� �����ϴ� ��� ���� �۵��� �� �ִ� �������Դϴ�.
�ٽ��̶�� ���� ��â�ϰ� ������ ���� ������ ����� ������ ���·� ���� ���� ������ �� �ִ� ������� ���� ���� �� �ִ�.
*/

unsigned int __stdcall AcceptTProc(void* pParam)
{
	DWORD					dummy, dwTransferred;
	LPEOVERLAPPED			lpEov;
	LPSOCKETCONTEXT			lpSockContext;
	BOOL					bResult;
	sockaddr_in				* pLocal, * pRemote;
	int						localLen, remoteLen;

	// while�� �̷���� ���� �������� ���� �Ǵµ�
	// �̶� ��� ������ �ð��� ���ٸ� �ý����� �����ϰ� ������ �� ���̴�.
	// ������ �ð��� ����� ���� GetQueuedCompletionStatus �̴�.

	// �� API�� IOCP�� ���� �Է� �̳� ��� �� �Ϸῡ ���� �̺�Ʈ�� ���ٸ� �׷��� ��ȣ�� ��ٸ��鼭 ���ŷ�Ǵ� �Լ� �̴�.
	// ������ �� �� �ִ� �����Ͱ� ���� ��쿡�� �����尡 ���ȴٴ� ���� ������� �� �ִ�.
	while (1)
	{
		
		// ù��° CP ���ڿ� �������� ��ϵ� �ð��� �����ϴ� ���� 
		// �׿��� ���ڴ� ��� (Get)�����̴�.
		// �̰��� �����尡 ��ϵǾ� ���� �� �߻��� ��ȣ�� � ���¿��� �߻��� ���ΰ��� �Ѱ��ִ� ���̴�.
		// �̷��� �Ѱ��ִ� ���� ù��°�� ������ ���������� ���۵� ����Ʈ ���̴�. 
		// ù��°�� ���۵� ����Ʈ ���̴�. �̰��� �б� (WSARecv) �� ���Ͽ� �߻��� ���̶�� �о���� ����Ʈ ��, ���� (WSASend) �� ���Ͽ� �߻��� ���̸� ������ ����Ʈ ���� ����Ų��.
		// �ι�° ���� ��ũ ������ ������ CP Key��� ���̴�. �տ� CreateIOCompletionPort�� �̿��Ͽ� ���ϰ� IOCP�� ������ �� �ٷ� Completion Key��� ���� �����Ѵ�.

		// �� �� � ������� �߻��Ͽ��� �� �̺�Ʈ�� ���� GetQueued �� ��� ���� �Ǿ��� ���� �� Completion Key�� �տ����� ���� ������ �����Ͽ��� �� �Ѱ�� ���� ���޵ȴ�.
		// ���� �̰��� �ϳ��� ���а����μ� ����� �� �ִ�. ��� Accept �����忡���� �� ���� ���ǹ� �ϴ� ���޵� ���� �������� �ʱ� �����Դϴ�.

		// ����° ������ ���� �� ����ü�� ������ ���̴�. �̰��� �ٷ� ����¿� ���Ǿ��� ������ ����ü�� �ּҸ� �Ѱ��ִ� �κ��̴�.
		// ��� ���� �񵿱�� �۵��ϰ� �ǹǷ� ��� �۾��� �Ͼ ���� �׻� ������ ����ü�� ����ϸ� �׿� ���� �ּҰ� ���޵˴ϴ�.
		bResult = GetQueuedCompletionStatus(Server.hIocpAcept, &dwTransferred, (LPDWORD)&dummy, (LPOVERLAPPED*)&lpEov, INFINITE);


		// �̺�Ʈ �߻�! ���� ���� �̺�Ʈ �� ���̴�.
		// �ռ� ������ AcceptEx�� Ŭ���̾�Ʈ�� ������ �Ŀ� ù��° ������ ����� ���۵Ǿ�߸� �������� ������ �ν��Ѵٰ� �Ͽ���.
		// �߿��� ���� �̷��� ù��° ����� �Ѿ���� �� ���� ��Ʈ��ũ �����̹Ƿ� AcceptEx�� ȣ���� ��ÿ��� ������ ����ü�� ����Ѵٴ� ���̴�.

		// ���� GetQueue ���� ��� ���� ù��° �о���� ù��° ������ ����� ��, �ι�° CP Key  ,����° AcceptEx�� ���ڷ� �־��� �� Ȯ�� ������ ����ü �ּ��̴�.

		// �� Ȯ��� ������ ����ü�� ���谡 ��� �ϳ��� ���� ����ü�ȿ� ���ϴ� �͵�� �̷������. AcceptEx�� ȣ���� �� ��� ���� ���� ����ü ���� ������θ� ���Ǿ��� �����Դϴ�.
		// ������ ���� ������ Ŭ�� ��� ���� ����ü�� �Ҵ�޾Ҵ����� �� �� �ִ� �ٰŰ� �˴ϴ�.

		// �̺�Ʈ�� �߻��Ͽ� GetQueue�� ��������� �������� ������ Ŭ��� �ڱ�� ����� ���� �ϳ��� ���ϰ� ���� Ȯ��� ������ ����ü�� �̹� �Ҵ���� ����
		// �̱� ������ ���� �׵��� ���� ���� ����ü �ȿ� ���ԵǾ� �ֱ� ������ �� Ȯ��� ������ ����ü�� �ּҸ��� �˰� �ȴٸ� ������ Ŭ�� ��� ���� ����ü�� ����ϴ��� 
		// �� �� �ִ�. ����
		// lpSockContext = (LPSOCKETCONTEXT)lpEov;

		// ������ ���� ���
		if (lpEov != NULL && dwTransferred != 0)
		{
			// --- increase current user number ---
			Server.iCurUserNum++;

			// �̺�Ʈ�� �߻��� ���� ���ؽ�Ʈ�� ����
			// --- get socketcontext ---
			lpSockContext = (LPSOCKETCONTEXT)lpEov;


			// ���� �ּҸ� �˾Ƴ��� Ŭ�� �Ҵ�� ���� ����ü�� ����� ������ �ִ� �κ�
			// ���ӹ��� ���� �ּҸ� ��� ����
			// --- connect realize ----
			GetAcceptExSockaddrs(lpSockContext->cpRTEnd, MAXRECVPACKETSIZE, sizeof(sockaddr_in) + 16,
				sizeof(sockaddr_in) + 16, (sockaddr**)&pLocal, &localLen, (sockaddr**)&pRemote, &remoteLen);

			// --- store remoteAddr ---
			CopyMemory(&lpSockContext->remoteAddr, pRemote, sizeof(sockaddr_in));

			// �б� ������ ��� ���Ͽ� ���� �߿��� �κ��� �����ϰ� �ִ�.
			// �б� ���� ��ġ ����
			// --- setting end pointer ---
			RecvTcpBufEnqueue(lpSockContext, dwTransferred);


			// Accept �������� ���� ��� �������Ƿ� Worker ������� ���� ��
			// Ŭ�� ���� ����ü�� �Ҵ�� ������ Ŭ�� Work�� �����ϴ� �۾��� �Ѵ�.


			// ���ӵ� Ŭ���̾�Ʈ���� ����� ����
			// �۾��� IOCP�� ����
			// -- tcp socket association ---
			CreateIoCompletionPort((HANDLE)lpSockContext->sockTcp, Server.hIocpWorkTcp, (DWORD)lpSockContext, 0);

			// �񵿱� �����̴�.
			// ���ŷ ���Ͽ����� recv���� ��� ũ�������� �ݵ��
			// ���� �Ŀ� ����� �����Ǵ� �ݸ鿡 �񵿱� ������ �ϴ� ������ ������ �ϸ� �Ŀ� ��� �̺�Ʈ �߻��Ͽ��� �� ó���ϴ� ���
			// ���� �� �б⸦ ���� �񵿱� �б⸦ �õ�
			// tcp socket postrecv
			PostTcpRecv(lpSockContext);

			// ���ƿ� �����͸� �ؼ��ϵ��� ��û�� ���Ͽ� �ִ� �κ� ť�� �װ� ó���� ���μ������� �����Ѵ�.
			// ��Ŷ ó���� ��û
			GameBufEnqueueTcp(lpSockContext);
		}
	}
}


/*
�۾� ������
	�۾� �����尡 ������ �������� ������ ó���ϴ� �κ��̴�.
	������ ���� �����͸� ���ۿ� �����ϰ� �װ��� ó���ϵ��� �䱸�ϸ�
	���� �������� ������ �� �����͸� ������ ������ �� �����忡�� �ϴ� ���̴�.

	�����δ� ��������� �Ǹ� �װ��� Ǯ���ϴ� �۾��� Ŀ�ο��� �Ѵ�.
	���� ����ŭ �����带 �̸� �����Ͽ� ���� �� ������ ��� ��� �ߴ� �۾� ��û�� ������
	�� �� �ϳ��� Ȱ��ȭ�Ǿ� �� �۾��� ó���ϴ� ���̴�.

	�̹� ��� �����尡 Ȱ��ȭ�� ������ ��ٸ��� ��밡�������� ����
*/
unsigned int __stdcall InWorkerTcpTProc(void* pParam)
{
	DWORD				dwTransferred;
	LPEOVERLAPPED		lpEov;
	LPSOCKETCONTEXT		lpSockContext;
	BOOL				bResult;

	while (1)
	{
		// �����κ��� �����͸� �ްų� �������� �������� 
		// ���� �������� �Ϸᰡ �Ͼ �� ���� ���
		// �Ѿ�� ������ ����ü�� �����ʹ� ��� �۾�(WSASend, WSARecv)�� ��û�� �� ���ڷ� �־����� ���̴�. �̷� ��û�� ������ ������ ���Ͽ� 
		// ���ڷ� �־����� ���̴�. �̷��� ��û�� ������ ������ ���Ͽ� �ϰ� �ǹǷ� �տ��� �� �Ͱ� ���� ���� ����ü �ȿ� ����� ���ԵǾ� �ִ� ���̴�.
		// ������ ����� ���� ������ ����ü�� ��� �۾��� ��������� ��Ÿ���� ���Ͽ� ���� ���� ������ �ִ� ���·� ���
		// ����� ��� �۾� ������� �Ѿ���� ������ �߻��� �̺�Ʈ�� � �Ϳ� ���� ����ΰ��� �����ϱ� ���Ͽ� ���ȴ�. 
		bResult = GetQueuedCompletionStatus(Server.hIocpWorkTcp, &dwTransferred, (LPDWORD)&lpSockContext, (LPOVERLAPPED*)&lpEov, INFINITE);

		if (lpEov != NULL)
		{
			// ���� �Ѱ��� ������ ����ü�� NULL �� �ƴѵ� ó���� ����Ʈ ���� 0�̸�
			// ������ Ŭ�� ���� ������ ���� ���� �׶� EnqueueClose ��� �Լ��� �̿��ؼ� ó��
			
			// ������ ����
			if (dwTransferred == 0)
			{
				// LOGOUT
				EnqueueClose(lpSockContext);
			}
		}
		else
		{
			// �б���� ������ ����ü���� ��ȣ
			if (lpEov->mode == RECVEOVTCP)
			{
				// �� �����͸� ť�� �ִ� �۾�
				// �б� ������ġ ����
				RecvTcpBufEnqueue(lpSockContext, dwTransferred);

				// �����͸� �ؼ��ϵ��� ��û
				// ��Ŷ ó���� ��û
				GameBufEnqueueTcp(lpSockContext);

				// �ٽ� �����͸� ���� �� �ֵ���
				// ������ �б⸦ ���� �񵿱� �б⸦ �õ�
				PostTcpRecv(lpSockContext);
			}
			// ������� ������ ����ü���� ��ȣ
			else if (lpEov->mode == SENDEOVTCP)
			{
				// TCP ���� ����� ������ �������̳� �������� �����Ѵ�.
				// �̿� ���� �ϳ��� ��Ŷ ������� ���� �������� �ʴ´�.
				// ex) 100 ����Ʈ�� ������ ���Ͽ� WSASend(100)�̶� �Ѵٰ� �ϴ���� �̰���
				// �ѹ��� 100����Ʈ�� ��� ���۵� ����� ��Ÿ�� �� �ִ� �Ͱ� �Բ�
				// �װ��� ���� �κи� ������ �� �ִ� ����� ��Ÿ�� �� �ִ� ���̴�.
				// ���� ������ ����Ʈ ���� 30����Ʈ�� ������ 70 ����Ʈ�� ���۵��� �ʴ� �κ��̴�. �̰��� ���������ִ� ���� �������� å���� �ִ�.
				// �̷��� ������ �����⸦ ���ϴ� ����Ʈ ���� �Բ� ������ ����Ʈ ���� üũ�ϴ� �κ��� �ʿ�!
				// �̰��� �����͸� ���� ���� ��Ÿ����.

				// ������ �� ���� �� �ִٸ� �������� ��û
				PostTcpSendRest(lpSockContext, dwTransferred);
			}
		}
	}
	return 1;

}

/*
���� ����

�� �κ��� IO �������� ���� �߿��� �κִɤ� �����ϰ� �ִ� ���̴�.
�� ���۵��� ��û�� ������ �Ǵ� �װͿ� ���� �������� ���۵� �����͵��� �߰��� ���İ��� �κ����� 
�̰��� ��������� ������ �ϴ� �ܶ��̴�.,

������ �ּ�ȭ�� ���������� ����. ���� ���޹��� �����͵��� �� �������� ���� �ش� �ϴ� �縸ŭ�� �ϳ��� ��ġ�μ� ó���Ѵ�.
���� ä�ο� �����ϱ��� ���������� 100����Ʈ�� �����Ͷ�� �װ��� �ϳ��� ������ ó���ϴ� ���̴�.

�׷��� �������� ���� �ϳ��� ��Ŷ�� ���ۿ� �����ϴ� ������ ����Ѵ�.
������ ������� ���� �������� �䱸�� �ش��ϴ� ��Ŷ�� �����ϰ� �ؼ��ϴ� ���� ��κ��̹Ƿ�
�̷� ���� ������ ������ ���� CPU �ð��� ����ϴ� �κ� ���� �ϳ��� �� ���̴�.
�̷��� ������ ���⼭�� �װ��� �ִ������� ���� �� �ִ� ���⤷�� ã�ƺ��� �� ���̴�.

������ ���� ���۵� �����͸� ���� �� �ֵ��� �񵿱� �Լ��� ȣ���ϴ� �κ��̴�.
*/


void PostTcpRecv(LPSOCKETCONTEXT lpSockContext)
{
	WSABUF			wsaBuf;
	DWORD			dwReceived, dwFlags;
	int				iResult;

	// WSARecv �Լ��� ���� ���� ����
	dwFlags = 0;
	wsaBuf.buf = lpSockContext->cpRTEnd; // �� ������ ������ ���� �޴´�.
	wsaBuf.len = MAXRECVPACKETSIZE; // �̰������� �޾Ƶ��� �� �ִ� �ִ� ����� MAXRECVPACKETSIZE �� �Ѵ�.
	// ���� �����͸� �޴� ��ġ�� �ش� �ޱ� ������ ��� ������ ��(cpRTEnd)�� ��ġ��Ű�� ���̴�.
	// �����͸� �ޱ⿡ �ش��ϴ� ������ ����ü(eovRecvTcp)�� ���ۿ� ���� ǥ�ú��� ���� ��� ������ �����鿡��
	// �ִ� ���� ��������μ� ���� �������� ó���� ����� �Ѵ�.
	// WSARecv�� �񵿱� �Լ��̹Ƿ� ����� �� ���¿��� �� ȣ���� ������ �ٷ� �Ѿ��
	// �����Ͱ� ���۵Ǿ� ���� ��쿡�� �̺�Ʈ�� �߻��ϴ� ���̴�.
	// �� ��쿡�� �о���� �����ʹ� �ش� ����ü�� cpRTEnd�� �������� �Ͽ� ���޵� ����Ʈ ����ŭ�� ����ȴ�.
	// �̷��� �� �Ŀ� �� �о���� �����Ϳ� ���Ͽ� ���۸� �����ϴ� �κ��� RecvTcpBufEnqueue�̴�.

	iResult = WSARecv(lpSockContext->sockTcp, &wsaBuf, 1, &dwReceived, &dwFlags,
		(OVERLAPPED*)&lpSockContext->eovRecvTcp, NULL);

	//PENDING�� ������ ������ ����
	if (iResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// never comes
		DConsole.Output("***** PostTcpRecv error : %d, %d\n", lpSockContext->index, WSAGetLastError());
	}

}


/*
WSARECV�� �񵿱� �Լ��̹Ƿ� ����� �� ���¿���
�� ȣ���� ������ �ٷ� �Ѿ�� �����Ͱ� ���۵Ǿ� ���� ��쿡��
�̺�Ʈ�� �߻��ϴ� ���̴�. �� ��쿡�� �о���� �����ʹ� �ش� ���� ����ü�� cpRTEnd�� �������� �Ͽ� ���޵� ����Ʈ ����ŭ ����
�� �Ŀ� �� �о���� �����Ϳ�  ���Ͽ� ���۸� �����ϴ� �κ��� �ٷ� RecvTcpBufEnqueue �̴�
*/

void RecvTcpBufEnqueue(LPSOCKETCONTEXT lpSockContext, int iPacketSize)
{
	int		iExtra;


	//  ���۹��� ��Ŷ���� ���Ͽ� ���� ���۸� �ʰ��ϴ� ���� �˻� (�������� ���� (�ּҰ� ����))
	iExtra = lpSockContext->cpRTEnd + iPacketSize - lpSockContext->cRecvTcpRingBuf - RINGBUFSIZE;

	if (iExtra >= 0)
	{
		// �ޱ� ���۸� �ʰ��Ѵٸ� �� �κ��� ������ �̵�
		CopyMemory(lpSockContext->cRecvTcpRingBuf, lpSockContext->crEcvTcpRingBuf + RINGBUFSIZe, iExtra);

		// ť�� ���� ��ġ ����
		lpSockContext->cpRTEnd = lpSockContext->cRecvTcpRingBuf + iExtra;
	}
	else
	{
		// ť�� ���� ��ġ ����
		lpSockContext->cpRTEnd += iPacketSize;
	}

#ifdef _LOGLEVEL1_
	DConsole.Output( "RecvTcpBufEnqueue : %d, %d byte\n", lpSockContext->index, iPacketSize );
#endif

}