#include "stdafx.h"

//---------------------------------------------------------------------------------
// ������
//---------------------------------------------------------------------------------
CLanServer::CLanServer()
{
	CCrashDump::CCrashDump();
	
	LOG_SET(LOG::FILE, LOG::LEVEL_DEBUG);

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

	if (!CNPacket::_ValueSizeCheck())
		CCrashDump::Crash();

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
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// socket ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_listen_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (_listen_sock == INVALID_SOCKET)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// bind
	//////////////////////////////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(iPort);
	InetPton(AF_INET, wOpenIP, &serverAddr.sin_addr);
	result = bind(_listen_sock, (SOCKADDR *)&serverAddr, sizeof(SOCKADDR_IN));
	if (result == SOCKET_ERROR)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//listen
	//////////////////////////////////////////////////////////////////////////////////////////////////
	result = listen(_listen_sock, SOMAXCONN);
	if (result == SOCKET_ERROR)
		return FALSE;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// nagle �ɼ�
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;
	if (_bNagle == true)
	{
		int opt_val = TRUE;
		setsockopt(_listen_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt_val, sizeof(opt_val));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Thread ����
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_hAcceptThread = (HANDLE)_beginthreadex(
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
		_hWorkerThread[iCnt] = (HANDLE)_beginthreadex(
			NULL,
			0,
			WorkerThread,
			this,
			0,
			(unsigned int *)&dwThreadID
			);
	}

	_hMonitorThread = (HANDLE)_beginthreadex(
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
		if (Session[iCnt]->_iSessionID == iSessionID)
		{
			pPacket->addRef();

			Session[iCnt]->SendQ.Put((char *)&pPacket, sizeof(pPacket));
			InterlockedIncrement64((LONG64 *)&_SendPacketCounter);
			SendPost(Session[iCnt]);
			break;
		}
	}
	return true;
}

//---------------------------------------------------------------------------------
// ���� ���ư��� �κ�(������)
//---------------------------------------------------------------------------------
// Worker Thread
int		CLanServer::WorkerThread_Update()
{
	int result;

	DWORD dwTransferred;
	OVERLAPPED *pOverlapped;
	SESSION *pSession;

	while (!_bShutdown)
	{
		///////////////////////////////////////////////////////////////////////////
		// Transferred, Overlapped, key �ʱ�ȭ
		///////////////////////////////////////////////////////////////////////////
		dwTransferred = 0;
		pOverlapped = NULL;
		pSession = NULL;

		result = GetQueuedCompletionStatus(_hIOCP, &dwTransferred, (PULONG_PTR)&pSession,
			(LPOVERLAPPED *)&pOverlapped, INFINITE);

		//----------------------------------------------------------------------------
		// Error, ���� ó��
		//----------------------------------------------------------------------------
		// IOCP ���� ���� ����
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// ��Ŀ������ ���� ����
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// ��������
		// Ŭ���̾�Ʈ ���� closesocket() Ȥ�� shutdown() �Լ��� ȣ���� ����
		//----------------------------------------------------------------------------
		else if (dwTransferred == 0)
		{
			if (pOverlapped == &(pSession->_RecvOverlapped))
			{
				result = GetLastError();
			}

			else if (pOverlapped == &(pSession->_SendOverlapped))
				pSession->_bSendFlag = false;

			if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
				ReleaseSession(pSession);

			continue;
		}
		//----------------------------------------------------------------------------

		OnWorkerThreadBegin();

		//////////////////////////////////////////////////////////////////////////////
		// Recv �Ϸ�
		//////////////////////////////////////////////////////////////////////////////
		if (pOverlapped == &pSession->_RecvOverlapped)
		{
			CompleteRecv(pSession, dwTransferred);
		}

		//////////////////////////////////////////////////////////////////////////////
		// Send �Ϸ�
		//////////////////////////////////////////////////////////////////////////////
		else if (pOverlapped == &pSession->_SendOverlapped)
		{
			CompleteSend(pSession, dwTransferred);
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
			ReleaseSession(pSession);

		//Count�� 0���� ������ ũ���� ����
		else if (0 > pSession->_lIOCount)
			CCrashDump::Crash();

		OnWorkerThreadEnd();
	}

	return 0;
}

// Accept Thread
int		CLanServer::AcceptThread_Update()
{
	HANDLE result;

	SOCKET ClientSocket;
	int addrlen = sizeof(SOCKADDR_IN);
	SOCKADDR_IN clientSock;
	WCHAR clientIP[16];

	while (!_bShutdown)
	{
		//////////////////////////////////////////////////////////////////////////////
		// Accept
		//////////////////////////////////////////////////////////////////////////////
		ClientSocket = WSAAccept(_listen_sock, (SOCKADDR *)&clientSock, &addrlen, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			DisconnectSession(ClientSocket);
			continue;
		}
		InetNtop(AF_INET, &clientSock.sin_addr, clientIP, 16);

		//////////////////////////////////////////////////////////////////////////////
		// Session���� �� á�� ��
		//////////////////////////////////////////////////////////////////////////////
		if (_iSessionCount >= MAX_SESSION)
			OnError(dfMAX_SESSION, L"Session is Maximun!");

		//////////////////////////////////////////////////////////////////////////////
		// Request ��û
		//////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(clientIP, ntohs(clientSock.sin_port)))
		{
			SocketClose(ClientSocket);
			continue;
		}
		InterlockedIncrement64((LONG64 *)&_AcceptCounter);
		InterlockedIncrement64((LONG64 *)&_AcceptTotalCounter);

		//////////////////////////////////////////////////////////////////////////////
		// ���� �߰� ����
		//////////////////////////////////////////////////////////////////////////////
		for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
		{
			// �� ����
			if (Session[iCnt]->_SessionInfo._socket == INVALID_SOCKET)
			{
				/////////////////////////////////////////////////////////////////////
				// ���� �ʱ�ȭ
				/////////////////////////////////////////////////////////////////////
				wcscpy_s(Session[iCnt]->_SessionInfo._IP, 16, clientIP);
				Session[iCnt]->_SessionInfo._iPort = ntohs(clientSock.sin_port);

				/////////////////////////////////////////////////////////////////////
				// KeepAlive
				/////////////////////////////////////////////////////////////////////
				tcp_keepalive tcpkl;

				tcpkl.onoff = 1;
				tcpkl.keepalivetime = 3000;		//30�� �����Ҷ� ª�� ���̺궩 2~30��
				tcpkl.keepaliveinterval = 2000; //  keepalive ��ȣ

				DWORD dwReturnByte;
				WSAIoctl(ClientSocket, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive), 0,
					0, &dwReturnByte, NULL, NULL);
				/////////////////////////////////////////////////////////////////////

				Session[iCnt]->_SessionInfo._socket = ClientSocket;

				Session[iCnt]->_iSessionID = InterlockedIncrement64((LONG64 *)&_iSessionID);

				Session[iCnt]->RecvQ.ClearBuffer();
				Session[iCnt]->SendQ.ClearBuffer();

				memset(&(Session[iCnt]->_RecvOverlapped), 0, sizeof(OVERLAPPED));
				memset(&(Session[iCnt]->_SendOverlapped), 0, sizeof(OVERLAPPED));

				Session[iCnt]->_bSendFlag = FALSE;
				Session[iCnt]->_lIOCount = 0;

				Session[iCnt]->_iSendPacketCnt = 0;

				/////////////////////////////////////////////////////////////////////
				// IOCP ���
				/////////////////////////////////////////////////////////////////////
				result = CreateIoCompletionPort((HANDLE)Session[iCnt]->_SessionInfo._socket,
					_hIOCP,
					(ULONG_PTR)Session[iCnt],
					0);
				if (!result)
					PostQueuedCompletionStatus(_hIOCP, 0, 0, 0);

				InterlockedIncrement64((LONG64 *)&Session[iCnt]->_lIOCount);

				/////////////////////////////////////////////////////////////////////
				// OnClientJoin
				// �������ʿ� ������ �������� �˸�
				/////////////////////////////////////////////////////////////////////
				OnClientJoin(&Session[iCnt]->_SessionInfo, Session[iCnt]->_iSessionID);

				/////////////////////////////////////////////////////////////////////
				// recv ���
				/////////////////////////////////////////////////////////////////////
				RecvPost(Session[iCnt], true);

				InterlockedIncrement64((LONG64 *)&_iSessionCount);
				break;
			}
		}
	}

	return 0;
}

// MonitorThread
int		CLanServer::MonitorThread_Update()
{
	DWORD iGetTime = timeGetTime();

	while (1)
	{
		if (timeGetTime() - iGetTime < 1000)
			continue;

		_AcceptTPS = _AcceptCounter;
		_AcceptTotalTPS += _AcceptTotalCounter;
		_RecvPacketTPS = _RecvPacketCounter;
		_SendPacketTPS = _SendPacketCounter;
		_PacketPoolTPS = 0;

		_AcceptCounter = 0;
		_AcceptTotalCounter = 0;
		_RecvPacketCounter = 0;
		_SendPacketCounter = 0;

		iGetTime = timeGetTime();
	}

	return 0;
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
bool	CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
{
	int result;
	DWORD dwRecvSize, dwflag = 0;
	WSABUF wBuf;

	//////////////////////////////////////////////////////////////////////////////
	// WSABUF ���
	//////////////////////////////////////////////////////////////////////////////
	wBuf.buf = pSession->RecvQ.GetWriteBufferPtr();
	wBuf.len = pSession->RecvQ.GetNotBrokenPutSize();


	if (!bAcceptRecv)
		InterlockedIncrement64((LONG64 *)&(pSession->_lIOCount));

	result = WSARecv(pSession->_SessionInfo._socket, &wBuf, 1, &dwRecvSize, &dwflag, &pSession->_RecvOverlapped, NULL);

	//////////////////////////////////////////////////////////////////////////////
	// WSARecv Error
	//////////////////////////////////////////////////////////////////////////////
	if (result == SOCKET_ERROR)
	{
		int iErrorCode = GetLastError();
		if (iErrorCode != WSA_IO_PENDING)
		{
			//////////////////////////////////////////////////////////////////////
			// IO_PENDING�� �ƴϸ� ��¥ ����
			//////////////////////////////////////////////////////////////////////
			if (iErrorCode != 10054)
				OnError(iErrorCode, L"RecvPost Error\n");

			if (0 == InterlockedDecrement64((LONG64 *)&(pSession->_lIOCount)))
				ReleaseSession(pSession);

			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------
// Send ���
//--------------------------------------------------------------------------------
bool	CLanServer::SendPost(SESSION *pSession)
{
	int retval, iCount = 0;
	DWORD dwSendSize, dwflag = 0;
	WSABUF wBuf[MAX_WSABUF];
	CNPacket *pPacket = NULL;

	////////////////////////////////////////////////////////////////////////////////////
	// ���� ������ ������ �۾� ������ �˻�
	// Flag value�� true  => ������ ��
	//              false => �Ⱥ����� �� -> ������ ������ �ٲ۴�
	////////////////////////////////////////////////////////////////////////////////////
	if (true == InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, true, false))
		return false;

	do
	{
		int QueueSize = pSession->SendQ.GetUseSize() / 8;

		if (QueueSize == 0)
		{
			////////////////////////////////////////////////////////////////////////////////
			// SendFlag -> false
			////////////////////////////////////////////////////////////////////////////////
			InterlockedExchange((LONG *)&pSession->_bSendFlag, false);
			break;
		}

		//////////////////////////////////////////////////////////////////////////////
		// WSABUF ���
		//////////////////////////////////////////////////////////////////////////////
		for (iCount = 0; iCount < QueueSize; iCount++)
		{
			if (iCount >= MAX_WSABUF)
				break;

			pSession->SendQ.Peek((char *)&pPacket, (pSession->_iSendPacketCnt + iCount) * sizeof(char *), sizeof(char *));

			wBuf[iCount].buf = (char *)pPacket->GetHeaderBufferPtr();
			wBuf[iCount].len = pPacket->GetPacketSize();

			pPacket = NULL;
		}

		pSession->_iSendPacketCnt += iCount;

		InterlockedIncrement64((LONG64 *)&pSession->_lIOCount);
		retval = WSASend(pSession->_SessionInfo._socket, wBuf, iCount, &dwSendSize, dwflag, &pSession->_SendOverlapped, NULL);

		//////////////////////////////////////////////////////////////////////////////
		// WSASend Error
		//////////////////////////////////////////////////////////////////////////////
		if (retval == SOCKET_ERROR)
		{
			int iErrorCode = GetLastError();
			if (iErrorCode != WSA_IO_PENDING)
			{
				if (iErrorCode != 10054)
					OnError(iErrorCode, L"SendPost Error\n");

				if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
					ReleaseSession(pSession);

				return FALSE;
			}
		}
	} while (0);

	return TRUE;
}

//--------------------------------------------------------------------------------
// Recv, Send �Ϸ�
//--------------------------------------------------------------------------------
void	CLanServer::CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	short header;

	//////////////////////////////////////////////////////////////////////////////
	// RecvQ WritePos �̵�(���� ��ŭ)
	//////////////////////////////////////////////////////////////////////////////
	pSession->RecvQ.MoveWritePos(dwTransferred);

	CNPacket *pPacket = CNPacket::Alloc();

	while (pSession->RecvQ.GetUseSize() > 0)
	{
		//////////////////////////////////////////////////////////////////////////
		// RecvQ�� ��� ���̸�ŭ �ִ��� �˻� �� ������ Peek
		//////////////////////////////////////////////////////////////////////////
		if (pSession->RecvQ.GetUseSize() <= sizeof(header))
			break;
		pSession->RecvQ.Peek((char *)&header, sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// RecvQ�� ��� ���� + Payload ��ŭ �ִ��� �˻� �� ��� ����
		//////////////////////////////////////////////////////////////////////////
		if (pSession->RecvQ.GetUseSize() < sizeof(header) + header)
			break;;
		pSession->RecvQ.RemoveData(sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// Payload�� ���� �� ��Ŷ Ŭ������ ����
		//////////////////////////////////////////////////////////////////////////
		pPacket->Put(pSession->RecvQ.GetReadBufferPtr(), header);
		pSession->RecvQ.RemoveData(header);

		//////////////////////////////////////////////////////////////////////////
		// OnRecv ȣ��
		//////////////////////////////////////////////////////////////////////////
		OnRecv(pSession->_iSessionID, pPacket);

		InterlockedIncrement64((LONG64 *)&_RecvPacketCounter);
	}

	pPacket->Free();
	RecvPost(pSession);
}

void	CLanServer::CompleteSend(SESSION *pSession, DWORD dwTransferred)
{
	CNPacket *pPacket = NULL;
	int iCnt;

	//////////////////////////////////////////////////////////////////////////////
	// ���´� ������ ����
	//////////////////////////////////////////////////////////////////////////////
	for (iCnt = 0; iCnt < pSession->_iSendPacketCnt; iCnt++)
	{
		pSession->SendQ.Get((char *)&pPacket, sizeof(char *));
		pPacket->Free();
	}

	pSession->_iSendPacketCnt -= iCnt;

	//////////////////////////////////////////////////////////////////////////////
	// SendFlag => false
	//////////////////////////////////////////////////////////////////////////////
	if (false == InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, false, true))
		OnError(dfFLAG_ERROR, L"SendFlag Error");

	//////////////////////////////////////////////////////////////////////////////
	// �������� ������ �ٽ� Send�ϵ��� ��� ��
	//////////////////////////////////////////////////////////////////////////////
	SendPost(pSession);
}

//--------------------------------------------------------------------------------
// Disconnect, Release
//--------------------------------------------------------------------------------
void CLanServer::SocketClose(SOCKET socket)
{

}

void CLanServer::DisconnectSession(SESSION *pSession)
{

}

void CLanServer::DisconnectSession(__int64 iSessionID)
{

}

void CLanServer::ReleaseSession(SESSION *pSession)
{

}

void CLanServer::ReleaseSession(__int64 iSessionID)
{

}