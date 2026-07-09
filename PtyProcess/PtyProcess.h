#ifndef _Upp_PtyProcess_h_
#define _Upp_PtyProcess_h_

#include <Core/Core.h>

#ifdef PLATFORM_POSIX
    #include <sys/ioctl.h>
    #include <sys/wait.h>
    #include <termios.h>
    #include <poll.h>
#elif PLATFORM_WIN32
    #include <windows.h>
    #include "lib/libwinpty.h"
#endif

namespace Upp {

class APtyProcess : public Pte<APtyProcess>, public AProcess {
public:
    APtyProcess()                                                                                                    {}
    virtual ~APtyProcess()                                                                                           {}

    APtyProcess& Co(bool b = true)                  { co = b; return *this; }
    APtyProcess& NoCo()                             { return Co(false); }
    bool         IsCo() const                       { return async; }

    APtyProcess& ConvertCharset(bool b = true)      { convertcharset = b; return *this; }
    APtyProcess& NoConvertCharset()                 { return ConvertCharset(false); }

    bool         Start(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                         { return DoStart(cmdline, nullptr, env, cd); }
    bool         Start(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr) { return DoStart(cmd, args, env, cd); }
    bool         Start(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr);

    virtual Size GetSize() = 0;
    virtual bool SetSize(Size csz, Size psz) = 0;
    bool         SetSize(Size sz)                   { return SetSize(sz, Null); }
    bool         SetSize(int col, int row)          { return SetSize(Size(col, row)); }

    Event<>      WhenWakeUp;

    template<class T> bool Is() const               { return dynamic_cast<T*>(this); }
    template<class T> T& To()                       { static_assert(std::is_base_of<APtyProcess, T>::value); return static_cast<T&>(*this); }
    template<class T> const T& To() const           { static_assert(std::is_base_of<APtyProcess, T>::value); return static_cast<T&>(*this); }
    
protected:
    virtual void Init() = 0;
    virtual void Free() = 0;
    virtual bool DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) = 0;

    // Optional MT (async) support
    struct AsyncData {
        Thread            thread;
        Mutex             lock;
        ConditionVariable cv;
        String            buffer;
        bool              stop:1;
        bool              eof:1;
        AsyncData() : stop(false), eof(false) {}
    };

    One<AsyncData>        async;
    bool                  co:1;
    bool                  convertcharset:1;
    int                   exit_code;
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
    bool        SetSize(Size csz, Size psz = Null) final;

    bool        SetAttrs(const termios& t);
    bool        GetAttrs(termios& t);
    Gate<termios&> WhenAttrs;

    int          GetPid() const                      { return pid; }
    int          GetSocket() const                   { return master; }

    void         Kill() final;
    bool         IsRunning() final;
    int          GetExitCode() final;
    
    bool         Read(String& s) final;
    void         Write(String s) final;

private:
    void        Init() final;
    void        Free() final;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) final;

    void        StartAsync();
    void        StopAsync();
    void        DrainAsync();
    
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

template <>
inline constexpr bool is_upp_guest<pollfd> = true;

#elif defined(PLATFORM_WIN32)

class WindowsPtyProcess : public APtyProcess {
    friend class PtyWaitEvent;
public:
    WindowsPtyProcess()                                                                                                    { Init(); }
    virtual ~WindowsPtyProcess()                                                                                           { Kill(); }

    void        Kill() override;
    bool        IsRunning() override;
    int         GetExitCode() override;
    
    bool        Read(String& s) override;
    void        Write(String s) override;

    HANDLE      GetProcessHandle() const;

protected:
    void        Init() override;
    void        Free() override;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) override;

    void        StartAsync();
    void        StopAsync();
    void        DrainAsync();
    
    HANDLE      hProcess;
    HANDLE      hOutputRead;
    HANDLE      hErrorRead;
    HANDLE      hInputWrite;
    String      rbuffer, wbuffer;
    Size        cSize;
};

class WinPtyProcess : public WindowsPtyProcess {
public:
    WinPtyProcess()                                                                                                    { Init(); }
    WinPtyProcess(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr)                 { Init(); APtyProcess::Start(cmdline, env, cd); }
    WinPtyProcess(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                            { Init(); APtyProcess::Start(cmdline, nullptr, env, cd); }
    WinPtyProcess(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr)    { Init(); APtyProcess::Start(cmd, args, env, cd); }
    virtual ~WinPtyProcess()                                                                                           { Kill(); }

    Size         GetSize() final;
    bool         SetSize(Size csz, Size psz = Null) final;

private:
    void        Init() final;
    void        Free() final;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) final;

    winpty_t*   hConsole;
};

#if defined(flagWIN10)

// Enable the newest ConPty library if available.
// We can now use/ship the conpty.dll + OpenConsole.exe (as does Windows Terminal)

class ConPtyDll {
public:
    ConPtyDll();
    virtual ~ConPtyDll();
    
    bool        Init();
    HRESULT     Create(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
    void        Close(HPCON hPC);
    HRESULT     Resize(HPCON hPC, COORD size);
    
private:
    HMODULE     hConPtyLib;

    // Function pointer types
    typedef     HRESULT(WINAPI* create_t)(COORD size, HANDLE hInput, HANDLE hOutput, DWORD dwFlags, HPCON* phPC);
    typedef     VOID(WINAPI* close_t)(HPCON hPC);
    typedef     HRESULT(WINAPI* resize_t)(HPCON hPC, COORD size);

    // Function pointers
    create_t    pCreate;
    close_t     pClose;
    resize_t    pResize;
};

class ConPtyProcess : public WindowsPtyProcess {
public:
    ConPtyProcess()                                                                                                    { Init(); }
    ConPtyProcess(const char *cmdline, const VectorMap<String, String>& env, const char *cd = nullptr)                 { Init(); APtyProcess::Start(cmdline, env, cd); }
    ConPtyProcess(const char *cmdline, const char *env = nullptr, const char *cd = nullptr)                            { Init(); APtyProcess::Start(cmdline, nullptr, env, cd); }
    ConPtyProcess(const char *cmd, const Vector<String> *args, const char *env = nullptr, const char *cd = nullptr)    { Init(); APtyProcess::Start(cmd, args, env, cd); }
    virtual ~ConPtyProcess()                                                                                           { Kill(); }

    Size         GetSize() final;
    bool         SetSize(Size csz, Size psz = Null) final;

private:
    void        Init() final;
    void        Free() final;
    bool        DoStart(const char *cmd, const Vector<String> *args, const char *env, const char *cd) final;

    ConPtyDll   conptylib;
    HPCON       hConsole;
    PPROC_THREAD_ATTRIBUTE_LIST hProcAttrList;
};
using PtyProcess = ConPtyProcess;

#else

using PtyProcess = WinPtyProcess;

#endif

#endif

class PtyWaitEvent : NoCopy {
public:
    PtyWaitEvent();
    ~PtyWaitEvent();
    
    void            Clear();
    void            Add(APtyProcess& pty, dword events);
    void            Remove(APtyProcess& pty);
    bool            Wait(int timeout);
    dword           Get(int i) const;
    dword           operator[](int i) const;
    
    void            WakeUp();
    
private:

#ifdef PLATFORM_WIN32

    struct Slot : Moveable<Slot> {
        Slot();
        ~Slot();
        const  APtyProcess* ptyptr = nullptr;
        HANDLE hProcess;
        HANDLE hRead;
        HANDLE hWrite;
        HANDLE hError;
        bool   eRead;
        bool   eWrite;
        bool   eError;
        bool   eException;
        dword  events;
    };
    
    Vector<Slot> slots;
    HANDLE hWakeUpEvent;
    
#elif PLATFORM_POSIX

    Vector<pollfd> slots;
    int wakeuppipe[2];

#endif
};

}

#endif