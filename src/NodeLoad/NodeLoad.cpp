// NodeLoad.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "resource.h"
#include "getopt.h"

typedef int (*PFN_SUM)(int, int);

#include "wke.h"
#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

typedef struct WkeCommandOptions
{
	int transparent;
	WCHAR htmlFile[MAX_PATH + sizeof(WCHAR)];
	WCHAR htmlUrl[USHRT_MAX + sizeof(WCHAR)];
	int showHelp;

} wkeCommandOptions;

class wkeApplication
{
public:
	wkeApplication()
	{
		clear();
	}
	void clear()
	{
		window = 0;
		memset(&options, 0, sizeof(options));
	}
public:
	wkeWebView window;
	wkeCommandOptions options;
};


LPCWSTR GetWorkingDirectory(LPWSTR buffer, size_t bufferSize)
{
	GetCurrentDirectoryW(bufferSize, buffer);
	wcscat(buffer, L"\\");
	return buffer;
}

LPCWSTR GetWorkingPath(LPWSTR buffer, size_t bufferSize, LPCWSTR relatedPath)
{
	WCHAR dir[MAX_PATH + 1] = { 0 };
	GetWorkingDirectory(dir, MAX_PATH);
	_snwprintf(buffer, bufferSize, L"%s%s", dir, relatedPath);
	return buffer;
}

LPCWSTR FormatWorkingPath(LPWSTR buffer, size_t bufferSize, LPCWSTR fmt, ...)
{
	WCHAR relatedPath[MAX_PATH + 1] = { 0 };
	va_list args;
	va_start(args, fmt);
	_vsnwprintf(relatedPath, MAX_PATH, fmt, args);
	va_end(args);

	return GetWorkingPath(buffer, bufferSize, relatedPath);
}

LPCWSTR GetProgramDirectory(LPWSTR buffer, size_t bufferSize)
{
	DWORD i = GetModuleFileNameW(NULL, buffer, bufferSize);

	--i;
	while (buffer[i] != '\\' && i != 0)
		--i;

	buffer[i + 1] = 0;
	return buffer;
}

LPCWSTR GetProgramPath(LPWSTR buffer, size_t bufferSize, LPCWSTR relatedPath)
{
	WCHAR dir[MAX_PATH + 1] = { 0 };
	GetProgramDirectory(dir, MAX_PATH);
	_snwprintf(buffer, bufferSize, L"%s%s", dir, relatedPath);
	return buffer;
}

LPCWSTR FormatProgramPath(LPWSTR buffer, size_t bufferSize, LPCWSTR fmt, ...)
{
	WCHAR relatedPath[MAX_PATH + 1] = { 0 };
	va_list args;
	va_start(args, fmt);
	_vsnwprintf(relatedPath, MAX_PATH, fmt, args);
	va_end(args);

	return GetProgramPath(buffer, bufferSize, relatedPath);
}
BOOL FixupHtmlFileUrl(LPCWSTR pathOption, LPWSTR urlBuffer, size_t bufferSize)
{
	WCHAR htmlPath[MAX_PATH + 1] = { 0 };

	if (pathOption[0] == 0)
	{
		do
		{
			GetWorkingPath(htmlPath, MAX_PATH, L"index.html");
			if (PathFileExistsW(htmlPath))
				break;

			GetWorkingPath(htmlPath, MAX_PATH, L"main.html");
			if (PathFileExistsW(htmlPath))
				break;

			GetWorkingPath(htmlPath, MAX_PATH, L"wkexe.html");
			if (PathFileExistsW(htmlPath))
				break;

			GetProgramPath(htmlPath, MAX_PATH, L"index.html");
			if (PathFileExistsW(htmlPath))
				break;

			GetProgramPath(htmlPath, MAX_PATH, L"main.html");
			if (PathFileExistsW(htmlPath))
				break;

			GetProgramPath(htmlPath, MAX_PATH, L"wkexe.html");
			if (PathFileExistsW(htmlPath))
				break;

			return FALSE;
		} while (0);

		swprintf_s(urlBuffer, bufferSize, L"file:///%s", htmlPath);
		return TRUE;
	}

	else//if (!wcsstr(pathOption, L"://"))
	{
		do
		{
			GetWorkingPath(htmlPath, MAX_PATH, pathOption);
			if (PathFileExistsW(htmlPath))
				break;

			GetProgramPath(htmlPath, MAX_PATH, pathOption);
			if (PathFileExistsW(htmlPath))
				break;

			return FALSE;
		} while (0);

		swprintf_s(urlBuffer, bufferSize, L"file:///%s", htmlPath);
		return TRUE;
	}

	return FALSE;
}

BOOL FixupHtmlUrl(wkeApplication* app)
{
	LPWSTR htmlOption = app->options.htmlFile;
	WCHAR htmlUrl[MAX_PATH + 1] = { 0 };

	// 包含 :// 说明是完整的URL
	if (wcsstr(htmlOption, L"://"))
	{
		wcsncpy_s(app->options.htmlUrl, MAX_PATH, htmlOption, MAX_PATH);
		return TRUE;
	}

	// 若不是完整URL，补全之
	if (FixupHtmlFileUrl(htmlOption, htmlUrl, MAX_PATH))
	{
		wcsncpy_s(app->options.htmlUrl, MAX_PATH, htmlUrl, MAX_PATH);
		return TRUE;
	}
	// 无法获得完整的URL，出错
	return FALSE;
}

typedef struct
{
	struct option_w option;
	LPCWSTR description;

} OPTIONW;


static OPTIONW s_options[] =
{
	{ { L"help",          ARG_NONE,   NULL,       L'h' }, L"显示应用程序帮助信息" },
	{ { L"file",          ARG_REQ,    NULL,       L'f' }, L"设置要打开的本地文件" },
	{ { L"urls",		  ARG_REQ,    NULL,       L'u' }, L"设置要打开的URLS地址" },
	{ { L"trans",		  ARG_OPT,    0,          L't' }, L"支持使用分层窗口透明" },
	{ { NULL,             ARG_NULL,   0,             0 }, NULL }
};

void ParseOptions(int argc, LPWSTR* argv, wkeCommandOptions* options)
{
	struct option_w longOptions[100] = { 0 };
	WCHAR shortOptions[100] = { 0 };

	WCHAR val[2] = { 0 };
	OPTIONW* opt = s_options;

	int i = 0;
	do
	{
		longOptions[i++] = opt->option;

		val[0] = opt->option.val;
		wcscat_s(shortOptions, 99, val);
		if (opt->option.has_arg)
			wcscat_s(shortOptions, 99, L":");

		++opt;
	} while (opt->description);

	do
	{
		int option = getopt_long_w(argc, argv, shortOptions, longOptions, NULL);
		if (option == -1)
			break;

		switch (option)
		{
		case L'h':
			options->showHelp = TRUE;
			break;

		case L'f':
			wcscpy_s(options->htmlFile, MAX_PATH, optarg_w);
			break;

		case L'u':
			wcscpy_s(options->htmlUrl, USHRT_MAX, optarg_w);
			break;

		case L't':
			if (!optarg_w ||
				_wcsicmp(optarg_w, L"yes") == 0 ||
				_wcsicmp(optarg_w, L"true") == 0)
			{
				options->transparent = TRUE;
			}
			else
			{
				options->transparent = FALSE;
			}
			break;
		}
	} while (1);
}

void PrintHelp()
{
	WCHAR helpString[1024] = { 0 };
	int helpLength = 0;

	OPTIONW* opt = s_options;
	do
	{
		WCHAR* buffer = (LPWSTR)helpString + helpLength;
		int capacity = 1024 - helpLength;
		helpLength += swprintf_s(buffer, capacity, L"--%s\t-%c\t-%s\n", opt->option.name, opt->option.val, opt->description);

		++opt;
	} while (opt->description);

	MessageBoxW(NULL, helpString, L"NodeLoad", MB_OK | MB_ICONINFORMATION);
}
BOOL ProcessOptions(wkeApplication* app)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	ParseOptions(argc, argv, &app->options);
	LocalFree(argv);

	return TRUE;
}

// 回调：点击了关闭、返回 true 将销毁窗口，返回 false 什么都不做。
bool HandleWindowClosing(wkeWebView webWindow, void* param)
{
	wkeApplication* app = (wkeApplication*)param;
	return IDYES == MessageBoxW(NULL, L"确定要退出程序吗？", L"wkexe", MB_YESNO | MB_ICONQUESTION);
}

// 回调：窗口已销毁
void HandleWindowDestroy(wkeWebView webWindow, void* param)
{
	wkeApplication* app = (wkeApplication*)param;
	app->window = NULL;
	PostQuitMessage(0);
}

// 回调：文档加载成功
void HandleDocumentReady(wkeWebView webWindow, void* param)
{
	wkeShowWindow(webWindow, TRUE);
}

// 回调：页面标题改变
void HandleTitleChanged(wkeWebView webWindow, void* param, const wkeString title)
{
	wkeSetWindowTitleW(webWindow, wkeGetStringW(title));
}

// 回调：创建新的页面，比如说调用了 window.open 或者点击了 <a target="_blank" .../>
wkeWebView HandleCreateView(wkeWebView webWindow, void* param, wkeNavigationType navType, const wkeString url, const wkeWindowFeatures* features)
{
	wkeWebView newWindow = wkeCreateWebWindow(WKE_WINDOW_TYPE_POPUP, NULL, features->x, features->y, features->width, features->height);
	wkeShowWindow(newWindow, true);
	return newWindow;
}
bool HandleLoadUrlBegin(wkeWebView webView, void* param, const char *url, void *job)
{
	if (strcmp(url, "http://hook.test/") == 0) {
		wkeNetSetMIMEType(job, "text/html");
		wkeNetChangeRequestUrl(job, url);
		wkeNetSetData(job, "<li>这是个hook页面</li><a herf=\"http://www.baidu.com/\">HookRequest</a>", sizeof("<li>这是个hook页面</li><a herf=\"http://www.baidu.com/\">HookRequest</a>"));
		return true;
	}
	else if (strcmp(url, "http://www.baidu.com/") == 0) {
		wkeNetHookRequest(job);
	}
	return false;
}

void HandleLoadUrlEnd(wkeWebView webView, void* param, const char *url, void *job, void* buf, int len)
{
	wchar_t *str = L"百度一下";
	wchar_t *str1 = L"HOOK一下";

	int slen = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	if (slen == 0) return;

	char utf81[100];
	::WideCharToMultiByte(CP_UTF8, 0, str, -1, &utf81[0], slen, NULL, NULL);

	slen = ::WideCharToMultiByte(CP_UTF8, 0, str1, -1, NULL, 0, NULL, NULL);
	if (slen == 0) return;

	char utf82[100];
	::WideCharToMultiByte(CP_UTF8, 0, str1, -1, &utf82[0], slen, NULL, NULL);

	const char *b = strstr(static_cast<const char*>(buf), &utf81[0]);
	memcpy((void *)b, &utf82, slen);
	return;
}
void PrintHelpAndQuit(wkeApplication* app)
{
	PrintHelp();
	PostQuitMessage(0);
}
BOOL EnableDebugPrivilege(BOOL bEnable)
{
	BOOL fOK = FALSE;
	HANDLE hToken;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) //打开进程访问令牌
	{
		//试图修改“调试”特权
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		fOK = (GetLastError() == ERROR_SUCCESS);
		CloseHandle(hToken);
	}
	return fOK;
}

typedef int (*p_fn_sum)(int x, int y);
#if defined(_WINDOWS)
int WINAPI _tWinMain(
	__in HINSTANCE hInstance,
	__in_opt HINSTANCE hPrevInstance,
	__in LPTSTR lpCmdLine,
	__in int nShowCmd
)
#else
int main()
#endif
{
	MSG msg = { 0 };
	wkeApplication app;
	wkeSettings settings;

	//EnableDebugPrivilege(TRUE);
	
	wkeStartup();
	wkeResourceInitialize();
	
	ProcessOptions(&app);

	if (app.options.showHelp)
	{
		PrintHelpAndQuit(&app);
	}
	else
	{
		memset(&settings, 0, sizeof(settings));

#if defined(WKE_BROWSER_USE_LOCAL_PROXY)
		settings.proxy.type = WKE_PROXY_SOCKS5;
		strcpy(settings.proxy.hostname, "127.0.0.1");
		settings.proxy.port = 1080;
		settings.mask |= WKE_SETTING_PROXY;
#endif
		wkeConfigure(&settings);

		if (app.options.transparent)
		{
			app.window = wkeCreateWebWindow(WKE_WINDOW_TYPE_TRANSPARENT, NULL, 0, 0, 800, 600);
		}
		else
		{
			app.window = wkeCreateWebWindow(WKE_WINDOW_TYPE_POPUP, NULL, 0, 0, 800, 600);
		}

		if (app.window)
		{
			wkeOnWindowClosing(app.window, HandleWindowClosing, &app);
			wkeOnWindowDestroy(app.window, HandleWindowDestroy, &app);
			wkeOnDocumentReady(app.window, HandleDocumentReady, &app);
			wkeOnTitleChanged(app.window, HandleTitleChanged, &app);
			wkeOnCreateView(app.window, HandleCreateView, &app);
			wkeOnLoadUrlBegin(app.window, HandleLoadUrlBegin, &app);
			wkeOnLoadUrlEnd(app.window, HandleLoadUrlEnd, &app);

			wkeMoveToCenter(app.window);

			if (*app.options.htmlUrl)
			{
				wkeLoadURLW(app.window, app.options.htmlUrl);
			}
			else
			{
				if (*app.options.htmlFile)
				{
					LONG nDataSize = 0;
					WCHAR * pWData = 0;
					FILE * pFile = _wfopen(app.options.htmlFile, L"rb");
					if (pFile)
					{
						fseek(pFile, 0, SEEK_END);
						nDataSize = ftell(pFile);
						fseek(pFile, 0, SEEK_SET);
						pWData = (WCHAR*)malloc(nDataSize * sizeof(WCHAR));
						if (pWData)
						{
							fread(pWData, nDataSize, sizeof(WCHAR), pFile);
						}
						fclose(pFile);
					}
					if (pWData)
					{
						wkeLoadHTMLW(app.window, pWData);
						free(pWData);
					}
					else
					{
						wkeLoadHTMLW(app.window, L"<html><head><title>test</title></head><body bgcolor=cyan><p style=\"background-color: #00FF00\">Testing</p><img id=\"webkit logo\" src=\"http://www.ppsbbs.tech/view/img/logo.png\" alt=\"Face\"><div style=\"border: solid blue; background: white;\" contenteditable=\"true\">div with blue border</div><ul><li>foo<li>bar<li>baz</ul></body></html>");
					}
				}
				else
				{
					wkeLoadHTMLW(app.window, L"<html><head><title>test</title></head><body bgcolor=cyan><p style=\"background-color: #00FF00\">Testing</p><img id=\"webkit logo\" src=\"http://www.ppsbbs.tech/view/img/logo.png\" alt=\"Face\"><div style=\"border: solid blue; background: white;\" contenteditable=\"true\">div with blue border</div><ul><li>foo<li>bar<li>baz</ul></body></html>");
				}
			}

			/*
			while (1)
			{
				if (PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessageW(&msg);
				}
				else
				{
					;
				}
			}
			*/

			while (GetMessageW(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
	}

	wkeFinalize();
	wkeCleanup();

    return 0;
}

