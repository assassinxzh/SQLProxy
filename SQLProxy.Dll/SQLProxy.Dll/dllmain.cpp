// dllmain.cpp : ���� DLL Ӧ�ó������ڵ㡣
#include "stdafx.h"
#pragma warning(disable:4996)
#include <winsock.h>
#include "DebugLog.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		//��ʼ��windows�����绷��----------------------------
		WSADATA wsaData;
		WSAStartup(0x0101, &wsaData);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		WSACleanup(); //������绷��
		break;
	}
	return TRUE;
}

