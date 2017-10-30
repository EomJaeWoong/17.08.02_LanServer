#include "stdafx.h"

CLanServerTest::CLanServerTest()
	: CLanServer(){}

CLanServerTest::~CLanServerTest(){}

void CLanServerTest::OnClientJoin(SESSION_INFO *pSessionInfo, __int64 ClientID)		// Accept �� ����ó�� �Ϸ� �� ȣ��.
{
	///////////////////////////////////////////////////////////////
	// Login Packet Send
	///////////////////////////////////////////////////////////////
	/*
	PRO_BEGIN(L"PacketAlloc");
	CNPacket *pLoginPacket = CNPacket::Alloc();
	PRO_END(L"PacketAlloc");

	*pLoginPacket << (short)8;
	*pLoginPacket << 0x7fffffffffffffff;

	PRO_BEGIN(L"SendPacket");
	SendPacket(ClientID, pLoginPacket);
	PRO_END(L"SendPacket");

	pLoginPacket->Free();
	*/
}

void CLanServerTest::OnClientLeave(__int64 ClientID)   					// Disconnect �� ȣ��
{

}

bool CLanServerTest::OnConnectionRequest(WCHAR *ClientIP, int Port)		// accept ����
{
	return true;
}

void CLanServerTest::OnRecv(__int64 ClientID, CNPacket *pPacket)			// ��Ŷ ���� �Ϸ� ��
{
	CNPacket *pSendPacket = CNPacket::Alloc();

	__int64 iValue;

	// Packet Process
	*pPacket >> iValue;

	pSendPacket->SetCustomShortHeader((unsigned short)sizeof(iValue));
	*pSendPacket << iValue;
	//////////////////////

	SendPacket(ClientID, pSendPacket);

	pSendPacket->Free();
}

void CLanServerTest::OnSend(__int64 ClientID, int sendsize)				// ��Ŷ �۽� �Ϸ� ��
{

}

void CLanServerTest::OnWorkerThreadBegin()								// ��Ŀ������ GQCS �ٷ� �ϴܿ��� ȣ��
{

}

void CLanServerTest::OnWorkerThreadEnd()								// ��Ŀ������ 1���� ���� ��
{

}

void CLanServerTest::OnError(int errorCode, WCHAR *errorString)
{
	LOG(NULL, LOG::LEVEL_DEBUG, L"ErrorCode : %2d, ErrorMsg : %s\n", errorCode, errorString);
	//wprintf(L"ErrorCode : %d, ErrorMsg : %s\n", errorCode, errorString);
}