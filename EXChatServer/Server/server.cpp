#include "framework.h"
#include "server.h"


SERVERCONTEXT					Server;

LRESULT CALLBACK CGui::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
	}
	break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

CGui::CGui()
{
}

CGui::~CGui()
{
}

int CGui::CreateGui(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASS				wc;
	char					cTitle[] = "BNet Server",
							cClass[] = "_Gui_Class_";


	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL; //LoadIcon(hInstance, (LPCTSTR)IDI_TTT);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)BLACK_BRUSH;
	wc.lpszMenuName = NULL; //(LPCSTR)IDC_TTT;
	wc.lpszClassName = cClass;
	RegisterClass(&wc);

	m_hInstance = hInstance;

	m_hWnd = CreateWindow(cClass, cTitle, WS_OVERLAPPEDWINDOW,
		100, 100, 300, 100, NULL, NULL, hInstance, NULL);
	if (!m_hWnd) return 0;

	ShowWindow(m_hWnd, nCmdShow);
	UpdateWindow(m_hWnd);

	return 1;
}

int CGui::Initialize()
{
	return 0;
}

int CGui::DoPump()
{
	MSG						msg;


	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{

	CGui					gui;
	int						iRet;


	//	gui.CreateGui( hInstance, lpCmdLine, SW_HIDE );
	gui.CreateGui(hInstance, lpCmdLine, nCmdShow);
	gui.Initialize();
	iRet = gui.DoPump();

	return iRet;
}