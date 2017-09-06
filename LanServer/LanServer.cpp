#include "stdafx.h"

//---------------------------------------------------------------------------------
// 생성자
//---------------------------------------------------------------------------------
CLanServer::CLanServer()
{
	CCrashDump::CCrashDump();
	
	LOG_SET(LOG::FILE, LOG::LEVEL_DEBUG);

	///////////////////////////////////////////////////////////////////////////////
	// 빈 세션 생성
	///////////////////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
	{
		Session[iCnt] = new SESSION;

		///////////////////////////////////////////////////////////////////////////
		// 세션 정보 구조체 초기화
		///////////////////////////////////////////////////////////////////////////
		Session[iCnt]->_SessionInfo._socket = INVALID_SOCKET;
		memset(&Session[iCnt]->_SessionInfo._IP, 0, sizeof(Session[iCnt]->_SessionInfo._IP));
		Session[iCnt]->_SessionInfo._iPort = -1;

		///////////////////////////////////////////////////////////////////////////
		// 세션 초기화
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
	// LanServer 변수 설정
	///////////////////////////////////////////////////////////////////////////////
	_iSessionID = 0;
	_bShutdown = false;
}

//---------------------------------------------------------------------------------
// 소멸자
//---------------------------------------------------------------------------------
CLanServer::~CLanServer()
{

}

//---------------------------------------------------------------------------------
// 서버 시작
//---------------------------------------------------------------------------------
bool	CLanServer::Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection)
{
	int result;
	DWORD dwThreadID;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 윈속 초기화
	//////////////////////////////////////////////////////////////////////////////////////////////////
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// IO Completion Port 생성
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hIOCP == NULL)
		return false;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// socket 생성
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
	// nagle 옵션
	//////////////////////////////////////////////////////////////////////////////////////////////////
	_bNagle = bNagle;
	if (_bNagle == true)
	{
		int opt_val = TRUE;
		setsockopt(_listen_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&opt_val, sizeof(opt_val));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Thread 생성
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
// 서버 멈춤
//---------------------------------------------------------------------------------
void	CLanServer::Stop()
{

}

//---------------------------------------------------------------------------------
// 패킷 보내기
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
// 실제 돌아가는 부분(쓰레드)
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
		// Transferred, Overlapped, key 초기화
		///////////////////////////////////////////////////////////////////////////
		dwTransferred = 0;
		pOverlapped = NULL;
		pSession = NULL;

		result = GetQueuedCompletionStatus(_hIOCP, &dwTransferred, (PULONG_PTR)&pSession,
			(LPOVERLAPPED *)&pOverlapped, INFINITE);

		//----------------------------------------------------------------------------
		// Error, 종료 처리
		//----------------------------------------------------------------------------
		// IOCP 에러 서버 종료
		if (result == FALSE && (pOverlapped == NULL || pSession == NULL))
		{
			int iErrorCode = WSAGetLastError();
			OnError(iErrorCode, L"IOCP HANDLE Error\n");

			break;
		}

		// 워커스레드 정상 종료
		else if (dwTransferred == 0 && pSession == NULL && pOverlapped == NULL)
		{
			OnError(0, L"Worker Thread Done.\n");
			PostQueuedCompletionStatus(_hIOCP, 0, NULL, NULL);
			return 0;
		}

		//----------------------------------------------------------------------------
		// 정상종료
		// 클라이언트 에서 closesocket() 혹은 shutdown() 함수를 호출한 시점
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
		// Recv 완료
		//////////////////////////////////////////////////////////////////////////////
		if (pOverlapped == &pSession->_RecvOverlapped)
		{
			CompleteRecv(pSession, dwTransferred);
		}

		//////////////////////////////////////////////////////////////////////////////
		// Send 완료
		//////////////////////////////////////////////////////////////////////////////
		else if (pOverlapped == &pSession->_SendOverlapped)
		{
			CompleteSend(pSession, dwTransferred);
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
			ReleaseSession(pSession);

		//Count가 0보다 작으면 크래쉬 내기
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
		// Session수가 꽉 찼을 때
		//////////////////////////////////////////////////////////////////////////////
		if (_iSessionCount >= MAX_SESSION)
			OnError(dfMAX_SESSION, L"Session is Maximun!");

		//////////////////////////////////////////////////////////////////////////////
		// Request 요청
		//////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(clientIP, ntohs(clientSock.sin_port)))
		{
			SocketClose(ClientSocket);
			continue;
		}
		InterlockedIncrement64((LONG64 *)&_AcceptCounter);
		InterlockedIncrement64((LONG64 *)&_AcceptTotalCounter);

		//////////////////////////////////////////////////////////////////////////////
		// 세션 추가 과정
		//////////////////////////////////////////////////////////////////////////////
		for (int iCnt = 0; iCnt < MAX_SESSION; iCnt++)
		{
			// 빈 세션
			if (Session[iCnt]->_SessionInfo._socket == INVALID_SOCKET)
			{
				/////////////////////////////////////////////////////////////////////
				// 세션 초기화
				/////////////////////////////////////////////////////////////////////
				wcscpy_s(Session[iCnt]->_SessionInfo._IP, 16, clientIP);
				Session[iCnt]->_SessionInfo._iPort = ntohs(clientSock.sin_port);

				/////////////////////////////////////////////////////////////////////
				// KeepAlive
				/////////////////////////////////////////////////////////////////////
				tcp_keepalive tcpkl;

				tcpkl.onoff = 1;
				tcpkl.keepalivetime = 3000;		//30초 개발할땐 짧게 라이브땐 2~30초
				tcpkl.keepaliveinterval = 2000; //  keepalive 신호

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
				// IOCP 등록
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
				// 컨텐츠쪽에 세션이 들어왔음을 알림
				/////////////////////////////////////////////////////////////////////
				OnClientJoin(&Session[iCnt]->_SessionInfo, Session[iCnt]->_iSessionID);

				/////////////////////////////////////////////////////////////////////
				// recv 등록
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
// 실제 쓰레드들
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
// Recv 등록
//--------------------------------------------------------------------------------
bool	CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv)
{
	int result;
	DWORD dwRecvSize, dwflag = 0;
	WSABUF wBuf;

	//////////////////////////////////////////////////////////////////////////////
	// WSABUF 등록
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
			// IO_PENDING이 아니면 진짜 에러
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
// Send 등록
//--------------------------------------------------------------------------------
bool	CLanServer::SendPost(SESSION *pSession)
{
	int retval, iCount = 0;
	DWORD dwSendSize, dwflag = 0;
	WSABUF wBuf[MAX_WSABUF];
	CNPacket *pPacket = NULL;

	////////////////////////////////////////////////////////////////////////////////////
	// 현재 세션이 보내기 작업 중인지 검사
	// Flag value가 true  => 보내는 중
	//              false => 안보내는 중 -> 보내는 중으로 바꾼다
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
		// WSABUF 등록
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
// Recv, Send 완료
//--------------------------------------------------------------------------------
void	CLanServer::CompleteRecv(SESSION *pSession, DWORD dwTransferred)
{
	short header;

	//////////////////////////////////////////////////////////////////////////////
	// RecvQ WritePos 이동(받은 만큼)
	//////////////////////////////////////////////////////////////////////////////
	pSession->RecvQ.MoveWritePos(dwTransferred);

	CNPacket *pPacket = CNPacket::Alloc();

	while (pSession->RecvQ.GetUseSize() > 0)
	{
		//////////////////////////////////////////////////////////////////////////
		// RecvQ에 헤더 길이만큼 있는지 검사 후 있으면 Peek
		//////////////////////////////////////////////////////////////////////////
		if (pSession->RecvQ.GetUseSize() <= sizeof(header))
			break;
		pSession->RecvQ.Peek((char *)&header, sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// RecvQ에 헤더 길이 + Payload 만큼 있는지 검사 후 헤더 제거
		//////////////////////////////////////////////////////////////////////////
		if (pSession->RecvQ.GetUseSize() < sizeof(header) + header)
			break;;
		pSession->RecvQ.RemoveData(sizeof(header));

		//////////////////////////////////////////////////////////////////////////
		// Payload를 뽑은 후 패킷 클래스에 넣음
		//////////////////////////////////////////////////////////////////////////
		pPacket->Put(pSession->RecvQ.GetReadBufferPtr(), header);
		pSession->RecvQ.RemoveData(header);

		//////////////////////////////////////////////////////////////////////////
		// OnRecv 호출
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
	// 보냈던 데이터 제거
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
	// 못보낸게 있으면 다시 Send하도록 등록 함
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