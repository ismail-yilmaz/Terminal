#include <Terminal/Terminal.h>
#include <PtyProcess/PtyProcess.h>

using namespace Upp;

// This example demonstrates a simple, cross-platform (POSIX/Windows)
// terminal example.

#ifdef PLATFORM_POSIX
const char *tshell = "SHELL";
#elif PLATFORM_WIN32
const char *tshell = "ComSpec"; // Alternatively you can use powershell...
#endif

struct TerminalExample : TopWindow {
	TerminalCtrl term;
	PtyProcess   pty;
	
	void Run()
	{
		SetRect(term.GetStdSize());	// 80 x 24 cells (scaled).
		Sizeable().Zoomable().CenterScreen().Add(term.SizePos());
		term.WhenBell   = [=]()                { BeepExclamation();  };
		term.WhenTitle  = [=](String s)        { Title(s);           };
		term.WhenOutput = [=](String s)        { pty.Write(s);       };
		term.WhenLink   = [=](const String& s) { PromptOK(DeQtf(s)); };
		term.WhenResize = [=]()                { pty.SetSize(term.GetPageSize()); };
		term.InlineImages().Hyperlinks().WindowOps();
		pty.Start(GetEnv(tshell), Environment(), GetHomeDirectory());
		PtyWaitEvent we;
		we.Add(pty, WAIT_READ | WAIT_IS_EXCEPTION);
		OpenMain();
		while(IsOpen() && pty.IsRunning()) {
			if(we.Wait(10))
				term.WriteUtf8(pty.Get());
			ProcessEvents();
		}
	}
};

GUI_APP_MAIN
{
	TerminalExample().Run();
}