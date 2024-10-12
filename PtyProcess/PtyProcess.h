#ifndef _Upp_PtyProcess_h_
#define _Upp_PtyProcess_h_

#include <Core/Core.h>

#ifdef PLATFORM_POSIX
    #include <sys/ioctl.h>
    #include <sys/wait.h>
    #include <termios.h>
#elif PLATFORM_WIN32
    #include <windows.h>
    #include "lib/libwinpty.h"
#endif

namespace Upp {

class APtyProcess : public AProcess {
public:
    APtyProcess()                                                                                                    {}
    virtual ~APtyProcess()                                                                                           {}

    APtyProcess& ConvertCharset(bool b = true)          { convertcharset = b; return *this; }
    APtyProcess& NoConvertCharset()                     { return ConvertCharset(false); }

    bool         Start(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                         { return DoStart(cmdline, nullptr, env, cd); }
    bool         Start(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr) { return DoStart(cmd, args, env, cd); }
    bool         Start(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr);

    virtual Size GetSize() = 0;
    virtual bool SetSize(Size sz) = 0;
    bool         SetSize(int col, int row)          { return SetSize(Size(col, row)); }

    template<class T> bool Is() const               { return dynamic_cast<T*>(this); }
    template<class T> T& To()                       { static_assert(std::is_base_of<APtyProcess, T>::value); return static_cast<T&>(*this); }
    template<class T> const T& To() const           { static_assert(std::is_base_of<APtyProcess, T>::value); return static_cast<T&>(*this); }

protected:
    virtual void Init() = 0;
    virtual void Free() = 0;
    virtual bool DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) = 0;

    int         exit_code;
    bool        convertcharset:1;
};

#if defined(PLATFORM_POSIX)

class PosixPtyProcess : public APtyProcess {
public:
    PosixPtyProcess()                                                                                                    { Init(); }
    PosixPtyProcess(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr)                 { Init(); APtyProcess::Start(cmdline, env, cd); }
    PosixPtyProcess(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                            { Init(); APtyProcess::Start(cmdline, nullptr, env, cd); }
    PosixPtyProcess(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr)    { Init(); APtyProcess::Start(cmd, args, env, cd); }
    virtual ~PosixPtyProcess()                                                                                           { Kill(); }

    Size        GetSize() final;
    bool        SetSize(Size sz) final;

    bool        SetAttrs(const termios& t);
    bool        GetAttrs(termios& t);
    Gate<termios&> WhenAttrs;

    int          GetPid() const                      { return pid; }

    void         Kill() final;
    bool         IsRunning() final;
    int          GetExitCode() final;
    
    bool         Read(String& s) final;
    void         Write(String s) final;

private:
    void        Init() final;
    void        Free() final;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) final;

    bool        ResetSignals();
    bool        Wait(dword event, int ms = 10);
    bool        DecodeExitCode(int status);

    int         master, slave;
    pid_t       pid;
    String      exit_string;
    String      sname;
    String      wbuffer;
};

using PtyProcess = PosixPtyProcess;

#elif defined(PLATFORM_WIN32)

class Win32PtyProcess : public APtyProcess {
    public:
    Win32PtyProcess()                                                                                                    { Init(); }
    virtual ~Win32PtyProcess()                                                                                           { Kill(); }

    void         Kill() override;
    bool         IsRunning() override;
    int          GetExitCode() override;
    
    bool         Read(String& s) override;
    void         Write(String s) override;

    HANDLE      GetProcessHandle() const;
    
protected:
    void        Init() override;
    void        Free() override;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) override;

    HANDLE      hProcess;
    HANDLE      hOutputRead;
    HANDLE      hErrorRead;
    HANDLE      hInputWrite;
    String      rbuffer, wbuffer;
    Size        cSize;
};

class WinPtyProcess : public Win32PtyProcess {
public:
    WinPtyProcess()                                                                                                    { Init(); }
    WinPtyProcess(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr)                 { Init(); APtyProcess::Start(cmdline, env, cd); }
    WinPtyProcess(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                            { Init(); APtyProcess::Start(cmdline, nullptr, env, cd); }
    WinPtyProcess(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr)    { Init(); APtyProcess::Start(cmd, args, env, cd); }
    virtual ~WinPtyProcess()                                                                                           { Kill(); }

    Size         GetSize() final;
    bool         SetSize(Size sz) final;

private:
    void        Init() final;
    void        Free() final;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) final;

    winpty_t*   hConsole;
};

#if defined(flagWIN10)

class ConPtyProcess : public Win32PtyProcess {
public:
    ConPtyProcess()                                                                                                    { Init(); }
    ConPtyProcess(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr)                 { Init(); APtyProcess::Start(cmdline, env, cd); }
    ConPtyProcess(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                            { Init(); APtyProcess::Start(cmdline, nullptr, env, cd); }
    ConPtyProcess(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr)    { Init(); APtyProcess::Start(cmd, args, env, cd); }
    virtual ~ConPtyProcess()                                                                                           { Kill(); }

    Size         GetSize() final;
    bool         SetSize(Size sz) final;

private:
    void        Init() final;
    void        Free() final;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) final;

    HPCON       hConsole;
    PPROC_THREAD_ATTRIBUTE_LIST hProcAttrList;
};
using PtyProcess = ConPtyProcess;

#else

using PtyProcess = WinPtyProcess;

#endif

#endif

}

#endif