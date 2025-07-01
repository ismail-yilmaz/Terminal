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
{
}

PtyWaitEvent::Slot::~Slot()
{
	if(oRead.hEvent)
		CloseHandle(oRead.hEvent);
	if(oWrite.hEvent)
		CloseHandle(oWrite.hEvent);
	if(oError.hEvent)
		CloseHandle(oError.hEvent);
}

#endif

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

	handles.Clear();
	exceptions.Clear();
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

	int index = slots.GetCount() - 1;
	handles.Add(slot.hRead, index);
	handles.Add(slot.hWrite, index);
	handles.Add(slot.hError, index);
	
	if(events & WAIT_READ) {
		slot.oRead.hEvent  = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		slot.oError.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}
	if(events & WAIT_WRITE) {
		slot.oWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}
	if(events & WAIT_IS_EXCEPTION) {
		exceptions.Add(slot.hRead);
		exceptions.Add(slot.hWrite);
		exceptions.Add(slot.hError);
	}

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
		const Slot& s = slots[i];
		if(s.hProcess == q.hProcess) {
			// Is there a better way?
			handles.RemoveKey(s.hRead);
			handles.RemoveKey(s.hWrite);
			handles.RemoveKey(s.hError);
			exceptions.RemoveKey(s.hRead);
			exceptions.RemoveKey(s.hWrite);
			exceptions.RemoveKey(s.hError);
			return true;
		}
		return false;
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

	DWORD bytesTransferred;
	ULONG_PTR completionKey;
	lastOverlapped = nullptr;  // Reset before waiting

	BOOL success = GetQueuedCompletionStatus(hIocp, &bytesTransferred, &completionKey, &lastOverlapped, timeout);

	// First check for exceptional condition (operation completed with an error)
	if(!success) {
		if(lastOverlapped) {
			DWORD error = GetLastError();
			if(IsException(error) && exceptions.Find((HANDLE) completionKey) >= 0) {
				LLOG("Exception occured while waiting for pipe #" << completionKey);
				return true;
			}
		}
		return false;
	}
	return handles.Find((HANDLE) completionKey) >= 0;


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

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount() || !lastOverlapped)
		return 0;

	dword events = 0;

	if(lastOverlapped == &slots[i].oRead
	|| lastOverlapped == &slots[i].oError)
		events |= WAIT_READ;
	if(lastOverlapped == &slots[i].oWrite)
		events |= WAIT_WRITE;

	// Check for exceptional condition
	HANDLE handle = nullptr;
	if(lastOverlapped == &slots[i].oRead)
		handle = slots[i].hRead;
	else
	if(lastOverlapped == &slots[i].oWrite)
		handle = slots[i].hWrite;
	else
	if(lastOverlapped == &slots[i].oError)
		handle = slots[i].hError;
	
	if(handle
	&& (WaitForSingleObject(handle, 0) == WAIT_OBJECT_0) // For rare but potential deadlocks
	&& !GetOverlappedResult(handle, lastOverlapped, nullptr, FALSE)) {
		DWORD error = GetLastError();
		if(IsException(error) && exceptions.Find(handle) >= 0) {
			events |= WAIT_IS_EXCEPTION;
		}
	}

#elif PLATFORM_POSIX

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount())
		return 0;

	dword events = 0;

	const pollfd& q = slots[i];
	if(q.revents & POLLIN)
		events |= WAIT_READ;
	if(q.revents & POLLOUT)
		events |= WAIT_WRITE;
	if(q.revents & POLLPRI)
		events |= WAIT_IS_EXCEPTION;

#endif

	return events;
}

dword PtyWaitEvent::operator[](int i) const
{
	return Get(i);
}

}