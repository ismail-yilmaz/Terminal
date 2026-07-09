#include "PtyProcess.h"

namespace Upp {

#define LLOG(x)	// RLOG("PtyWaitEvent: " << x);

#ifdef PLATFORM_WIN32

namespace {

bool IsException(DWORD error)
{
	return error == ERROR_BROKEN_PIPE
		|| error == ERROR_PIPE_NOT_CONNECTED
		|| error == ERROR_OPERATION_ABORTED
		|| error == ERROR_HANDLE_EOF
		|| error == ERROR_NO_DATA
		|| error == ERROR_BAD_PIPE;
}

}

PtyWaitEvent::Slot::Slot()
: hProcess(nullptr)
, hRead(nullptr)
, hWrite(nullptr)
, hError(nullptr)
, eRead(false)
, eWrite(false)
, eError(false)
, eException(false)
, events(0)
{
}

PtyWaitEvent::Slot::~Slot()
{
}

#endif

PtyWaitEvent::PtyWaitEvent()
{
#ifdef PLATFORM_WIN32

	hWakeUpEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

#elif PLATFORM_POSIX

	if(pipe(wakeuppipe) == 0) {
		fcntl(wakeuppipe[0], F_SETFL, O_NONBLOCK);
		fcntl(wakeuppipe[1], F_SETFL, O_NONBLOCK);
	}

#endif
}

PtyWaitEvent::~PtyWaitEvent()
{
#ifdef PLATFORM_WIN32

	if(hWakeUpEvent)
		CloseHandle(hWakeUpEvent);

#elif PLATFORM_POSIX

	close(wakeuppipe[0]);
	close(wakeuppipe[1]);

#endif
}

void PtyWaitEvent::Clear()
{
	slots.Clear();
}

void PtyWaitEvent::Add(APtyProcess& pty, dword events)
{
	if(pty.IsCo())
		return;

#ifdef PLATFORM_WIN32

	const auto& p = static_cast<const WindowsPtyProcess&>(pty);

	Slot& slot      = slots.Add();
	slot.hProcess   = p.hProcess;
	slot.hRead      = p.hOutputRead;
	slot.hWrite     = p.hInputWrite;
	slot.hError     = p.hErrorRead;
	slot.eRead      = false;
	slot.eWrite     = false;
	slot.eError     = false;
	slot.events     = events;

#elif PLATFORM_POSIX

	auto& p = static_cast<const PosixPtyProcess&>(pty);
	pollfd& q = slots.Add({0});
	q.fd = p.GetSocket();
	if(events & WAIT_READ)
		q.events |= POLLIN;
	if(events & WAIT_WRITE)
		q.events |= POLLOUT;
	if(events & WAIT_IS_EXCEPTION)
		q.events |= POLLPRI;

#endif
}

void PtyWaitEvent::Remove(APtyProcess& pty)
{
	if(pty.IsCo())
		return;

#ifdef PLATFORM_WIN32

	const auto& q = static_cast<const WindowsPtyProcess&>(pty);
	slots.RemoveIf([this, &q](int i) {	return slots[i].hProcess == q.hProcess; });

#elif PLATFORM_POSIX

	const auto& q = static_cast<const PosixPtyProcess&>(pty);
	slots.RemoveIf([this, &q](int i) { return slots[i].fd == q.GetSocket(); });

#endif
}

void PtyWaitEvent::WakeUp()
{
#ifdef PLATFORM_WIN32

	if(hWakeUpEvent)
		SetEvent(hWakeUpEvent);

#elif PLATFORM_POSIX

	char c = 1;
	(void) write(wakeuppipe[1], &c, 1);

#endif
}

bool PtyWaitEvent::Wait(int timeout)
{
#ifdef PLATFORM_WIN32

	auto CheckPipe = [](HANDLE h, bool& dataFlag, bool& exceptionFlag, dword events, bool& hasEvents) -> bool {
		if(!h) return false;

		DWORD n = 0;
		if(PeekNamedPipe(h, nullptr, 0, nullptr, &n, nullptr)) {
			if(n > 0) {
				dataFlag = hasEvents = true;
				return true;
			}
		}
		else if((events & WAIT_IS_EXCEPTION) && IsException(GetLastError())) {
			exceptionFlag = hasEvents = true;
			return true;
		}
		return false;
	};

	bool hasEvents = false;

	// Initial Peek for all Sync pipes
	for(Slot& slot : slots) {
		slot.eRead = slot.eWrite = slot.eError = slot.eException = false;
		if(slot.events & WAIT_READ) {
			CheckPipe(slot.hRead, slot.eRead, slot.eException, slot.events, hasEvents);
			CheckPipe(slot.hError, slot.eError, slot.eException, slot.events, hasEvents);
		}
		if((slot.events & WAIT_WRITE) && slot.hWrite) {
			slot.eWrite = hasEvents = true;
		}
	}
	if(hasEvents)
		return true;
	// The granular wait logic to mimic POSIX behavior (roughly)
	if(timeout > 0) {
		if(slots.IsEmpty()) {
			// 100% Async Setup: Pure, 0% CPU kernel wait.
			if(hWakeUpEvent)
				WaitForSingleObject(hWakeUpEvent, timeout);
			else
				Sleep(timeout);
		}
		else {
			// Mixed Setup (Sync + Async): We must periodically peek the Sync pipes,
			// but we still want instant interruptibility for Async triggers.
			int elapsed = 0;
			const int slice = 10;
			while(elapsed < timeout) {
				int waittime = min(slice, timeout - elapsed);

				if (hWakeUpEvent) {
					if (WaitForSingleObject(hWakeUpEvent, waittime) == WAIT_OBJECT_0)
						return true; // Async thread woke us instantly!
				}
				else
					Sleep(waittime);
				elapsed += waittime;
				// Re-peek the Sync pipes after the short slice
				for(Slot& slot : slots) {
					if(slot.events & WAIT_READ) {
						CheckPipe(slot.hRead, slot.eRead, slot.eException, slot.events, hasEvents);
						CheckPipe(slot.hError, slot.eError, slot.eException, slot.events, hasEvents);
					}
				}
				if(hasEvents)
					return true;
			}
		}
	}

	return hasEvents;

#elif PLATFORM_POSIX

	pollfd& q = slots.Add();
	q.fd = wakeuppipe[0];
	q.events = POLLIN;
	q.revents = 0;

	int rc = 0;
	do {
		rc = poll((pollfd*) slots.begin(), slots.GetCount(), timeout);
	}
	while(rc == -1 && errno == EINTR);

	// Check if our event pipe woke us up
	bool triggered = (slots.Top().revents & POLLIN);

	// Remove the wakeup pipe so Get(i) indexes remain undisturbed
	slots.Drop();

	if(rc == -1)
		LLOG("poll() failed: " << strerror(errno));

	// Drain the pipe so it doesn't constantly trigger on the next loop
	if(triggered) {
		char buf[64];
		while(read(wakeuppipe[0], buf, sizeof(buf)) > 0);
	}

	return rc > 0;
#endif
}

dword PtyWaitEvent::Get(int i) const
{
#ifdef PLATFORM_WIN32

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount())
		return 0;

	const Slot& slot = slots[i];
	dword events = 0;

	if((slot.events & WAIT_READ) && (slot.eRead || slot.eError))
		events |= WAIT_READ;
	if((slot.events & WAIT_WRITE) && slot.eWrite)
		events |= WAIT_WRITE;
	if((slot.events & WAIT_IS_EXCEPTION) && slot.eException)
		events |= WAIT_IS_EXCEPTION;

	return events;

#elif PLATFORM_POSIX

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount())
		return 0;

	const pollfd& q = slots[i];
	if(q.fd < 0)
		return 0;

	dword events = 0;

	if(q.revents & POLLIN)
		events |= WAIT_READ;
	if(q.revents & POLLOUT)
		events |= WAIT_WRITE;
	if(q.revents & POLLPRI)
		events |= WAIT_IS_EXCEPTION;
	if(q.revents & (POLLERR | POLLHUP | POLLNVAL))
		events |= WAIT_IS_EXCEPTION;

	return events;
#endif
}

dword PtyWaitEvent::operator[](int i) const
{
	return Get(i);
}

}