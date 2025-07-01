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
, lastOverlapped(nullptr)
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
	LPOVERLAPPED overlapped;

	BOOL success = GetQueuedCompletionStatus(
		hIocp, &bytesTransferred, &completionKey, &overlapped, timeout
	);

	if(!success) {
		DWORD error = GetLastError();
		if(overlapped && IsException(error)) {
			if(int i = handles.Find((HANDLE) completionKey); i >= 0) {
				slots[i].lastOverlapped = overlapped;
				return true;
			}
		}
		return false;
	}

	if(overlapped) {
		if(int i = handles.Find((HANDLE) completionKey); i >= 0) {
			slots[i].lastOverlapped = overlapped;
			return true;
		}
	}
	return false;

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

	if (slots.IsEmpty() || i < 0 || i >= slots.GetCount())
		return 0;

	const Slot& slot = slots[i];
	if (!slot.lastOverlapped)
		return 0;

	dword events = 0;
	
	if (slot.lastOverlapped == &slot.oRead || slot.lastOverlapped == &slot.oError)
		events |= WAIT_READ;
	if (slot.lastOverlapped == &slot.oWrite)
		events |= WAIT_WRITE;

	// Check for exceptions by examining the overlapped operation result
	DWORD bytesTransferred;
	HANDLE hPipe = (slot.lastOverlapped == &slot.oRead) ? slot.hRead :
				   (slot.lastOverlapped == &slot.oWrite) ? slot.hWrite : slot.hError;
	
	if(!GetOverlappedResult(hPipe, slot.lastOverlapped, &bytesTransferred, FALSE)) {
		DWORD error = GetLastError();
		if(IsException(error) && exceptions.Find(hPipe) >= 0) {
			events |= WAIT_IS_EXCEPTION;
		}
	}

	return events;
	
#elif PLATFORM_POSIX

	if(slots.IsEmpty() || i < 0 || i >= slots.GetCount())
		return 0;

	const pollfd& q = slots[i];
	if(q.fd < 0)  // Check for invalid descriptor
		return 0;

	dword events = 0;
	
	if(q.revents & POLLIN)
		events |= WAIT_READ;
	if(q.revents & POLLOUT)
		events |= WAIT_WRITE;
	if(q.revents & POLLPRI)
		events |= WAIT_IS_EXCEPTION;
	if(q.revents & (POLLERR | POLLHUP | POLLNVAL))
		events |= WAIT_IS_EXCEPTION;  // Handle all error conditions

	return events;
#endif
}

dword PtyWaitEvent::operator[](int i) const
{
	return Get(i);
}

}