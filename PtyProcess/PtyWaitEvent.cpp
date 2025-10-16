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

void PtyWaitEvent::Clear()
{
	slots.Clear();
}

void PtyWaitEvent::Add(const APtyProcess& pty, dword events)
{
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

void PtyWaitEvent::Remove(const APtyProcess& pty)
{
#ifdef PLATFORM_WIN32

	const auto& q = static_cast<const WindowsPtyProcess&>(pty);
	slots.RemoveIf([this, &q](int i) {
		return slots[i].hProcess == q.hProcess;
	});

#elif PLATFORM_POSIX

	const auto& q = static_cast<const PosixPtyProcess&>(pty);
	slots.RemoveIf([this, &q](int i) {
		return slots[i].fd == q.GetSocket();
	});

#endif
}

bool PtyWaitEvent::Wait(int timeout)
{
#ifdef PLATFORM_WIN32

	auto CheckPipe = [](HANDLE h, bool& dataFlag, bool& exceptionFlag, dword events, bool& hasEvents) -> bool {
		if(!h)
			return false;
		
		DWORD n = 0;
		if(PeekNamedPipe(h, nullptr, 0, nullptr, &n, nullptr)) {
			if(n > 0) {
				dataFlag = true;
				hasEvents = true;
				return true;
			}
		}
		else
		if((events & WAIT_IS_EXCEPTION) && IsException(GetLastError())) {
			exceptionFlag = true;
			hasEvents = true;
			return true;
		}
		return false;
	};

	bool hasEvents = false;
	for(Slot& slot : slots) {
		slot.eRead = slot.eWrite = slot.eError = slot.eException = false;
		if(slot.events & WAIT_READ) {
			CheckPipe(slot.hRead, slot.eRead, slot.eException, slot.events, hasEvents);
			CheckPipe(slot.hError, slot.eError, slot.eException, slot.events, hasEvents);
		}
		if((slot.events & WAIT_WRITE) && slot.hWrite) {
			slot.eWrite = true;
			hasEvents = true;
		}
	}
	
	// If no events are pending, sleep for the specified timeout
	if(!hasEvents && timeout > 0)
		Sleep(timeout);
	
	return hasEvents;

#elif PLATFORM_POSIX

	int rc = 0;
	do {
		rc = poll((pollfd*) slots.begin(), slots.GetCount(), timeout);
	}
	while(rc == -1 && errno == EINTR);  // Retry on signal interruption

	if(rc == -1)
		LLOG("poll() failed: " << strerror(errno));

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