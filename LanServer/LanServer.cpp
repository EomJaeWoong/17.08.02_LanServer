#include "stdafx.h"

//---------------------------------------------------------------------------------
// ������
//---------------------------------------------------------------------------------
CLanServer::CLanServer()
{
	///////////////////////////////////////////////////////////////////////////////
	// �� ���� ����
	///////////////////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		Session[iCnt] = new SESSION;

		///////////////////////////////////////////////////////////////////////////
		// ���� ���� ����ü �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		Session[iCnt]->_SessionInfo._socket = INVALID_SOCKET;
		memset(&Session[iCnt]->_SessionInfo._IP, 0, sizeof(Session[iCnt]->_SessionInfo._IP));
		Session[iCnt]->_SessionInfo._iPort = -1;

		///////////////////////////////////////////////////////////////////////////
		// ���� �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		Session[iCnt]->_iSessionID = 0;

		memset(&Session[iCnt]->_SendOverlapped, 0, sizeof(OVERLAPPED));
		memset(&Session[iCnt]->_RecvOverlapped, 0, sizeof(OVERLAPPED));

		Session[iCnt]->SendQ.ClearBuffer();
		Session[iCnt]->RecvQ.ClearBuffer();

		Session[iCnt]->_bSendFlag = false;
		Session[iCnt]->_lIOCount = 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	// LanServer ���� ����
	///////////////////////////////////////////////////////////////////////////////
	_iSessionID = 0;
	_bShutdown = false;
}

//---------------------------------------------------------------------------------
// �Ҹ���
//---------------------------------------------------------------------------------
CLanServer::~CLanServer()
{

}

//---------------------------------------------------------------------------------
// ���� ����
//---------------------------------------------------------------------------------
bool	CLanServer::Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection)
{
	int result;
	DWORD dwThreadID;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ���� �ʱ�ȭ
	//////////////////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// IO Completion Port ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// socket ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	listen_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listen_sock == INVALID_SOCKET)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// bind
	//////////////////////////////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(iPort);
	InetPton(AF_INET, wOpenIP, &serverAddr.sin_addr);
	result = bind(listen_sock, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (result == SOCKET_ERROR)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//listen
	//////////////////////////////////////////////////////////////////////////////////////////////////
	result = listen(listen_sock, SOMAXCONN);
	if (result == SOCKET_ERROR)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// nagle �ɼ�
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;
	if (_bNagle == true)
	{
		int opt_val = TRUE;
		setsockopt(listen_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt_val, sizeof(opt_val));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Thread ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	hAcceptThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		AcceptThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	_iWorkerThdNum = iWorkerThdNum;

	for (int iCnt = 0; iCnt < iWorkerThdNum; iCnt++)
	{
		hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			this,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	hMonitorThread = (HANDLE)_beginthreadex(
		NULL,
		0,
		MonitorThread,
		this,
		0,
		(unsigned int *)&dwThreadID
		);

	return TRUE;
}

//---------------------------------------------------------------------------------
// ���� ����
//---------------------------------------------------------------------------------
void	CLanServer::Stop()
{

}

//---------------------------------------------------------------------------------
// ��Ŷ ������
//---------------------------------------------------------------------------------
bool	CLanServer::SendPacket(__int64 iSessionID, CNPacket *pPacket)
{
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		// 1. �Ű������� �� ��Ŷ�� ����� ����(����)
		// 2. �ش� ������ SendQ�� ��Ŷ�� ����( �����͸�)
		// 3. �ݺ����� ���� �� SendPost ȣ��
	}

	return true;
}

//---------------------------------------------------------------------------------
// ���� ���ư��� �κ�(������)
//---------------------------------------------------------------------------------
// Worker Thread
int		CLanServer::WorkerThread_Update()
{

}

// Accept Thread
int		CLanServer::AcceptThread_Update()
{

}

// MonitorThread
int		CLanServer::MonitorThread_Update()
{

}

//---------------------------------------------------------------------------------
// ���� �������
//---------------------------------------------------------------------------------
unsigned __stdcall	CLanServer::WorkerThread(LPVOID workerArg)
{
	return ((CLanServer *)workerArg)->WorkerThread_Update();
}

unsigned __stdcall	CLanServer::AcceptThread(LPVOID acceptArg)
{
	return ((CLanServer *)acceptArg)->AcceptThread_Update();
}

unsigned __stdcall	CLanServer::MonitorThread(LPVOID monitorArg)
{
	return ((CLanServer *)monitorArg)->MonitorThread_Update();
}

//--------------------------------------------------------------------------------
// Recv ���
//--------------------------------------------------------------------------------
bool	CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv = false)
{

}

//--------------------------------------------------------------------------------
// Send ���
//--------------------------------------------------------------------------------
bool	CLanServer::SendPost(SESSION *pSession)
{

}

//--------------------------------------------------------------------------------
// Recv, Send �Ϸ�
//--------------------------------------------------------------------------------
void	CLanServer::CompleteRecv(SESSION *pSession, CNPacket *pPacket)
{

}

void	CLanServer::CompleteSend(SESSION *pSession)
{

}

//--------------------------------------------------------------------------------
// Disconnect, Release
//--------------------------------------------------------------------------------
void SocketClose(SOCKET socket);
void DisconnectSession(SESSION *pSession);
void DisconnectSession(__int64 iSessionID);

void ReleaseSession(SESSION *pSession);
void ReleaseSession(__int64 iSessionID);