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
#include <Windows.h>
#include <locale.h>
#include <strsafe.h>
#include <time.h>
#include <psapi.h>
#include <dbghelp.h>
#include <crtdbg.h>
#include <tlhelp32.h>
#include <float.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Winmm.lib")

#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "ImageHlp")
#pragma comment(lib, "psapi")

#define chINRANGE(low, Num, High) (((low) <= (Num)) && ((Num) <= (High)))

// TODO: ���α׷��� �ʿ��� �߰� ����� ���⿡�� �����մϴ�.
//#include "lib\Library.h"

#include "lib\APIHook.h"
#include "lib\CrashDump.h"
#include "lib\Profiler.h"
#include "lib\SystemLog.h"

using namespace Eom;

#include "StreamQueue.h"
#include "NPacket.h"
#include "Config.h"
#include "LanServer.h"
#include "LanServerTest.h"