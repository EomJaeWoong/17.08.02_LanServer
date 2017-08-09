#include "stdafx.h"

//---------------------------------------------------------------------------------
// 생성자
//---------------------------------------------------------------------------------
CLanServer::CLanServer()
{
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
		// 1. 매개변수로 온 패킷에 헤더를 셋팅(만듦)
		// 2. 해당 세션의 SendQ에 패킷을 넣음( 포인터를)
		// 3. 반복문을 나간 후 SendPost 호출
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
		// GQCS 인자 초기화
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
				int i = 0;
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
			CompleteRecv(pSession, NULL);
		}

		//////////////////////////////////////////////////////////////////////////////
		// Send 완료
		//////////////////////////////////////////////////////////////////////////////
		else if (pOverlapped == &pSession->_SendOverlapped)
		{
			CompleteSend(pSession);
		}

		if (0 == InterlockedDecrement64((LONG64 *)&pSession->_lIOCount))
			ReleaseSession(pSession);

		//Count가 0보다 작으면 크래쉬 내기
		else if (0 > pSession->_lIOCount)
			// 크래쉬!

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
			OnError(1, L"Session is Maximun!");

		//////////////////////////////////////////////////////////////////////////////
		// Request 요청
		//////////////////////////////////////////////////////////////////////////////
		if (!OnConnectionRequest(clientIP, ntohs(clientSock.sin_port)))
		{
			DisconnectSession(ClientSocket);
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
				tcpkl.keepalivetime = 3000; //30초 개발할땐 짧게 라이브땐 2~30초
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

				/////////////////////////////////////////////////////////////////////
				// IOCP 등록
				/////////////////////////////////////////////////////////////////////
				result = CreateIoCompletionPort((HANDLE)Session[iCnt]->_SessionInfo._socket,
					_hIOCP,
					(ULONG_PTR)&Session[iCnt],
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
bool	CLanServer::RecvPost(SESSION *pSession, bool bAcceptRecv = false)
{
	int result, iCount = 1;
	DWORD dwRecvSize, dwflag = 0;
	WSABUF wBuf[2];

	//////////////////////////////////////////////////////////////////////////////
	// WSABUF 등록
	//////////////////////////////////////////////////////////////////////////////
	wBuf[0].buf = pSession->RecvQ.GetWriteBufferPtr();
	wBuf[0].len = pSession->RecvQ.GetNotBrokenPutSize();

	//////////////////////////////////////////////////////////////////////////////
	// 링버퍼 경계점에 왔을 때 한번 더 WSABUF등록
	//////////////////////////////////////////////////////////////////////////////
	if (pSession->RecvQ.GetWriteBufferPtr() == pSession->RecvQ.GetBufferPtr())
	{
		wBuf[1].buf = pSession->RecvQ.GetWriteBufferPtr();
		wBuf[1].len = pSession->RecvQ.GetNotBrokenPutSize();
		iCount++;
	}

	if (!bAcceptRecv)
		InterlockedIncrement64((LONG64 *)&(pSession->_lIOCount));

	result = WSARecv(pSession->_SessionInfo._socket, wBuf, iCount, &dwRecvSize, &dwflag, &pSession->_RecvOverlapped, NULL);

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
	int retval, iCount = 1;
	DWORD dwSendSize, dwflag = 0;
	WSABUF wBuf[2];
	CNPacket *pPacket = NULL;

	////////////////////////////////////////////////////////////////////////////////////
	// 현재 세션이 보내기 작업 중인지 검사
	////////////////////////////////////////////////////////////////////////////////
	if (true == InterlockedCompareExchange((LONG *)&pSession->_bSendFlag, (LONG)true, (LONG)true))
		return FALSE;

	do
	{
		////////////////////////////////////////////////////////////////////////////////
		// SendFlag -> true
		////////////////////////////////////////////////////////////////////////////////
		InterlockedExchange((LONG *)&pSession->_bSendFlag, (LONG)true);

		//////////////////////////////////////////////////////////////////////////////
		// WSABUF 등록
		//////////////////////////////////////////////////////////////////////////////
		wBuf[0].buf = pSession->SendQ.GetReadBufferPtr();
		wBuf[0].len = pSession->SendQ.GetNotBrokenGetSize();

		//////////////////////////////////////////////////////////////////////////////
		// 링버퍼 경계점에 왔을 때 한번 더 WSABUF 등록
		//////////////////////////////////////////////////////////////////////////////
		if (pSession->SendQ.GetWriteBufferPtr() == pSession->SendQ.GetBufferPtr())
		{
			wBuf[1].buf = pSession->SendQ.GetReadBufferPtr();
			wBuf[1].len = pSession->SendQ.GetNotBrokenGetSize();
			iCount++;
		}

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