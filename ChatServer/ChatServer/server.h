#pragma once
#include <winsock2.h>
#include <MSWSock.h>

#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "mswsock.lib" )

/*
���� ���� ����ü
���� ���α׷� ��ü���� ����ϴ� ���� ������ ���� ���� ����ü �Դϴ�.
�� ����ü�� ������ ������ Ÿ�Ժ��� ������ �縸ŭ�� �Ҵ�� �޸� �����͸� ���������� ���� ����ü�� ä��, �� ���� ��ü���� ���� ������ �����մϴ�.
ù ��° ������ sockListener �� ������ ������ ��ٸ��� ���� �����̴�.
*/
typedef struct
{
	SOCKET sockListener;//������ ������ ��ٸ��� ���� �����̴�.
	HANDLE hIocpWorkTcp,//IOCP�� �ڵ�μ� ������ �б� ���� �뵵
		hIocpAcept;//IOCP�� �ڵ�μ� ������ ������ �޾Ƶ��� �� ����Ѵ�.

	SOCKETCONTET* sc;	// ���� ����ü�� ���� ���� �������̴�. �̰��� �̿��ϸ� ��ü ������ ���� ������ Ž���� �� �ֽ��ϴ�.

	OBJECTNODE* pn,		
		* rn;

	// ���� ���� �����ͷ� �̰��� �̿��ؼ� ���� Ž���� ����
	CProcess* ps;	//���μ����� ������
	CChannel* ch;	//ä���� ������
	CRoom* rm;	//���� ������
	CDataBase* db;	//�����ͺ��̽��� ������

	// ���� ���Ͽ��� ���� ����
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