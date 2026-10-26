"""Sampling profiler for a running x64 client: suspends its main thread (the game loop) ~200 times a second,
walks the stack with dbghelp and prints the share of time spent working (not waiting in Present or the frame
limiter) and the exclusive / inclusive hot functions. Symbols come from the PDBs the exe and
DLLs point at (src/build/win32/x64/<Configuration>), so profile a build whose PDBs are still in place.

usage: python sampler.py <pid> <seconds> [top N] [stacks.pkl]
The optional pickle keeps every sampled stack (function names, innermost first) for analyze.py."""
import collections
import ctypes
import ctypes.wintypes as wt
import os
import pickle
import sys
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
dbg = ctypes.WinDLL("dbghelp", use_last_error=True)

PROCESS_ALL_ACCESS = 0x1FFFFF
THREAD_ALL_ACCESS = 0x1FFFFF
TH32CS_SNAPTHREAD = 0x4
CONTEXT_FULL = 0x10000B          # CONTEXT_AMD64 | CONTROL | INTEGER | FLOATING_POINT
CONTEXT_SIZE = 1232              # sizeof(CONTEXT) on x64; must be 16-byte aligned
IMAGE_FILE_MACHINE_AMD64 = 0x8664
ADDR_MODE_FLAT = 3


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG), ("dwFlags", wt.DWORD)]


class ADDRESS64(ctypes.Structure):
    _fields_ = [("Offset", ctypes.c_uint64), ("Segment", ctypes.c_uint16), ("Mode", ctypes.c_uint32)]


class STACKFRAME64(ctypes.Structure):
    _fields_ = [("AddrPC", ADDRESS64), ("AddrReturn", ADDRESS64), ("AddrFrame", ADDRESS64), ("AddrStack", ADDRESS64),
                ("AddrBStore", ADDRESS64), ("FuncTableEntry", ctypes.c_void_p), ("Params", ctypes.c_uint64 * 4),
                ("Far", wt.BOOL), ("Virtual", wt.BOOL), ("Reserved", ctypes.c_uint64 * 3),
                ("KdHelp", ctypes.c_uint64 * 40)]  # KDHELP64 with room to spare


class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [("SizeOfStruct", ctypes.c_ulong), ("TypeIndex", ctypes.c_ulong), ("Reserved", ctypes.c_uint64 * 2),
                ("Index", ctypes.c_ulong), ("Size", ctypes.c_ulong), ("ModBase", ctypes.c_uint64), ("Flags", ctypes.c_ulong),
                ("Value", ctypes.c_uint64), ("Address", ctypes.c_uint64), ("Register", ctypes.c_ulong),
                ("Scope", ctypes.c_ulong), ("Tag", ctypes.c_ulong), ("NameLen", ctypes.c_ulong),
                ("MaxNameLen", ctypes.c_ulong), ("Name", ctypes.c_char * 1024)]


class FILETIME(ctypes.Structure):
    _fields_ = [("lo", wt.DWORD), ("hi", wt.DWORD)]


k32.OpenProcess.restype = wt.HANDLE
k32.OpenThread.restype = wt.HANDLE
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
k32.GetThreadTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(FILETIME)] * 4
dbg.SymFunctionTableAccess64.restype = ctypes.c_void_p
dbg.SymGetModuleBase64.restype = ctypes.c_uint64
dbg.SymInitialize.argtypes = [wt.HANDLE, ctypes.c_char_p, wt.BOOL]
dbg.SymFromAddr.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(SYMBOL_INFO)]
dbg.StackWalk64.argtypes = [wt.DWORD, wt.HANDLE, wt.HANDLE, ctypes.POINTER(STACKFRAME64), ctypes.c_void_p,
                            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]


def thread_ids(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    entry = THREADENTRY32()
    entry.dwSize = ctypes.sizeof(entry)
    out = []
    ok = k32.Thread32First(snap, ctypes.byref(entry))
    while ok:
        if entry.th32OwnerProcessID == pid:
            out.append(entry.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(entry))
    k32.CloseHandle(snap)
    return out


def thread_cpu(handle):
    times = [FILETIME() for _ in range(4)]
    k32.GetThreadTimes(handle, *[ctypes.byref(t) for t in times])
    return sum((t.hi << 32) | t.lo for t in times[2:])


def thread_created(handle):
    times = [FILETIME() for _ in range(4)]
    k32.GetThreadTimes(handle, *[ctypes.byref(t) for t in times])
    return (times[0].hi << 32) | times[0].lo


def sample(pid, seconds, interval=0.004, busiest=False):
    """Stacks (lists of return addresses, innermost first) of the main thread (the first one the process created,
    the game loop), or of the busiest thread over one second, plus the process handle."""
    process = k32.OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    handles = {t: k32.OpenThread(THREAD_ALL_ACCESS, False, t) for t in thread_ids(pid)}
    handles = {t: h for t, h in handles.items() if h}
    if busiest:
        before = {t: thread_cpu(h) for t, h in handles.items()}
        time.sleep(1.0)
        thread = handles[max(handles, key=lambda t: thread_cpu(handles[t]) - before[t])]
    else:
        thread = handles[min(handles, key=lambda t: thread_created(handles[t]))]

    dbg.SymSetOptions(0x2 | 0x4 | 0x200)  # UNDNAME | DEFERRED_LOADS | FAIL_CRITICAL_ERRORS
    # SWG_SYMBOL_PATH: extra PDB folders searched first, e.g. for an older exe whose build PDB was overwritten
    search = os.environ.get("SWG_SYMBOL_PATH")
    if not dbg.SymInitialize(process, search.encode("mbcs") if search else None, True):
        raise SystemExit("SymInitialize failed (%d)" % ctypes.get_last_error())
    table_access = ctypes.cast(dbg.SymFunctionTableAccess64, ctypes.c_void_p)
    module_base = ctypes.cast(dbg.SymGetModuleBase64, ctypes.c_void_p)
    buffer = ctypes.create_string_buffer(CONTEXT_SIZE + 16)
    context = (ctypes.addressof(buffer) + 15) & ~15

    stacks = []
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        if k32.SuspendThread(thread) == 0xFFFFFFFF:
            break
        try:
            ctypes.memset(context, 0, CONTEXT_SIZE)
            ctypes.c_uint32.from_address(context + 0x30).value = CONTEXT_FULL
            if not k32.GetThreadContext(thread, context):
                continue
            frame = STACKFRAME64()
            frame.AddrPC.Offset, frame.AddrPC.Mode = ctypes.c_uint64.from_address(context + 0xF8).value, ADDR_MODE_FLAT
            frame.AddrStack.Offset, frame.AddrStack.Mode = ctypes.c_uint64.from_address(context + 0x98).value, ADDR_MODE_FLAT
            frame.AddrFrame.Offset, frame.AddrFrame.Mode = ctypes.c_uint64.from_address(context + 0xA0).value, ADDR_MODE_FLAT
            addresses = []
            for _ in range(96):
                if not dbg.StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, ctypes.byref(frame), context,
                                       None, table_access, module_base, None) or not frame.AddrPC.Offset:
                    break
                addresses.append(frame.AddrPC.Offset)
            stacks.append(addresses)
        finally:
            k32.ResumeThread(thread)
        time.sleep(interval)
    return process, stacks


def symbolize(process, stacks):
    names = {}
    symbol = SYMBOL_INFO()
    symbol.SizeOfStruct = 88  # sizeof(SYMBOL_INFO) without the name buffer
    symbol.MaxNameLen = 1000
    displacement = ctypes.c_uint64()

    def name(address):
        if address not in names:
            ok = dbg.SymFromAddr(process, address, ctypes.byref(displacement), ctypes.byref(symbol))
            names[address] = symbol.Name.decode(errors="replace") if ok else "0x%x" % address
        return names[address]

    named = [[name(a) for a in stack] for stack in stacks]
    dbg.SymCleanup(process)
    return named


# frames spent here are the game loop waiting (on the display, the GPU or the frame rate limiter), not working;
# a driver may spin while it waits, so thread CPU time alone overstates the work
WAITING = ("Graphics::present", "Clock::limitFrameRate")


def work_fraction(stacks):
    """Share of the samples in which the game loop was doing frame work rather than waiting."""
    stacks = [s for s in stacks if s]
    if not stacks:
        return 0.0
    waiting = sum(1 for s in stacks if any(w in s for w in WAITING))
    return 1.0 - float(waiting) / len(stacks)


def main():
    pid, seconds = int(sys.argv[1]), float(sys.argv[2])
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    process, raw = sample(pid, seconds)
    stacks = symbolize(process, raw)
    print("work (not waiting in Present / the frame limiter): %.0f%%" % (100.0 * work_fraction(stacks)))
    exclusive, inclusive = collections.Counter(), collections.Counter()
    for stack in stacks:
        if stack:
            exclusive[stack[0]] += 1
            inclusive.update(set(stack))
    n = max(1, len(stacks))
    print("samples:", len(stacks))
    print("\n== exclusive")
    for k, v in exclusive.most_common(top):
        print("%5.1f%%  %s" % (100.0 * v / n, k[:160]))
    print("\n== inclusive")
    for k, v in inclusive.most_common(top * 2):
        print("%5.1f%%  %s" % (100.0 * v / n, k[:160]))
    if len(sys.argv) > 4:
        with open(sys.argv[4], "wb") as f:
            pickle.dump(stacks, f)


if __name__ == "__main__":
    main()
