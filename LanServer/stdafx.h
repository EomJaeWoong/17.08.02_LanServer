// stdafx.h : ���� ��������� ���� ��������� �ʴ�
// ǥ�� �ý��� ���� ���� �� ������Ʈ ���� ���� ������
// ��� �ִ� ���� �����Դϴ�.
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mstcpip.h>
#include <process.h>
#include <conio.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Winmm.lib")

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
#include "StreamQueue.h"
#include "NPacket.h"
#include "Config.h"
#include "LanServer.h"
#include "LanServerTest.h"