#include <Windows.h>
#include <Shellapi.h>
#include <Commctrl.h>
#include <strsafe.h>
#include <wchar.h>
#include "resource.h"

#define CLASSNAME L"DoubleClickWnd"
#define APPNAME L"Double Click Fixer"

#define DOUBLE_CLICK_THRESHOLD_MS 50

#define ID_TRAYICON 1001
#define SWM_TRAYMSG WM_APP
#define SWM_DISABLED WM_APP + 1
#define SWM_EXIT WM_APP + 2

NOTIFYICONDATA niData;
HHOOK hMouseHook;
HWND gHWnd = NULL;

unsigned __int64 gTimerFrequency = 0;
unsigned __int64 gDoubleClickThresholdMs = 0;
unsigned __int64 gDoubleClickThreshold = 0;
unsigned int gBlockedDblClick = 0;
unsigned __int64 gMinDblClickElapsed = 0;

// faulty double click = elapsed time between last mouseup - mousedown < THRESHOLD

void CDECL Trace(PCSTR pszFormat, ...)
{
    CHAR szTrace[1024];

    va_list args;
    va_start(args, pszFormat);
    (void) StringCchVPrintfA(szTrace, ARRAYSIZE(szTrace), pszFormat, args);
    va_end(args);

    OutputDebugStringA(szTrace);
}

void Error(const wchar_t *message)
{
    MessageBox(gHWnd, message, APPNAME, MB_OK | MB_ICONEXCLAMATION);
}

__declspec(dllexport) LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static bool blocked = false;
    static unsigned __int64 previousClick = 0;

    if (nCode == HC_ACTION)
    {
        if (wParam == WM_LBUTTONDOWN)
        {
            unsigned __int64 currentTime;
            QueryPerformanceCounter((LARGE_INTEGER *) &currentTime);
            unsigned __int64 elapsed = currentTime - previousClick;
            // Trace("LBUTTONDOWN elapsed = %I64u", elapsed);

            if (gMinDblClickElapsed == 0 || elapsed < gMinDblClickElapsed)
                gMinDblClickElapsed = elapsed;

            if (!blocked && elapsed < gDoubleClickThreshold)
            {
                Trace("LBUTTONDOWN blocked, elapsed time = %I64u", currentTime - previousClick);
                blocked = true;
                ++gBlockedDblClick;
                return 1;
            } else if (blocked)
                blocked = false;
        }
        else if (wParam == WM_LBUTTONUP)
        {
            if (blocked) {
                blocked = false;
                return 1;
            }
            else
                QueryPerformanceCounter((LARGE_INTEGER *) &previousClick);
        }
    }

    return CallNextHookEx(hMouseHook, nCode,wParam,lParam);
}

void ShowContextMenu(HWND hWnd)
{
    SetForegroundWindow(hWnd);

    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (hMenu)
    {
        wchar_t buffer[1024];

        swprintf(buffer, sizeof(buffer), L"Double click threshold: %I64u ms", gDoubleClickThresholdMs);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, SWM_DISABLED, buffer);
        swprintf(buffer, sizeof(buffer), L"Blocked %u double click(s)", gBlockedDblClick);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, SWM_DISABLED, buffer);
        swprintf(buffer, sizeof(buffer), L"Minimum delta: %.2f ms", 1000.0 * gMinDblClickElapsed / gTimerFrequency);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, SWM_DISABLED, buffer);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, L"");
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, SWM_EXIT, L"Exit");

        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hMenu);
    }
    else
        Error(L"ShowContextMenu failed");

}

LRESULT CALLBACK WndMainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    int wmId, wmEvent;
    switch (message) {
        case SWM_TRAYMSG:
            switch (lParam)
            {
                case WM_RBUTTONUP:
                    ShowContextMenu(hWnd);
                    break;
            }
            break;
        case WM_COMMAND:
            wmId = LOWORD(wParam);
            // wmEvent = HIWORD(wParam);
            switch (wmId)
            {
                case SWM_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            niData.uFlags = 0;
            Shell_NotifyIcon(NIM_DELETE, &niData);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    // parse threshold value from command line
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    gDoubleClickThresholdMs = argc > 1 ? _wtoi(argv[1]) : DOUBLE_CLICK_THRESHOLD_MS;
    if (gDoubleClickThresholdMs == 0)
    {
        Error(L"Invalid value for double click threshold");
        return 1;
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC  = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASS wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndMainProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = CLASSNAME;

    if (!RegisterClass(&wc))
    {
        Error(L"RegisterClass failed");
        return false;
    }

    gHWnd = CreateWindow(CLASSNAME, APPNAME,
            WS_CAPTION | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            0, 0, hInstance, 0);
    if (!gHWnd)
    {
        Error(L"CreateWindow failed");
        return false;
    }


    QueryPerformanceFrequency((LARGE_INTEGER *) &gTimerFrequency);
    gDoubleClickThreshold = gDoubleClickThresholdMs * gTimerFrequency / 1000;
    Trace("Timer frequency = %I64u, double click threshold = %I64u", gTimerFrequency, gDoubleClickThreshold);

    ZeroMemory(&niData, sizeof(NOTIFYICONDATA));
    niData.cbSize = sizeof(NOTIFYICONDATA);
    niData.uID = ID_TRAYICON;
    niData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
    niData.hIcon = (HICON) LoadImage(hInstance, MAKEINTRESOURCE(IDI_TRAYICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    niData.hWnd = gHWnd;
    wcscpy(niData.szTip, L"Double Click Fixer");
    niData.uCallbackMessage = SWM_TRAYMSG;

    Shell_NotifyIcon(NIM_ADD, &niData);

    if (niData.hIcon && DestroyIcon(niData.hIcon))
        niData.hIcon = NULL;

    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC) MouseProc, hInstance, NULL);

    MSG msg;
    BOOL ret;
    while ((ret = GetMessage(&msg, (HWND) NULL, 0, 0)) != 0 && ret != -1)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMouseHook);

    return msg.wParam;
}
