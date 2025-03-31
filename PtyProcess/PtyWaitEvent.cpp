#include "PtyProcess.h"

namespace Upp {

#define LLOG(x)	// RLOG("PtyWaitEvent: " << x);

PtyWaitEvent::PtyWaitEvent()
{
#ifdef PLATFORM_WIN32

	hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if(!hIocp) {
        LLOG("Failed to create IO Completion Port!");
        Exit(1);
    }

#endif
}

PtyWaitEvent::~PtyWaitEvent()
{
#ifdef PLATFORM_WIN32

	if(hIocp)
		CloseHandle(hIocp);

#endif
}

void PtyWaitEvent::Clear()
{
	slots.Clear();
	
#ifdef PLATFORM_WIN32

	lastOverlapped = nullptr;

#endif
}

void PtyWaitEvent::Add(const APtyProcess& pty, dword events)
{
#ifdef PLATFORM_WIN32

	const auto& p = static_cast<const WindowsPtyProcess&>(pty);

    Slot& slot    = slots.Add();
    slot.hProcess = p.hProcess;
    slot.hRead    = p.hOutputRead;
    slot.hWrite   = p.hInputWrite;
    slot.hError   = p.hErrorRead;

    // Register pipes
    CreateIoCompletionPort(slot.hRead,  hIocp, (ULONG_PTR) slot.hRead,  0);
    CreateIoCompletionPort(slot.hWrite, hIocp, (ULONG_PTR) slot.hWrite, 0);
    CreateIoCompletionPort(slot.hError, hIocp, (ULONG_PTR) slot.hError, 0);

    if(events & WAIT_READ) {
        slot.oRead.hEvent  = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        slot.oError.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    }
    if(events & WAIT_WRITE)
        slot.oWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

#elif PLATFORM_POSIX

	auto& p = static_cast<const PosixPtyProcess&>(pty);
	pollfd& q = slots.Add({0});
	q.fd = p.GetSocket();
	if(events & WAIT_READ)
		q.events |= POLLIN;
	if(events & WAIT_WRITE)
		q.events |= POLLOUT;
//	if(events & WAIT_IS_EXCEPTION)
//		q.events |= POLLPRI;

#endif
}

bool PtyWaitEvent::Wait(int timeout)
{
#ifdef PLATFORM_WIN32

    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    lastOverlapped = nullptr;  // Reset before waiting

    BOOL success = GetQueuedCompletionStatus(hIocp, &bytesTransferred, &completionKey, &lastOverlapped, timeout);
    if(!success || !completionKey)
		return false;

    for(int i = 0; i < slots.GetCount(); ++i) {
        if(slots[i].hRead  == (HANDLE) completionKey
        || slots[i].hWrite == (HANDLE) completionKey
        || slots[i].hError == (HANDLE) completionKey) {
            return true;
        }
    }

    return false;
    
#elif PLATFORM_POSIX

	return poll((pollfd*) slots.begin(), slots.GetCount(), timeout) > 0;

#endif
}

dword PtyWaitEvent::Get(int i) const
{
#ifdef PLATFORM_WIN32

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount() || !lastOverlapped)
		return 0;
        
    dword events = 0;
    if(lastOverlapped == &slots[i].oRead
    || lastOverlapped == &slots[i].oError)
		events |= WAIT_READ;
    if(lastOverlapped == &slots[i].oWrite)
		events |= WAIT_WRITE;

#elif PLATFORM_POSIX

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount())
		return 0;
	
	dword events = 0;
	const pollfd& q = slots[i];
	if(q.revents & POLLIN)
		events |= WAIT_READ;
	if(q.revents & POLLOUT)
		events |= WAIT_WRITE;
//	if(q.revents & POLLPRI)
//		events |= WAIT_IS_EXCEPTION;

#endif

    return events;
}

dword PtyWaitEvent::operator[](int i) const
{
	return Get(i);
}

}