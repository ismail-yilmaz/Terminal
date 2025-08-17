#include <Turtle/Turtle.h>
#include <Terminal/Terminal.h>
#include <PtyProcess/PtyProcess.h>

// This example demonstrates a simple, cross-platform (POSIX/Windows)
// terminal example, running on the U++ Turtle backend.Turtle allows
// U++ GUI applications to run on modern web browsers  that  support
// HTML-5 canvas and websockets. Turtle can be switched on or off by
// a compile-time flag.

#ifdef PLATFORM_POSIX
const char *tshell = "SHELL";
#elif PLATFORM_WIN32
const char *tshell = "ComSpec"; // Alternatively you can use powershell...
#endif

using namespace Upp;

struct TerminalExample : TopWindow {
	TerminalCtrl  term;
	PtyProcess    pty;  // This class is completely optional
	
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


void AppMainLoop()
{
	// "Main" stuff should go in here...
	TerminalExample().Run();
}

CONSOLE_APP_MAIN
{

#ifdef _DEBUG
	TurtleServer::DebugMode();
#endif

	// MemoryLimitKb(100000000); // Can aid preventing DDoS attacks.

	TurtleServer guiserver;
	guiserver.Host("localhost");
	guiserver.HtmlPort(8888);
	guiserver.MaxConnections(15);
	RunTurtleGui(guiserver, AppMainLoop);
}