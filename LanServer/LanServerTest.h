#ifndef __LANSERVERTEST__H__
#define __LANSERVERTEST__H__

class CLanServerTest : public CLanServer
{
public:
	CLanServerTest();
	virtual ~CLanServerTest();

public:
	virtual void OnClientJoin(SESSION_INFO *pSessionInfo, __int64 iSessionID);   // Accept �� ����ó�� �Ϸ� �� ȣ��.
	virtual void OnClientLeave(__int64 iSessionID);   					// Disconnect �� ȣ��
	virtual bool OnConnectionRequest(WCHAR *SessionIP, int Port);		// accept ����

	virtual void OnRecv(__int64 iSessionID, CNPacket *pPacket);			// ��Ŷ ���� �Ϸ� ��
	virtual void OnSend(__int64 iSessionID, int sendsize);				// ��Ŷ �۽� �Ϸ� ��

	virtual void OnWorkerThreadBegin();								// ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
	virtual void OnWorkerThreadEnd();								// ��Ŀ������ 1���� ���� ��

	virtual void OnError(int errorCode, WCHAR *errorString);

};

#endif