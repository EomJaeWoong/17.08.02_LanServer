#ifndef __LANSERVER__H__
#define __LANSERVER__H__

enum
{
	// �ִ� ������ ��
	MAX_THREAD = 50,

	// �ִ� ������ ��
	MAX_SESSION = 200,

	// WSABUF ����
	MAX_WSABUF = 100
};

//-------------------------------------------------------------------------------------
// ���� ���� ����ü
//-------------------------------------------------------------------------------------
typedef struct stSESSION_INFO
{
	SOCKET _socket;
	WCHAR _IP[16];
	int _iPort;

} SESSION_INFO;

//-------------------------------------------------------------------------------------
// ����
//-------------------------------------------------------------------------------------
typedef struct stSESSION
{
	bool _bUsed;

	SESSION_INFO _SessionInfo;
	__int64 _iSessionID;

	OVERLAPPED _SendOverlapped;
	OVERLAPPED _RecvOverlapped;

	CAyaStreamSQ SendQ;
	CAyaStreamSQ RecvQ;

	BOOL _bSendFlag;
	LONG _lIOCount;

	int _iSendPacketCnt;
} SESSION;



class CLanServer
{
public:
	CLanServer();
	virtual ~CLanServer();

	bool Start(WCHAR *wOpenIP, int iPort, int iWorkerThdNum, bool bNagle, int iMaxConnection);
	void Stop();

	int GetClientCount();

protected:
	bool SendPacket(__int64 iSessionID, CNPacket *pPacket);

	//---------------------------------------------------------------------------------
	// OnClientJoin			-> ����ó�� �Ϸ� �� ȣ��
	// OnConnectionLeave		-> Disconnect�� ȣ��
	// OnConnectionRequest		-> Accept ���� ȣ��
	//		return false; �� Ŭ���̾�Ʈ �ź�
	//		return true; �� ���� ���
	// OnRecv				-> ��Ŷ ���� �Ϸ� ��
	// OnSend				-> ��Ŷ �۽� �Ϸ� ��
	// OnWorkerThreadBegin		-> ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	// OnWorkerThreadEnd		-> ��Ŀ������ 1���� ��
	// OnError				-> ���� �޽���
	//---------------------------------------------------------------------------------
	virtual void OnClientJoin(SESSION_INFO* pSessionInfo, __int64 iSessionID) = 0;
	virtual void OnClientLeave(__int64 iSessionID) = 0;
	virtual bool OnConnectionRequest(WCHAR *ClientIP, int Port) = 0;

	virtual void OnRecv(__int64 iSessionID, CNPacket *pPacket) = 0;
	virtual void OnSend(__int64 iSessionID, int sendsize) = 0;

	virtual void OnWorkerThreadBegin() = 0;
	virtual void OnWorkerThreadEnd() = 0;

	virtual void OnError(int errorcode, WCHAR* errorMsg) = 0;

private:
	static unsigned __stdcall WorkerThread(LPVOID workerArg);
	static unsigned __stdcall AcceptThread(LPVOID acceptArg);
	static unsigned __stdcall MonitorThread(LPVOID monitorArg);

	int WorkerThread_Update();
	int AcceptThread_Update();
	int MonitorThread_Update();

	void RecvPost(SESSION *pSession, bool bAcceptRecv = false);
	BOOL SendPost(SESSION *pSession);

	bool CompleteRecv(SESSION *pSession, CNPacket *pPacket);
	bool CompleteSend(SESSION *pSession);

	//---------------------------------------------------------------------------------
	// Session Index ���� �Լ�
	//---------------------------------------------------------------------------------
	int GetBlankSessionIndex();
	void InsertBlankSessionIndex(int iSessionIndex);

	void DisconnectSession(SOCKET socket);
	void DisconnectSession(SESSION *pSession);
	void DisconnectSession(__int64 iSessionID);

	void ReleaseSession(SESSION *pSession);
	void ReleaseSession(__int64 iSessionID);

public:
	//---------------------------------------------------------------------------------
	// ����͸� ������
	//---------------------------------------------------------------------------------
	int _AcceptCounter;
	int _AcceptTotalCounter;
	int _RecvPacketCounter;
	int _SendPacketCounter;

	int _AcceptTPS;
	int _AcceptTotalTPS;
	int _RecvPacketTPS;
	int _SendPacketTPS;
	int _PacketPoolTPS;
	int _iSessionCount;

protected:
	////////////////////////////////////////////////////////////////////////
	// IOCP Handle
	////////////////////////////////////////////////////////////////////////
	HANDLE hIOCP;

	////////////////////////////////////////////////////////////////////////
	// Thread Handle
	////////////////////////////////////////////////////////////////////////
	HANDLE hAcceptThread;
	HANDLE hWorkerThread[MAX_THREAD];
	HANDLE hMonitorThread;

	////////////////////////////////////////////////////////////////////////
	// listen socket
	////////////////////////////////////////////////////////////////////////
	SOCKET listen_sock;

	SESSION Session[MAX_SESSION];

	int _iWorkerThdNum;
	__int64 _iSessionID;

	bool _bShutdown;

	bool _bNagle;
};

#endif
