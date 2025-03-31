#include "PtyProcess.h"

namespace Upp {

#define LLOG(x)	// RLOG("PtyProcess [WIN32]: " << x);
	
#ifdef PLATFORM_WIN32

String sParseArgs(const char *cmd, const Vector<String> *pargs)
{
	while(*cmd && (byte) *cmd <= ' ') ++cmd;

	if(!pargs)
		return cmd;

	String cmdh;
	cmdh = cmd;
	for(int i = 0; i < pargs->GetCount(); i++) {
		cmdh << ' ';
		String argument = (*pargs)[i];
		if(argument.GetCount() && argument.FindFirstOf(" \t\n\v\"") < 0)
			cmdh << argument;
		else {
			cmdh << '\"';
			const char *s = argument;
			for(;;) {
				int num_backslashes = 0;
				while(*s == '\\') {
					s++;
					num_backslashes++;
				}
				if(*s == '\0') {
					cmdh.Cat('\\', 2 * num_backslashes);
					break;
				}
				else
				if(*s == '\"') {
					cmdh.Cat('\\', 2 * num_backslashes + 1);
					cmdh << '\"';
				}
				else {
					cmdh.Cat('\\', num_backslashes);
					cmdh.Cat(*s);
				}
				s++;
			}
			cmdh << '\"';
		}
	}
	return cmdh;
}

Vector<WCHAR> sEnvtoWCHAR(const char *envptr)
{
	Vector<WCHAR> env;
	
	if(envptr) {
		int len = 0;
		while(envptr[len] || envptr[len + 1]) len++;
		if(len) {
			env = ToUtf16(envptr, len);
			env.Add(0);
		}
	}
	
	env.Add(0);
	return env;
}

bool APtyProcess::Start(const char *cmdline, const VectorMap<String, String>& env, const char *cd)
{
	String senv;
	for(int i = 0; i < env.GetCount(); i++)
		senv << env.GetKey(i) << "=" << env[i] << '\0';
	return DoStart(cmdline, nullptr, ~senv, cd);
}

HANDLE WinPtyCreateProcess(const char *cmdptr, const char *envptr, const char *cd, winpty_t* hConsole)
{
	Vector<WCHAR> cmd = ToSystemCharsetW(cmdptr);
	cmd.Add(0);

	auto hSpawnConfig = winpty_spawn_config_new(
		WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
		cmd,
		nullptr,
		cd ? ToSystemCharsetW(cd).begin() : nullptr,
		sEnvtoWCHAR(envptr),
		nullptr);
		
	if(!hSpawnConfig) {
		LLOG("winpty_spawn_config_new() failed.");
		return nullptr;
	}
	
	HANDLE hProcess = nullptr;
	
	auto success = winpty_spawn(
		hConsole,
		hSpawnConfig,
		&hProcess,
		nullptr,
		nullptr,
		nullptr);

	winpty_spawn_config_free(hSpawnConfig);
	
	if(!success) {
		LLOG("winpty_spawn() failed.");
		return nullptr;
	}

	return hProcess;
}

void WindowsPtyProcess::Init()
{
	hProcess       = nullptr;
	hOutputRead    = nullptr;
	hErrorRead     = nullptr;
	hInputWrite    = nullptr;
	cSize          = Null;
	convertcharset = false;
	exit_code      = Null;
}

void WindowsPtyProcess::Free()
{
	if(hProcess) {
		CloseHandle(hProcess);
		hProcess = nullptr;
	}

	if(hOutputRead) {
		CloseHandle(hOutputRead);
		hOutputRead = nullptr;
	}

	if(hErrorRead) {
		CloseHandle(hErrorRead);
		hErrorRead = nullptr;
	}

	if(hInputWrite) {
		CloseHandle(hInputWrite);
		hInputWrite = nullptr;
	}
}

bool WindowsPtyProcess::DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd)
{
	Kill();
	exit_code = Null;

	String command = sParseArgs(cmd, args);
	if(command.IsEmpty()) {
		LLOG("Couldn't parse arguments.");
		Free();
		return false;
	}
	return true;
}


void WindowsPtyProcess::Kill()
{
	if(hProcess && IsRunning()) {
		TerminateProcess(hProcess, (DWORD)-1);
		exit_code = 255;
	}
	Free();
}

bool WindowsPtyProcess::IsRunning()
{
	dword exitcode;
	if(!hProcess)
		return false;
	if(GetExitCodeProcess(hProcess, &exitcode) && exitcode == STILL_ACTIVE)
		return true;
	dword n;
	if(PeekNamedPipe(hOutputRead, nullptr, 0, nullptr, &n, nullptr) && n)
		return true;
	exit_code = exitcode;
	LLOG("IsRunning() -> no, just exited, exit code = " << exit_code);
	return false;
}

int WindowsPtyProcess::GetExitCode()
{
	return IsRunning() ? (int) Null : exit_code;
}

bool WindowsPtyProcess::Read(String& s)
{
	String rread;
	constexpr const DWORD BUFSIZE = 4096;

	s = rbuffer;
	rbuffer.Clear();
	bool running = IsRunning();
	char buffer[BUFSIZE];
	DWORD n = 0;
	
	for(HANDLE hPipe : { hOutputRead, hErrorRead }) {
		while(hPipe
			&& PeekNamedPipe(hPipe, nullptr, 0, nullptr, &n, nullptr) && n
			&& ReadFile(hPipe, buffer, min(n, BUFSIZE), &n, nullptr) && n)
				rread.Cat(buffer, n);
	}

	LLOG("Read() -> " << rread.GetLength() << " bytes read.");

	if(!IsNull(rread)) {
		s << (convertcharset ? FromOEMCharset(rread) : rread);
	}

	return !IsNull(rread) && running;
}

void WindowsPtyProcess::Write(String s)
{
	if(IsNull(s) && IsNull(wbuffer))
		return;
	if(convertcharset)
		s = ToSystemCharset(s);
	wbuffer.Cat(s);
	dword done = 0;
	if(hInputWrite) {
		bool ret = true;
		dword n = 0;
		for(int wn = 0; ret && wn < wbuffer.GetLength(); wn += n) {
			ret = WriteFile(hInputWrite, ~wbuffer, min(wbuffer.GetLength(), 4096), &n, nullptr);
			done += n;
			if(n > 0)
				wbuffer.Remove(0, n);
		}
	}
	LLOG("Write() -> " << done << "/" << wbuffer.GetLength() << " bytes.");
}

HANDLE WindowsPtyProcess::GetProcessHandle() const
{
	return hProcess;
}

void WinPtyProcess::Init()
{
	WindowsPtyProcess::Init();

	hConsole = nullptr;
}

void WinPtyProcess::Free()
{
	WindowsPtyProcess::Free();
	
	if(hConsole) {
		winpty_free(hConsole);
		hConsole = nullptr;
	}

}

bool WinPtyProcess::DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd)
{
	if(!WindowsPtyProcess::DoStart(cmd, args, env, cd))
		return false;

	auto hAgentConfig = winpty_config_new(0, nullptr);
	if(!hAgentConfig) {
		LLOG("winpty_config_new() failed.");
		Free();
		return false;
	}

	winpty_config_set_initial_size(hAgentConfig, 80, 24);
		
	hConsole = winpty_open(hAgentConfig, nullptr);
	winpty_config_free(hAgentConfig);
	if(!hConsole) {
		LLOG("winpty_open() failed.");
		Free();
		return false;
	}
	
	hInputWrite = CreateFileW(
		winpty_conin_name(hConsole),
		GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);

	hOutputRead = CreateFileW(
		winpty_conout_name(hConsole),
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);
	
	hErrorRead = CreateFileW(
		winpty_conerr_name(hConsole),
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr);

	if(!hInputWrite || !hOutputRead || !hErrorRead) {
		LLOG("Couldn't create file I/O handles.");
		Free();
		return false;
	}
	
	hProcess = WinPtyCreateProcess(cmd, env, cd, hConsole);
	if(!hProcess) {
		LLOG("WinPtyCreateProcess() failed.");
		Free();
		return false;
	}

	return true;
}

Size WinPtyProcess::GetSize()
{
	if(hConsole && !IsNull(cSize)) {
		LLOG("Fetched pty size: " << cSize);
		return cSize;
	}
	LLOG("Couldn't fetch pty size!");
	return Null;
}

bool WinPtyProcess::SetSize(Size sz)
{
	if(hConsole) {
		if(winpty_set_size(hConsole, max(2, sz.cx), max(2, sz.cy), nullptr)) {
			LLOG("Pty size is set to: " << sz);
			cSize = sz;
			return true;
		}
	}
	cSize = Null;
	LLOG("Couldn't set pty size!");
	return false;
}

#if defined(flagWIN10)

ConPtyDll::ConPtyDll()
: hConPtyLib(nullptr)
, pCreate(nullptr)
, pClose(nullptr)
, pResize(nullptr)
{
}

ConPtyDll::~ConPtyDll()
{
	if(hConPtyLib)
		FreeLibrary(hConPtyLib);
}

bool ConPtyDll::Init()
{
	// First try loading the rewritten version of conpty.dll (uses OpenConsole.exe)

	if(hConPtyLib = LoadLibraryW(L"conpty.dll"); !hConPtyLib) {
		LLOG("Warning: Couldn't load the new conpty.dll. Falling back to the default API.");
		if(hConPtyLib = LoadLibraryW(L"kernel32.dll"); !hConPtyLib) {
			LLOG("Failed to load kernel32.dll. Error: " << GetLastError());
			return false;
		}
	}
	
	LLOG("ConPty API is succesfully initialized.");


	// Get function addresses
	pCreate = reinterpret_cast<create_t>(GetProcAddress(hConPtyLib, "CreatePseudoConsole"));
	pClose  = reinterpret_cast<close_t>(GetProcAddress(hConPtyLib, "ClosePseudoConsole"));
	pResize = reinterpret_cast<resize_t>(GetProcAddress(hConPtyLib, "ResizePseudoConsole"));
	if(!pCreate || !pClose || !pResize) {
		LLOG("Failed to retrieve ConPty function addresses. Error: " << GetLastError());
		FreeLibrary(hConPtyLib);
		hConPtyLib = nullptr;
		return false;
	}

	return true;
}

HRESULT ConPtyDll::Create(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON *phPC)
{
	return pCreate ? pCreate(size, hInput, hOutput, dwFlags, phPC) : E_FAIL;
}

void ConPtyDll::Close(HPCON hPC)
{
	if(pClose)
		pClose(hPC);
}

HRESULT ConPtyDll::Resize(HPCON hPC, COORD size)
{
	return pResize ? pResize(hPC, size) : E_FAIL;
}

bool Win32CreateProcess(const char *cmdptr, const char *envptr, STARTUPINFOEX& si, PROCESS_INFORMATION& pi, const char *cd)
{
	Vector<WCHAR> cmd = ToSystemCharsetW(cmdptr);
	cmd.Add(0);
	
	return CreateProcessW(
		nullptr,
		cmd,
		nullptr,
		nullptr,
		FALSE,
		EXTENDED_STARTUPINFO_PRESENT,
		(void *) envptr,
		cd ? ToSystemCharsetW(cd).begin() : nullptr,
		(LPSTARTUPINFOW) &si.StartupInfo,
		&pi);
}

void ConPtyProcess::Init()
{
	WindowsPtyProcess::Init();

	hConsole       = nullptr;
	hProcAttrList  = nullptr;
}

void ConPtyProcess::Free()
{
	WindowsPtyProcess::Free();

	if(hConsole) {
		conptylib.Close(hConsole);
		hConsole = nullptr;
	}
	if(hProcAttrList) {
		DeleteProcThreadAttributeList(hProcAttrList);
		hProcAttrList = nullptr;
	}
}

bool ConPtyProcess::DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd)
{
	if(!WindowsPtyProcess::DoStart(cmd, args, env, cd) || !conptylib.Init())
		return false;

	
	HANDLE hOutputReadTmp, hOutputWrite;
	HANDLE hInputWriteTmp, hInputRead;
	HANDLE hErrorWrite;

	HANDLE hp = GetCurrentProcess();

	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = nullptr;
	sa.bInheritHandle = TRUE;

	CreatePipe(&hInputRead, &hInputWriteTmp, &sa, 0);
	CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0);
	
	DuplicateHandle(hp, hInputWriteTmp, hp, &hInputWrite, 0, FALSE, DUPLICATE_SAME_ACCESS);
	DuplicateHandle(hp, hOutputReadTmp, hp, &hOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS);
	DuplicateHandle(hp, hOutputWrite,   hp, &hErrorWrite, 0, TRUE,  DUPLICATE_SAME_ACCESS);
	
	CloseHandle(hInputWriteTmp);
	CloseHandle(hOutputReadTmp);

	COORD size;
	size.X = 80;
	size.Y = 24;
	
	if(conptylib.Create(size, hInputRead, hOutputWrite, 0, &hConsole) != S_OK) {
		LLOG("CreatePseudoConsole() failed.");
		Free();
		return false;
	}

	PROCESS_INFORMATION pi;
	STARTUPINFOEX si;
	ZeroMemory(&si, sizeof(STARTUPINFOEX));
	si.StartupInfo.cb = sizeof(STARTUPINFOEX);

	size_t listsize;  // FIXME: Use Upp's allocators (or Upp::Buffer<>) here.
	InitializeProcThreadAttributeList(nullptr, 1, 0, (PSIZE_T) &listsize);
	hProcAttrList = (PPROC_THREAD_ATTRIBUTE_LIST) HeapAlloc(GetProcessHeap(), 0, listsize);
	if(!hProcAttrList) {
		LLOG("HeapAlloc(): Out of memory error.");
		Free();
		return false;
	}

	if(!InitializeProcThreadAttributeList(hProcAttrList, 1, 0, (PSIZE_T) &listsize)) {
		LLOG("InitializeProcThreadAttributeList() failed.");
		Free();
		return false;
	}

	if(!UpdateProcThreadAttribute(
			hProcAttrList,
			0,
			PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
			hConsole,
			sizeof(hConsole),
			nullptr,
			nullptr)) {
		LLOG("UpdateProcThreadAttribute() failed.");
		Free();
		return false;
	}

	si.lpAttributeList        = hProcAttrList;
	si.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
	si.StartupInfo.hStdInput  = hInputRead;
	si.StartupInfo.hStdOutput = hOutputWrite;
	si.StartupInfo.hStdError  = hErrorWrite;

	bool h = Win32CreateProcess(cmd, env, si, pi, cd);

	CloseHandle(hErrorWrite);
	CloseHandle(hInputRead);
	CloseHandle(hOutputWrite);

	if(h) {
		hProcess = pi.hProcess;
		CloseHandle(pi.hThread);
	}
	else {
		LLOG("Win32CreateProcess() failed.");
		Free();
		return false;
	}

	return true;
}

Size ConPtyProcess::GetSize()
{
	if(hConsole && !IsNull(cSize)) {
		LLOG("Fetched pty size: " << cSize);
		return cSize;
	}
	LLOG("Couldn't fetch pty size!");
	return Null;
}

bool ConPtyProcess::SetSize(Size sz)
{
	if(hConsole) {
		COORD size;
		size.X = (SHORT) max(2, sz.cx);
		size.Y = (SHORT) max(2, sz.cy);
		if(conptylib.Resize(hConsole, size) == S_OK) {
			LLOG("Pty size is set to: " << sz);
			cSize = sz;
			return true;
		}
	}
	cSize = Null;
	LLOG("Couldn't set pty size!");
	return false;
}

#endif

#endif

}