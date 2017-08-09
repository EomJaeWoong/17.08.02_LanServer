// main.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"

CLanServerTest LanServer;

int _tmain(int argc, _TCHAR* argv[])
{
	char chControlKey;

	if (!LanServer.Start(SERVER_IP, SERVER_PORT, 2, false, 100))
		return -1;

	while (1)
	{
		wprintf(L"------------------------------------------------\n");
		wprintf(L"Connect Session : %d\n", LanServer._iSessionCount);
		wprintf(L"Accept TPS : %d\n", LanServer._AcceptTPS);
		wprintf(L"Accept Total : %d\n", LanServer._AcceptTotalTPS);
		wprintf(L"RecvPacket TPS : %d\n", LanServer._RecvPacketTPS);
		wprintf(L"SendPacket TPS : %d\n", LanServer._SendPacketTPS);
		wprintf(L"PacketPool Use : %d\n", 0);
		wprintf(L"PacketPool Alloc : %d\n", LanServer._PacketPoolTPS);
		wprintf(L"------------------------------------------------\n\n");

		Sleep(999);

		if (_kbhit() != 0){
			chControlKey = _getch();
			if (chControlKey == 'q' || chControlKey == 'Q')
			{
				//------------------------------------------------
				// 종료처리
				//------------------------------------------------
				break;
			}

			else if (chControlKey == 'G' || chControlKey == 'g')
			{
				//------------------------------------------------
				// 시작처리
				//------------------------------------------------
				LanServer.Start(SERVER_IP, SERVER_PORT, 1, false, 100);
			}

			else if (chControlKey == 'S' || chControlKey == 's')
			{
				//------------------------------------------------
				// 정지처리
				//------------------------------------------------
				LanServer.Stop();
			}

			/*
			else if (chControlKey == 'P' || chControlKey == 'p')
			{
				SaveProfile();
			}
			*/
		}
	}

	return 0;
}

