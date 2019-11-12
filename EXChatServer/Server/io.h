#pragma once

#define MAXSENDPACKETSIZE	5120
#define MAXRECVPACKETSIZE	5120
#define RINGBUFSIZE			65536 //32768
#define MAXTRANSFUNC		256
#define HEADERSIZE			4

/*
	���� ����ü : �̰��� ���ӵ� ������ �ϳ��� �Ҵ�Ǵ� ������ ������ ��� ������ ���� �ִ� ����ü
	������ �����Ҷ����� �Ҵ� , ���� �ϴ°��� �ƴ϶�, ������ ���۵� �� �̸� ������ ����ŭ�� �Ҵ��ϴ� ��� -> �޸� ����ȭ�� ������ �������� �����Դϴ�.

	���� ���� ����Ʈ�� �Ҵ��ϴ� ���� �ƴ϶� ���� ����ü�� �������� �Ҵ��ϰų� �����ϰ� �Ǹ� �޸� ����ȭ�� �ݵ�� ��Ÿ����.

	�޸� �Ҵ�� �������� �������� ���ɿ� ������.
	�޸� ���� �δٴ� ���� �����ϸ� �װ��� �����ؼ� ���� ���� ������ ������ �ϴ� ���� ����!!
	���� ������ �޸��� �Ҵ�� �����ÿ��� �޸��� Ÿ�缺�� ������ �˻��ؾ� �ϸ� �׷��� �κ��� ������� �ʾ��� ��� �޸� ħ�� ���� ���� �߻��� ������ �ִ�.

	�̷��� �������� �޸𸮸� �̸� �Ҵ�޾� ���� ��������μ� ������ �� �ִ� ���̴�.
*/

typedef struct
{
	OVERLAPPED	ovl;
	// ���¸��� Ŭ�󿡼� �Ѿ�� �����Ͱ� �б����� ������ �����ϱ� ���� �÷���
	int			mode;
}EOVERLAPPED, * LPEOVERLAPPED;

/* ���� ����ü */
typedef struct
{
	// Ȯ��� ������
	EOVERLAPPED					eovRecvTcp,
		eovSendTcp;
	// ���� ��� �ʿ��� �͵��̴�.
	char						cRecvTcpRingBuf[RINGBUFSIZE + MAXRECVPACKETSIZE],	// �б⿡�� ������ �����Ͱ� ����� �����̴�.
		* cpRTBegin,	//  �� ������ ����
		* cpRTEnd,		//	�� ������ ��
		* cpRTMark, /* For GameBufEnqueue Start Position */

		cSendTcpRingBuf[RINGBUFSIZE + MAXSENDPACKETSIZE], // ���⿡�� ������ �����Ͱ� ����� �����Դϴ�.
		* cpSTBegin,	// �� ������ ����
		* cpSTEnd;		//�� ������ ��

	int							iSTRestCnt;// Ŭ���̾�Ʈ�� ������ �ϴ� �����Ͱ� �󸶳� ���Ҵ���
	SOCKET						sockTcp;		//���� ����� ���� ����
	CRITICAL_SECTION			csSTcp; // ���� ���ۿ����� ����ȭ�� ���� �Ӱ� ���� ��ü
	sockaddr_in					remoteAddr;		//������ Ŭ���̾�Ʈ�� �ּҸ� ��Ÿ����.
	int							index;			//������ Ŭ���̾�Ʈ�� ������ ���� �ε����̴�.

	char					cID[32];		//����ü�� �Ҵ�� Ŭ���̾�Ʈ�� ID
	int						idLen;			//���̵��� ����

	int						iProcess,		//���� ������ ��ġ�ϰ� �ִ� ���� �ε���, ���� ���ӽ� Process�� Channel�� 0�� �ε������� �����Ѵ�.
		iChannel,
		iRoom,

}SOCKETCONTEXT, * LPSOCKETCONTEXT;