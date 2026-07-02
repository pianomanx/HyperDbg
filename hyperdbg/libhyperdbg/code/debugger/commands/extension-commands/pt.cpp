/**
 * @file pt.cpp
 * @author Masoud Rahimi Jafari (Masoodrahimy1379@gmail.com)
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief !pt command
 * @details
 * @version 0.19
 * @date 2026-04-29
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

//
// Global Variables
//
extern BOOLEAN g_IsHyperTraceModuleLoaded;
extern BOOLEAN g_IsSerialConnectedToRemoteDebuggee;

static UINT64  g_ImageBase = 0;
static UINT64  g_CodeBase  = 0;
static UINT64  g_CodeSize  = 0;
static UINT8 * g_Code      = NULL;

/**
 * @brief help of the !pt command
 *
 * @return VOID
 */
VOID
CommandPtHelp()
{
    ShowMessages("!pt : enables, disables and configures Intel Processor Trace (PT).\n\n");

    ShowMessages("syntax : \t!pt enable\n");
    ShowMessages("syntax : \t!pt enable [size BufferSize (hex)]\n");
    ShowMessages("syntax : \t!pt enable [pid ProcessId (hex)] [size BufferSize (hex)] [core CoreId (hex)]\n");
    ShowMessages("syntax : \t!pt enable [tid ThreadId (hex)] [size BufferSize (hex)] [core CoreId (hex)]\n");
    ShowMessages("syntax : \t!pt enable [pname ProcessName (string)] [size BufferSize (hex)] [core CoreId (hex)]\n");
    ShowMessages("syntax : \t!pt enable [path Path (string)] [size BufferSize (hex)] [core CoreId (hex)]\n");
    ShowMessages("syntax : \t!pt enable [cr3 Cr3Value (hex)] [size BufferSize (hex)]\n");
    ShowMessages("syntax : \t!pt disable\n");
    ShowMessages("syntax : \t!pt pause\n");
    ShowMessages("syntax : \t!pt resume\n");
    ShowMessages("syntax : \t!pt flush\n");
    ShowMessages("syntax : \t!pt dump print [type TypeOfDump (string)]\n");
    ShowMessages("syntax : \t!pt dump path [type TypeOfDump (string)]\n");
    ShowMessages("syntax : \t!pt filter [Mode (string)]\n");
    ShowMessages("syntax : \t!pt filter [range1 FromAddress (hex) ToAddress (hex)] [range2 FromAddress (hex) ToAddress (hex)] [range3 FromAddress (hex) ToAddress (hex)] [range4 FromAddress (hex) ToAddress (hex)]\n");
    ShowMessages("syntax : \t!pt filter [range1 module ModuleName (string)] [range2 module ModuleName (string)] [range3 module ModuleName (string)] [range4 module ModuleName (string)]\n");
    ShowMessages("syntax : \t!pt filter [stoprange1 FromAddress (hex) ToAddress (hex)] [stoprange2 FromAddress (hex) ToAddress (hex)] [stoprange3 FromAddress (hex) ToAddress (hex)] [stoprange4 FromAddress (hex) ToAddress (hex)]\n");
    ShowMessages("syntax : \t!pt filter [stoprange1 module ModuleName (string)] [stoprange2 module ModuleName (string)] [stoprange3 module ModuleName (string)] [stoprange4 module ModuleName (string)]\n");
    ShowMessages("syntax : \t!pt packet [PacketType (string)]\n");

    ShowMessages("\n");
    ShowMessages("\t\te.g : !pt enable\n");
    ShowMessages("\t\te.g : !pt enable size 0x200000\n");
    ShowMessages("\t\te.g : !pt enable pid 0x4a8\n");
    ShowMessages("\t\te.g : !pt enable pname notepad.exe\n");
    ShowMessages("\t\te.g : !pt enable tid 0x1234 core 3\n");
    ShowMessages("\t\te.g : !pt enable cr3 0x1aabb000\n");
    ShowMessages("\t\te.g : !pt enable pid 0x4a8 size 0x200000\n");
    ShowMessages("\t\te.g : !pt enable path \"c:\\programs\\my exe file.exe\" size 0x200000 core 3\n");
    ShowMessages("\t\te.g : !pt disable\n");
    ShowMessages("\t\te.g : !pt pause\n");
    ShowMessages("\t\te.g : !pt resume\n");
    ShowMessages("\t\te.g : !pt flush\n");
    ShowMessages("\t\te.g : !pt dump print type instruction\n");
    ShowMessages("\t\te.g : !pt dump print type packet\n");
    ShowMessages("\t\te.g : !pt dump path C:\\trace.txt type instruction\n");
    ShowMessages("\t\te.g : !pt filter user\n");
    ShowMessages("\t\te.g : !pt filter user kernel\n");
    ShowMessages("\t\te.g : !pt filter range1 0x140001000 0x140002000\n");
    ShowMessages("\t\te.g : !pt filter range1 module main\n");
    ShowMessages("\t\te.g : !pt filter range1 module ntdll range2 module nt\n");
    ShowMessages("\t\te.g : !pt filter stoprange1 0x140003000 0x140004000\n");
    ShowMessages("\t\te.g : !pt packet psb pip tsc\n");

    ShowMessages("\n");
    ShowMessages("Where:\n");
    ShowMessages("\t[Mode (string)] could be 'kernel' or/and 'user'\n");
    ShowMessages("\t[TypeOfDump (string)] could be 'instruction' or 'packet'\n");
    ShowMessages("\t[ModuleName (string)] could be 'main' (the main module of the process), 'nt', 'win32k', or any other module name\n");
    ShowMessages("\t[PacketType (string)] could be either or a combination of 'psb', 'pip', 'tsc', 'mtc', 'cyc', 'tnt', 'tip', 'fup', or 'mode'\n");
    ShowMessages("\n");
}

/**
 * @brief Send PT requests
 *
 * @param PtRequest
 *
 * @return VOID
 */
BOOLEAN
CommandPtSendRequest(HYPERTRACE_PT_OPERATION_PACKETS * PtRequest)
{
    BOOL  Status;
    ULONG ReturnedLength;

    if (g_IsSerialConnectedToRemoteDebuggee)
    {
        //
        // Send the request over serial kernel debugger
        //
        if (!KdSendHyperTracePtPacketsToDebuggee(PtRequest, SIZEOF_HYPERTRACE_PT_OPERATION_PACKETS))
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
    else
    {
        AssertShowMessageReturnStmt(g_IsHyperTraceModuleLoaded, g_DeviceHandle, ASSERT_MESSAGE_HYPERTRACE_NOT_LOADED, ASSERT_MESSAGE_DRIVER_NOT_LOADED, AssertReturnFalse);

        //
        // Send IOCTL
        //
        Status = DeviceIoControl(
            g_DeviceHandle,                         // Handle to device
            IOCTL_PERFORM_HYPERTRACE_PT_OPERATION,  // IO Control Code (IOCTL)
            PtRequest,                              // Input Buffer to driver.
            SIZEOF_HYPERTRACE_PT_OPERATION_PACKETS, // Input buffer length
            PtRequest,                              // Output Buffer from driver.
            SIZEOF_HYPERTRACE_PT_OPERATION_PACKETS, // Length of output buffer in bytes.
            &ReturnedLength,                        // Bytes placed in buffer.
            NULL                                    // synchronous call
        );

        if (!Status)
        {
            ShowMessages("ioctl failed with code 0x%x\n", GetLastError());

            return FALSE;
        }

        if (PtRequest->KernelStatus == DEBUGGER_OPERATION_WAS_SUCCESSFUL)
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
}

/**
 * @brief Request to perform an PT operation
 *
 * @param PtRequest
 *
 * @return BOOLEAN
 */
BOOLEAN
HyperDbgPerformPtOperation(HYPERTRACE_PT_OPERATION_PACKETS * PtRequest)
{
    return CommandPtSendRequest(PtRequest);
}

/**
 * @brief Send pt enable command
 *
 * @return BOOLEAN
 */
static BOOLEAN
CommandPtSendEnable()
{
    HYPERTRACE_PT_OPERATION_PACKETS PtRequest = {};

    //
    // Set the PtRequest structure for the operation
    //
    PtRequest.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_ENABLE;

    //
    // Send the request to perform the operation
    //
    if (!HyperDbgPerformPtOperation(&PtRequest))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/**
 * @brief Send pt disable command
 *
 * @return BOOLEAN
 */
static BOOLEAN
CommandPtSendDisable()
{
    HYPERTRACE_PT_OPERATION_PACKETS PtRequest = {};

    //
    // Set the PtRequest structure for the operation
    //
    PtRequest.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_DISABLE;

    //
    // Send the request to perform the operation
    //
    if (!HyperDbgPerformPtOperation(&PtRequest))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/**
 * @brief Send pt pause command
 *
 * @return BOOLEAN
 */
static BOOLEAN
CommandPtSendPause()
{
    HYPERTRACE_PT_OPERATION_PACKETS PtRequest = {};

    //
    // Set the PtRequest structure for the operation
    //
    PtRequest.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_PAUSE;

    //
    // Send the request to perform the operation
    //
    if (!HyperDbgPerformPtOperation(&PtRequest))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/**
 * @brief Send pt resume command
 *
 * @return BOOLEAN
 */
static BOOLEAN
CommandPtSendResume()
{
    HYPERTRACE_PT_OPERATION_PACKETS PtRequest = {};

    //
    // Set the PtRequest structure for the operation
    //
    PtRequest.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_RESUME;

    //
    // Send the request to perform the operation
    //
    if (!HyperDbgPerformPtOperation(&PtRequest))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/**
 * @brief Send pt flush command
 *
 * @return BOOLEAN
 */
static BOOLEAN
CommandPtSendFlush()
{
    HYPERTRACE_PT_OPERATION_PACKETS PtRequest = {};

    //
    // Set the PtRequest structure for the operation
    //
    PtRequest.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_FLUSH;

    //
    // Send the request to perform the operation
    //
    if (!HyperDbgPerformPtOperation(&PtRequest))
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/**
 * @brief Send pt filter command
 *
 * @return BOOLEAN
 */
static BOOLEAN
CommandPtSendFilterByPid(UINT32  ProcessId,
                         BOOLEAN IsUserMode,
                         BOOLEAN IsKernelMode,
                         UINT64  StartRange1,
                         UINT64  EndRange1,
                         UINT64  StartRange2,
                         UINT64  EndRange2,
                         UINT64  StartRange3,
                         UINT64  EndRange3,
                         UINT64  StartRange4,
                         UINT64  EndRange4)
{
    HYPERTRACE_PT_OPERATION_PACKETS Op             = {};
    UINT8                           NumberOfRanges = 0;

    //
    // Set the Op structure for the operation
    //
    Op.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_FILTER;

    //
    // Set execution modes
    //
    Op.FilterOptions.TraceUser   = IsUserMode ? 1 : 0;
    Op.FilterOptions.TraceKernel = IsKernelMode ? 1 : 0;

    //
    // Set the process ID
    //
    Op.EnableOptions.Pid = ProcessId;

    //
    // Set the first range if provided
    //
    if (StartRange1 != NULL && EndRange1 != NULL)
    {
        Op.FilterOptions.AddrRanges[NumberOfRanges].Start = StartRange1;
        Op.FilterOptions.AddrRanges[NumberOfRanges].End   = EndRange1;
        NumberOfRanges++;
    }

    //
    // Set the second range if provided
    //
    if (StartRange2 != NULL && EndRange2 != NULL)
    {
        Op.FilterOptions.AddrRanges[NumberOfRanges].Start = StartRange2;
        Op.FilterOptions.AddrRanges[NumberOfRanges].End   = EndRange2;
        NumberOfRanges++;
    }

    //
    // Set the third range if provided
    //
    if (StartRange3 != NULL && EndRange3 != NULL)
    {
        Op.FilterOptions.AddrRanges[NumberOfRanges].Start = StartRange3;
        Op.FilterOptions.AddrRanges[NumberOfRanges].End   = EndRange3;
        NumberOfRanges++;
    }

    //
    // Set the fourth range if provided
    //
    if (StartRange4 != NULL && EndRange4 != NULL)
    {
        Op.FilterOptions.AddrRanges[NumberOfRanges].Start = StartRange4;
        Op.FilterOptions.AddrRanges[NumberOfRanges].End   = EndRange4;
        NumberOfRanges++;
    }

    //
    // Set number of ranges
    //
    Op.FilterOptions.NumAddrRanges = NumberOfRanges;

    //
    // Send the request to perform the operation
    //
    if (!HyperDbgPerformPtOperation(&Op))
    {
        return FALSE;
    }
    else
    {
        ShowMessages("[+] PT Filter: cr3=0x%llx, pid=0x%x, user=%x, kernel=%x, ranges=%x\n",
                     Op.EnableOptions.Cr3,
                     Op.EnableOptions.Pid,
                     Op.FilterOptions.TraceUser,
                     Op.FilterOptions.TraceKernel,
                     Op.FilterOptions.NumAddrRanges);

        return TRUE;
    }

    return TRUE;
}

static int
CommandPtReadImage(UINT8 * Buffer, SIZE_T Size, const struct pt_asid * Asid, UINT64 Ip, VOID * Context)
{
    (VOID) Asid;
    (VOID) Context;

    if (g_Code == NULL || Ip < g_CodeBase || Ip >= g_CodeBase + g_CodeSize)
        return -pte_nomap;

    UINT64 Available = g_CodeBase + g_CodeSize - Ip;
    SIZE_T Count     = (Size < Available) ? Size : (SIZE_T)Available;

    memcpy(Buffer, g_Code + (Ip - g_CodeBase), Count);
    return (int)Count;
}

typedef struct _PROC_BASIC_INFO
{
    LONG      ExitStatus;
    PVOID     PebBaseAddress;
    ULONG_PTR Reserved[4];
} PROC_BASIC_INFO;

typedef LONG(NTAPI * PFN_NT_QIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);

static BOOLEAN
CommandPtCaptureImage(HANDLE Process, UINT64 * TextStart, UINT64 * TextEnd)
{
    HMODULE            Ntdll = GetModuleHandleA("ntdll.dll");
    PFN_NT_QIP         NtQip = Ntdll ? (PFN_NT_QIP)GetProcAddress(Ntdll, "NtQueryInformationProcess") : NULL;
    PROC_BASIC_INFO    Pbi   = {0};
    ULONG              Ret   = 0;
    SIZE_T             Got   = 0;
    UINT64             Base  = 0;
    IMAGE_DOS_HEADER   Dos;
    IMAGE_NT_HEADERS64 Nt;
    UINT64             SectionBase;

    if (NtQip == NULL || NtQip(Process, 0, &Pbi, sizeof(Pbi), &Ret) < 0 || Pbi.PebBaseAddress == NULL)
        return FALSE;

    if (!ReadProcessMemory(Process, (PBYTE)Pbi.PebBaseAddress + 0x10, &Base, sizeof(Base), &Got) || Base == 0)
        return FALSE;

    if (!ReadProcessMemory(Process, (PVOID)Base, &Dos, sizeof(Dos), &Got) || Dos.e_magic != IMAGE_DOS_SIGNATURE)
        return FALSE;

    if (!ReadProcessMemory(Process, (PBYTE)Base + Dos.e_lfanew, &Nt, sizeof(Nt), &Got) || Nt.Signature != IMAGE_NT_SIGNATURE)
        return FALSE;

    g_ImageBase = Base;
    SectionBase = Base + Dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS64, OptionalHeader) + Nt.FileHeader.SizeOfOptionalHeader;

    for (WORD i = 0; i < Nt.FileHeader.NumberOfSections; i++)
    {
        IMAGE_SECTION_HEADER Section;

        if (!ReadProcessMemory(Process, (PBYTE)SectionBase + (UINT64)i * sizeof(Section), &Section, sizeof(Section), &Got))
            return FALSE;

        if (memcmp(Section.Name, ".text", 6) != 0)
            continue;

        UINT64 Start = Base + Section.VirtualAddress;
        UINT64 Size  = Section.Misc.VirtualSize ? Section.Misc.VirtualSize : Section.SizeOfRawData;

        if (Size == 0)
            return FALSE;

        g_Code = (UINT8 *)malloc((SIZE_T)Size);
        if (g_Code == NULL)
            return FALSE;

        if (!ReadProcessMemory(Process, (PVOID)Start, g_Code, (SIZE_T)Size, &Got) || Got != Size)
        {
            free(g_Code);
            g_Code = NULL;
            return FALSE;
        }

        g_CodeBase = Start;
        g_CodeSize = Size;
        *TextStart = Start;
        *TextEnd   = Start + Size - 1;
        return TRUE;
    }

    return FALSE;
}

static BOOLEAN
CommandPtResolveFunction(HANDLE Process, const CHAR * Path, const CHAR * Name, UINT64 * Start, UINT64 * End)
{
    union
    {
        SYMBOL_INFO Info;
        BYTE        Buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    } Symbol   = {0};
    BOOLEAN Ok = FALSE;

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(Process, NULL, FALSE))
        return FALSE;

    if (SymLoadModuleEx(Process, NULL, Path, NULL, (DWORD64)g_ImageBase, 0, NULL, 0) != 0)
    {
        Symbol.Info.SizeOfStruct = sizeof(SYMBOL_INFO);
        Symbol.Info.MaxNameLen   = MAX_SYM_NAME;

        if (SymFromName(Process, Name, &Symbol.Info) && Symbol.Info.Address != 0)
        {
            *Start = Symbol.Info.Address;
            *End   = Symbol.Info.Address + (Symbol.Info.Size ? Symbol.Info.Size : 0x200) - 1;
            Ok     = TRUE;
        }
    }

    SymCleanup(Process);
    return Ok;
}

static const CHAR *
CommandPtPacketName(enum pt_packet_type Type)
{
    switch (Type)
    {
    case ppt_psb:
        return "PSB";
    case ppt_psbend:
        return "PSBEND";
    case ppt_pad:
        return "PAD";
    case ppt_fup:
        return "FUP";
    case ppt_tip:
        return "TIP";
    case ppt_tip_pge:
        return "TIP.PGE";
    case ppt_tip_pgd:
        return "TIP.PGD";
    case ppt_tnt_8:
        return "TNT8";
    case ppt_tnt_64:
        return "TNT64";
    case ppt_mode:
        return "MODE";
    case ppt_pip:
        return "PIP";
    case ppt_vmcs:
        return "VMCS";
    case ppt_cbr:
        return "CBR";
    case ppt_tsc:
        return "TSC";
    case ppt_tma:
        return "TMA";
    case ppt_mtc:
        return "MTC";
    case ppt_cyc:
        return "CYC";
    case ppt_ovf:
        return "OVF";
    case ppt_stop:
        return "STOP";
    case ppt_exstop:
        return "EXSTOP";
    case ppt_mnt:
        return "MNT";
    case ppt_ptw:
        return "PTW";
    default:
        return "?";
    }
}

static UINT64
CommandPtReconstructIp(const struct pt_packet_ip * Packet, UINT64 * LastIp)
{
    UINT64 Value = *LastIp;

    switch (Packet->ipc)
    {
    case pt_ipc_update_16:
        Value = (Value & ~0xffffull) | (Packet->ip & 0xffffull);
        break;
    case pt_ipc_update_32:
        Value = (Value & ~0xffffffffull) | (Packet->ip & 0xffffffffull);
        break;
    case pt_ipc_update_48:
        Value = (Value & ~0xffffffffffffull) | (Packet->ip & 0xffffffffffffull);
        break;
    case pt_ipc_sext_48:
        Value = Packet->ip & 0xffffffffffffull;
        if (Value & 0x800000000000ull)
            Value |= 0xffff000000000000ull;
        break;
    default:
        Value = Packet->ip;
        break;
    }

    *LastIp = Value;
    return Value;
}

static UINT64
CommandPtDecodeCorePackets(UINT32 Cpu, const UINT8 * Buffer, UINT64 Size)
{
    struct pt_config           Config;
    struct pt_packet_decoder * Decoder;
    UINT64                     Count  = 0;
    UINT64                     LastIp = 0;
    int                        Status;

    pt_config_init(&Config);
    Config.begin = (UINT8 *)Buffer;
    Config.end   = (UINT8 *)Buffer + Size;

    Decoder = pt_pkt_alloc_decoder(&Config);
    if (Decoder == NULL)
    {
        ShowMessages("[-] core %u: cannot allocate packet decoder\n", Cpu);
        return 0;
    }

    for (;;)
    {
        Status = pt_pkt_sync_forward(Decoder);
        if (Status < 0)
            break;

        for (;;)
        {
            struct pt_packet Packet;

            Status = pt_pkt_next(Decoder, &Packet, sizeof(Packet));
            if (Status < 0)
                break;

            Count++;

            switch (Packet.type)
            {
            case ppt_tnt_8:
            case ppt_tnt_64:
                ShowMessages("    %-8s %2u  ", CommandPtPacketName(Packet.type), Packet.payload.tnt.bit_size);
                for (UINT8 Bit = 0; Bit < Packet.payload.tnt.bit_size && Bit < 64; Bit++)
                    putchar(((Packet.payload.tnt.payload >> (Packet.payload.tnt.bit_size - 1 - Bit)) & 1) ? 'T' : 'N');
                putchar('\n');
                break;

            case ppt_tip:
            case ppt_fup:
            case ppt_tip_pge:
            case ppt_tip_pgd:
                if (Packet.payload.ip.ipc == pt_ipc_suppressed)
                    ShowMessages("    %-8s (ip suppressed)\n", CommandPtPacketName(Packet.type));
                else
                {
                    UINT64 Ip = CommandPtReconstructIp(&Packet.payload.ip, &LastIp);
                    ShowMessages("    %-8s 0x%016llx  exe+0x%llx\n",
                                 CommandPtPacketName(Packet.type),
                                 (UINT64)Ip,
                                 (UINT64)(Ip - g_ImageBase));
                }
                break;

            case ppt_pip:
                ShowMessages("    %-8s cr3=0x%llx\n", CommandPtPacketName(Packet.type), (UINT64)Packet.payload.pip.cr3);
                break;

            case ppt_cbr:
                // ShowMessages("    %-8s ratio=%u\n", CommandPtPacketName(Packet.type), Packet.payload.cbr.ratio);
                break;

            case ppt_tsc:
                ShowMessages("    %-8s tsc=0x%llx\n", CommandPtPacketName(Packet.type), (UINT64)Packet.payload.tsc.tsc);
                break;

            default:
                // ShowMessages("    %-8s\n", CommandPtPacketName(Packet.type));
                break;
            }
        }
    }

    pt_pkt_free_decoder(Decoder);
    return Count;
}

static UINT64
CommandPtDecodeCore(UINT32 Cpu, const UINT8 * Buffer, UINT64 Size)
{
    struct pt_config         Config;
    struct pt_insn_decoder * Decoder;
    struct pt_image *        Image;
    UINT64                   Count = 0;
    int                      Status;

    pt_config_init(&Config);
    Config.begin = (UINT8 *)Buffer;
    Config.end   = (UINT8 *)Buffer + Size;

    Decoder = pt_insn_alloc_decoder(&Config);
    if (Decoder == NULL)
    {
        ShowMessages("[-] core %u: cannot allocate instruction decoder\n", Cpu);
        return 0;
    }

    Image = pt_insn_get_image(Decoder);
    pt_image_set_callback(Image, CommandPtReadImage, NULL);

    for (;;)
    {
        Status = pt_insn_sync_forward(Decoder);
        if (Status < 0)
            break;

        for (;;)
        {
            struct pt_insn Insn;

            while (Status & pts_event_pending)
            {
                struct pt_event Event;
                Status = pt_insn_event(Decoder, &Event, sizeof(Event));
                if (Status < 0)
                    break;
            }

            if (Status < 0 || (Status & pts_eos))
                break;

            Status = pt_insn_next(Decoder, &Insn, sizeof(Insn));
            if (Status < 0)
                break;

            ZydisDisassembledInstruction Disasm;
            ZydisMachineMode             Mode = (Insn.mode == ptem_32bit) ? ZYDIS_MACHINE_MODE_LEGACY_32 : ZYDIS_MACHINE_MODE_LONG_64;

            if (ZYAN_SUCCESS(ZydisDisassembleIntel(Mode, Insn.ip, Insn.raw, Insn.size, &Disasm)))
                ShowMessages("    0x%016llx  exe+0x%-6llx  %s\n",
                             (UINT64)Insn.ip,
                             (UINT64)(Insn.ip - g_ImageBase),
                             Disasm.text);
            else
                ShowMessages("    0x%016llx  (undecodable)\n", (UINT64)Insn.ip);

            Count++;
        }

        if (Status >= 0 && (Status & pts_eos))
            break;
    }

    pt_insn_free_decoder(Decoder);
    return Count;
}

static VOID
CommandPtRunAndTrace(const CHAR * Path, const CHAR * Function, BOOLEAN Packets, int PinCore)
{
    STARTUPINFOA                    Startup     = {};
    PROCESS_INFORMATION             Process     = {};
    HYPERTRACE_PT_MMAP_PACKETS      Mmap        = {};
    HYPERTRACE_PT_OPERATION_PACKETS Sizes       = {};
    UINT64                          TextStart   = 0;
    UINT64                          TextEnd     = 0;
    UINT64                          FilterStart = 0;
    UINT64                          FilterEnd   = 0;
    UINT64                          Total       = 0;

    Startup.cb = sizeof(Startup);

    if (!CreateProcessA(Path, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &Startup, &Process))
    {
        ShowMessages("[-] cannot launch '%s' (error 0x%x)\n", Path, GetLastError());
        return;
    }

    ShowMessages("[+] launched '%s' (pid %u, suspended)\n", Path, Process.dwProcessId);

    if (PinCore >= 0)
    {
        DWORD_PTR Mask = (DWORD_PTR)1 << PinCore;
        if (SetProcessAffinityMask(Process.hProcess, Mask))
            ShowMessages("[+] pinned target to core %d (all trace should land on this core)\n", PinCore);
        else
            ShowMessages("[!] could not pin to core %d (error 0x%x); running unpinned\n", PinCore, GetLastError());
    }
    else
    {
        ShowMessages("[*] target unpinned (scheduler may migrate it across cores)\n");
    }

    if (!CommandPtCaptureImage(Process.hProcess, &TextStart, &TextEnd))
    {
        ShowMessages("[-] cannot read target image / .text section\n");
        TerminateProcess(Process.hProcess, 1);
        goto Cleanup;
    }

    ShowMessages("[+] image base 0x%llx, .text 0x%llx-0x%llx (%llu bytes)\n",
                 (UINT64)g_ImageBase,
                 (UINT64)TextStart,
                 (UINT64)TextEnd,
                 (UINT64)g_CodeSize);

    FilterStart = TextStart;
    FilterEnd   = TextEnd;

    if (Function != NULL && CommandPtResolveFunction(Process.hProcess, Path, Function, &FilterStart, &FilterEnd))
    {
        ShowMessages("[+] IP filter narrowed to '%s' 0x%llx-0x%llx (%llu bytes)\n",
                     Function,
                     (UINT64)FilterStart,
                     (UINT64)FilterEnd,
                     (UINT64)(FilterEnd - FilterStart + 1));
    }
    else
    {
        ShowMessages("[!] IP filter: whole .text (symbol '%s' not found - build the target with a PDB)\n",
                     Function ? Function : "(none)");
    }

    if (!CommandPtSendFilterByPid(Process.dwProcessId, TRUE, FALSE, FilterStart, FilterEnd, NULL, NULL, NULL, NULL, NULL, NULL) ||
        !CommandPtSendEnable())
    {
        ShowMessages("[-] cannot enable Intel PT\n");
        TerminateProcess(Process.hProcess, 1);
        goto Cleanup;
    }

    if (!hyperdbg_u_pt_mmap(&Mmap))
    {
        ShowMessages("[-] pt_mmap failed\n");
        CommandPtSendDisable();
        TerminateProcess(Process.hProcess, 1);
        goto Cleanup;
    }

    ShowMessages("[+] PT enabled, %u per-core buffers mapped\n", Mmap.NumCpus);
    ShowMessages("[*] resuming target and waiting for it to exit...\n");

    ResumeThread(Process.hThread);
    WaitForSingleObject(Process.hProcess, INFINITE);
    ShowMessages("[+] target exited, decoding trace\n");

    CommandPtSendPause();

    Sizes.PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_SIZE;
    if (!hyperdbg_u_pt_operation(&Sizes))
    {
        ShowMessages("[-] cannot query PT sizes\n");
        CommandPtSendDisable();

        goto Cleanup;
    }

    for (UINT32 i = 0; i < Mmap.NumCpus; i++)
    {
        UINT32 Cpu   = Mmap.Cpus[i].CpuId;
        UINT64 Bytes = (Cpu < Sizes.NumCpus) ? Sizes.BytesPerCpu[Cpu] : 0;

        if (Bytes == 0)
            continue;

        if (Bytes > Mmap.Cpus[i].Size)
            Bytes = Mmap.Cpus[i].Size;

        ShowMessages("\n[*] core %u: %llu bytes of trace\n", Cpu, (UINT64)Bytes);
        Total += Packets
                     ? CommandPtDecodeCorePackets(Cpu, (const UINT8 *)(ULONG_PTR)Mmap.Cpus[i].UserVa, Bytes)
                     : CommandPtDecodeCore(Cpu, (const UINT8 *)(ULONG_PTR)Mmap.Cpus[i].UserVa, Bytes);
    }

    ShowMessages("\n[+] decoded %llu %s total\n", (UINT64)Total, Packets ? "packet(s)" : "instruction(s)");

    CommandPtSendDisable();

Cleanup:
    if (g_Code != NULL)
    {
        free(g_Code);
        g_Code = NULL;
    }
    if (Process.hThread != NULL)
        CloseHandle(Process.hThread);
    if (Process.hProcess != NULL)
        CloseHandle(Process.hProcess);
}

/**
 * @brief Map the per-CPU PT output buffers into the current process
 *
 * @details On success MmapRequest->Cpus[0..NumCpus) hold one { UserVa, Size }
 *          per CPU, valid in this process until PT is disabled / flushed.
 *          Only meaningful in local (VMI) mode.
 *
 * @param MmapRequest
 *
 * @return BOOLEAN
 */
BOOLEAN
HyperDbgPtMmapSendRequest(HYPERTRACE_PT_MMAP_PACKETS * MmapRequest)
{
    BOOL  Status;
    ULONG ReturnedLength;

    if (g_IsSerialConnectedToRemoteDebuggee)
    {
        //
        // The mmap surface maps into the caller's address space, which only
        // makes sense in local mode (no remote-debuggee transport for it).
        //
        ShowMessages("err, PT mmap is only available in local (VMI) mode\n");
        return FALSE;
    }

    AssertShowMessageReturnStmt(g_IsHyperTraceModuleLoaded, g_DeviceHandle, ASSERT_MESSAGE_HYPERTRACE_NOT_LOADED, ASSERT_MESSAGE_DRIVER_NOT_LOADED, AssertReturnFalse);

    Status = DeviceIoControl(
        g_DeviceHandle,                    // Handle to device
        IOCTL_PERFORM_HYPERTRACE_PT_MMAP,  // IO Control Code (IOCTL)
        MmapRequest,                       // Input Buffer to driver.
        SIZEOF_HYPERTRACE_PT_MMAP_PACKETS, // Input buffer length
        MmapRequest,                       // Output Buffer from driver.
        SIZEOF_HYPERTRACE_PT_MMAP_PACKETS, // Length of output buffer in bytes.
        &ReturnedLength,                   // Bytes placed in buffer.
        NULL                               // synchronous call
    );

    if (!Status)
    {
        ShowMessages("ioctl failed with code 0x%x\n", GetLastError());
        return FALSE;
    }

    return MmapRequest->KernelStatus == DEBUGGER_OPERATION_WAS_SUCCESSFUL;
}

/**
 * @brief Parse and display enable options for !pt enable command
 *
 * @param CommandTokens The command tokens to parse
 * @param PtRequest The PT request structure to fill with parsed options
 *
 * @return VOID
 */
static VOID
CommandPtParseEnable(vector<CommandToken> & CommandTokens, HYPERTRACE_PT_OPERATION_PACKETS * PtRequest)
{
    BOOLEAN HasPid   = FALSE;
    BOOLEAN HasPname = FALSE;
    BOOLEAN HasPath  = FALSE;
    BOOLEAN HasTid   = FALSE;
    BOOLEAN HasCr3   = FALSE;
    BOOLEAN HasSize  = FALSE;
    BOOLEAN HasCore  = FALSE;
    UINT64  Pid      = 0;
    UINT64  Tid      = 0;
    UINT64  Cr3      = 0;
    UINT64  Size     = 0;
    UINT32  Core     = 0;
    string  Pname;
    string  Path;

    for (SIZE_T i = 2; i < CommandTokens.size(); i++)
    {
        if (CompareLowerCaseStrings(CommandTokens.at(i), "pid"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'pid' expects a hex process ID\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            if (!ConvertTokenToUInt64(CommandTokens.at(i), &Pid))
            {
                ShowMessages("err, '%s' is not a valid hex process ID\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }
            HasPid = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "pname"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'pname' expects a process name\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            Pname    = GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i));
            HasPname = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "path"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'path' expects a process executable path\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            Path    = GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i));
            HasPath = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "tid"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'tid' expects a hex thread ID\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            if (!ConvertTokenToUInt64(CommandTokens.at(i), &Tid))
            {
                ShowMessages("err, '%s' is not a valid hex thread ID\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }
            HasTid = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "cr3"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'cr3' expects a hex CR3 value\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            if (!ConvertTokenToUInt64(CommandTokens.at(i), &Cr3))
            {
                ShowMessages("err, '%s' is not a valid hex CR3 value\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }
            HasCr3 = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "size"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'size' expects a hex buffer size\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            if (!ConvertTokenToUInt64(CommandTokens.at(i), &Size))
            {
                ShowMessages("err, '%s' is not a valid hex size\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }
            HasSize = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "core"))
        {
            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, 'core' expects a hex value as the core number\n\n");
                CommandPtHelp();
                return;
            }
            i++;
            if (!ConvertTokenToUInt32(CommandTokens.at(i), &Core))
            {
                ShowMessages("err, '%s' is not a valid hex size\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }
            HasCore = TRUE;
        }
        else
        {
            ShowMessages("err, unknown 'enable' option '%s'\n\n",
                         GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
            CommandPtHelp();
            return;
        }
    }

    //
    // pid, pname, tid, cr3 are mutually exclusive target selectors
    //
    INT32 SelectorCount = (HasPid ? 1 : 0) + (HasPname ? 1 : 0) + (HasPath ? 1 : 0) + (HasTid ? 1 : 0) + (HasCr3 ? 1 : 0);
    if (SelectorCount > 1)
    {
        ShowMessages("err, only one of 'pid', 'pname', 'path', 'tid', 'cr3' may be specified at a time\n\n");
        CommandPtHelp();
        return;
    }

    //
    // Show parsed enable options
    //
    ShowMessages("PT enable:\n");

    if (HasPid)
    {
        ShowMessages("  target pid   : 0x%llx\n", Pid);

        PtRequest->EnableOptions.EnableByPid = 1;
        PtRequest->EnableOptions.Pid         = (UINT32)Pid;
    }
    else if (HasPname)
    {
        ShowMessages("  target pname : %s\n", Pname.c_str());

        PtRequest->EnableOptions.EnableByCr3 = 1;
        strcpy_s(PtRequest->EnableOptions.ProcessName, sizeof(PtRequest->EnableOptions.ProcessName), Pname.c_str());
    }
    else if (HasPath)
    {
        ShowMessages("  target path : %s\n", Path.c_str());

        PtRequest->EnableOptions.EnableByPid = 1; // Path is resolved to a PID
    }
    else if (HasTid)
    {
        ShowMessages("  target tid   : 0x%llx\n", Tid);

        PtRequest->EnableOptions.EnableByTid = 1;
        PtRequest->EnableOptions.Tid         = (UINT32)Tid;
    }
    else if (HasCr3)
    {
        ShowMessages("  target cr3   : 0x%llx\n", Cr3);

        PtRequest->EnableOptions.EnableByCr3 = 1;
        PtRequest->EnableOptions.Cr3         = Cr3;
    }
    else
    {
        ShowMessages("  target       : all (no process/thread filter)\n");
    }

    //
    // Check for size options
    //
    if (HasSize)
    {
        ShowMessages("  buffer size  : 0x%llx bytes\n", Size);

        PtRequest->BufferSize = Size;
    }
    else
    {
        ShowMessages("  buffer size  : default (%llx bytes)\n", PT_DEFAULT_BUFFER_SIZE);

        PtRequest->BufferSize = PT_DEFAULT_BUFFER_SIZE;
    }

    //
    // Check for core options
    //
    if (HasCore)
    {
        ShowMessages("  core         : 0x%x\n", Core);
        PtRequest->CoreId = Core;
    }
    else
    {
        ShowMessages("  core         : default (0x%x)\n", PT_DEFAULT_PINNING_CORE);
        PtRequest->CoreId = PT_DEFAULT_PINNING_CORE;
    }

    //
    // Fill the PtRequest structure with parsed options
    //
    PtRequest->PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_ENABLE;

    //
    // Temporary workaround for testing , if a path is specified, resolve it to a PID and use that for the request
    // TODO: Should be removed
    //
    if (HasPath)
    {
        ShowMessages("  Running '%s' on core: %llx\n", Path.c_str(), PtRequest->CoreId);
        CommandPtRunAndTrace(Path.c_str(), NULL, FALSE, PtRequest->CoreId);
    }
}

/**
 * @brief Parse and display !pt dump parameters
 *
 * @param CommandTokens The command tokens to parse
 * @param PtRequest The PT request structure to fill with parsed options
 *
 * @return VOID
 */
static VOID
CommandPtParseDump(vector<CommandToken> & CommandTokens, HYPERTRACE_PT_OPERATION_PACKETS * PtRequest)
{
    if (CommandTokens.size() < 3)
    {
        ShowMessages("err, 'dump' requires additional options\n\n");
        CommandPtHelp();
        return;
    }

    if (CompareLowerCaseStrings(CommandTokens.at(2), "print"))
    {
        //
        // !pt dump print type <instruction|packet>
        //
        if (CommandTokens.size() != 5 || !CompareLowerCaseStrings(CommandTokens.at(3), "type"))
        {
            ShowMessages("err, syntax: !pt dump print type <instruction|packet>\n\n");
            CommandPtHelp();
            return;
        }

        if (!CompareLowerCaseStrings(CommandTokens.at(4), "instruction") &&
            !CompareLowerCaseStrings(CommandTokens.at(4), "packet"))
        {
            ShowMessages("err, dump type must be 'instruction' or 'packet', got '%s'\n\n",
                         GetCaseSensitiveStringFromCommandToken(CommandTokens.at(4)).c_str());
            CommandPtHelp();
            return;
        }

        ShowMessages("PT dump to console:\n");
        ShowMessages("  output : console (print)\n");
        ShowMessages("  format : %s\n",
                     GetCaseSensitiveStringFromCommandToken(CommandTokens.at(4)).c_str());
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(2), "path"))
    {
        //
        // !pt dump path <path-string> type <instruction|packet>
        //
        if (CommandTokens.size() != 6 || !CompareLowerCaseStrings(CommandTokens.at(4), "type"))
        {
            ShowMessages("err, syntax: !pt dump path <path> type <instruction|packet>\n\n");
            CommandPtHelp();
            return;
        }

        if (!CompareLowerCaseStrings(CommandTokens.at(5), "instruction") &&
            !CompareLowerCaseStrings(CommandTokens.at(5), "packet"))
        {
            ShowMessages("err, dump type must be 'instruction' or 'packet', got '%s'\n\n",
                         GetCaseSensitiveStringFromCommandToken(CommandTokens.at(5)).c_str());
            CommandPtHelp();
            return;
        }

        ShowMessages("PT dump to file:\n");
        ShowMessages("  output : file\n");
        ShowMessages("  path   : %s\n",
                     GetCaseSensitiveStringFromCommandToken(CommandTokens.at(3)).c_str());
        ShowMessages("  format : %s\n",
                     GetCaseSensitiveStringFromCommandToken(CommandTokens.at(5)).c_str());
    }
    else
    {
        ShowMessages("err, unknown 'dump' sub-option '%s'\n\n",
                     GetCaseSensitiveStringFromCommandToken(CommandTokens.at(2)).c_str());
        CommandPtHelp();
    }
}

/**
 * @brief Parse and display !pt filter parameters
 *
 * @param CommandTokens The command tokens to parse
 * @param PtRequest The PT request structure to fill with parsed options
 *
 * @return VOID
 */
static VOID
CommandPtParseFilter(vector<CommandToken> & CommandTokens, HYPERTRACE_PT_OPERATION_PACKETS * PtRequest)
{
    struct PtRangeEntry
    {
        BOOLEAN Active;
        BOOLEAN IsModule;
        UINT64  Start;
        UINT64  End;
        string  ModuleName;
    };

    PtRangeEntry Ranges[4]     = {};
    PtRangeEntry StopRanges[4] = {};
    BOOLEAN      TraceUser     = FALSE;
    BOOLEAN      TraceKernel   = FALSE;

    for (SIZE_T i = 2; i < CommandTokens.size(); i++)
    {
        if (CompareLowerCaseStrings(CommandTokens.at(i), "user"))
        {
            TraceUser = TRUE;
        }
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "kernel"))
        {
            TraceKernel = TRUE;
        }
        else
        {
            //
            // Resolve range1..range4 / stoprange1..stoprange4
            //
            BOOLEAN IsStop   = FALSE;
            INT32   RangeIdx = -1;

            if (CompareLowerCaseStrings(CommandTokens.at(i), "range1"))
            {
                IsStop   = FALSE;
                RangeIdx = 0;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "range2"))
            {
                IsStop   = FALSE;
                RangeIdx = 1;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "range3"))
            {
                IsStop   = FALSE;
                RangeIdx = 2;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "range4"))
            {
                IsStop   = FALSE;
                RangeIdx = 3;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "stoprange1"))
            {
                IsStop   = TRUE;
                RangeIdx = 0;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "stoprange2"))
            {
                IsStop   = TRUE;
                RangeIdx = 1;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "stoprange3"))
            {
                IsStop   = TRUE;
                RangeIdx = 2;
            }
            else if (CompareLowerCaseStrings(CommandTokens.at(i), "stoprange4"))
            {
                IsStop   = TRUE;
                RangeIdx = 3;
            }
            else
            {
                ShowMessages("err, unknown filter option '%s'\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }

            PtRangeEntry * Entry = IsStop ? &StopRanges[RangeIdx] : &Ranges[RangeIdx];

            if (i + 1 >= CommandTokens.size())
            {
                ShowMessages("err, '%s' expects <start> <end> or 'module <name>'\n\n",
                             GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                CommandPtHelp();
                return;
            }

            if (CompareLowerCaseStrings(CommandTokens.at(i + 1), "module"))
            {
                //
                // range<N> module <module-name>
                //
                if (i + 2 >= CommandTokens.size())
                {
                    ShowMessages("err, 'module' expects a module name\n\n");
                    CommandPtHelp();
                    return;
                }
                i += 2;
                Entry->IsModule   = TRUE;
                Entry->ModuleName = GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i));
                Entry->Active     = TRUE;
            }
            else
            {
                //
                // range<N> <start> <end>
                //
                if (i + 2 >= CommandTokens.size())
                {
                    ShowMessages("err, '%s' expects <start> <end>\n\n",
                                 GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                    CommandPtHelp();
                    return;
                }
                i++;
                if (!ConvertTokenToUInt64(CommandTokens.at(i), &Entry->Start))
                {
                    ShowMessages("err, '%s' is not a valid address\n\n",
                                 GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                    CommandPtHelp();
                    return;
                }
                i++;
                if (!ConvertTokenToUInt64(CommandTokens.at(i), &Entry->End))
                {
                    ShowMessages("err, '%s' is not a valid address\n\n",
                                 GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
                    CommandPtHelp();
                    return;
                }
                Entry->IsModule = FALSE;
                Entry->Active   = TRUE;
            }
        }
    }

    //
    // Show parsed filter options
    //
    ShowMessages("PT filter:\n");

    if (TraceUser)
    {
        ShowMessages("  privilege  : user (CPL > 0)\n");

        PtRequest->FilterOptions.TraceUser = 1;
    }
    if (TraceKernel)
    {
        ShowMessages("  privilege  : kernel (CPL == 0)\n");

        PtRequest->FilterOptions.TraceKernel = 1;
    }
    if (!TraceUser && !TraceKernel)
    {
        ShowMessages("  privilege  : (default - user + kernel)\n");

        PtRequest->FilterOptions.TraceUser   = 1;
        PtRequest->FilterOptions.TraceKernel = 1;
    }

    for (INT32 r = 0; r < 4; r++)
    {
        if (Ranges[r].Active)
        {
            if (Ranges[r].IsModule)
                ShowMessages("  range%d     : module '%s'\n", r + 1, Ranges[r].ModuleName.c_str());
            else
                ShowMessages("  range%d     : 0x%llx - 0x%llx\n", r + 1, Ranges[r].Start, Ranges[r].End);
        }

        //
        // Set the PtRequest structure for filter operation
        //
        PtRequest->FilterOptions.AddrRanges[r].IsStopRange = FALSE;

        PtRequest->FilterOptions.AddrRanges[r].Start = Ranges[r].Start;
        PtRequest->FilterOptions.AddrRanges[r].End   = Ranges[r].End;
    }

    for (INT32 r = 0; r < 4; r++)
    {
        if (StopRanges[r].Active)
        {
            if (StopRanges[r].IsModule)
                ShowMessages("  stoprange%d : module '%s'\n", r + 1, StopRanges[r].ModuleName.c_str());
            else
                ShowMessages("  stoprange%d : 0x%llx - 0x%llx\n", r + 1, StopRanges[r].Start, StopRanges[r].End);
        }

        //
        // Set the PtRequest structure for filter operation
        //
        PtRequest->FilterOptions.AddrRanges[r].IsStopRange = TRUE;

        PtRequest->FilterOptions.AddrRanges[r].Start = Ranges[r].Start;
        PtRequest->FilterOptions.AddrRanges[r].End   = Ranges[r].End;
    }

    //
    // Set the PtRequest structure for filter operation
    //
    PtRequest->PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_FILTER;
}

/**
 * @brief Parse and display !pt packet parameters
 * @param CommandTokens The command tokens to parse
 * @param PtRequest The PT request structure to fill with parsed options
 *
 * @return VOID
 */
static VOID
CommandPtParsePacket(vector<CommandToken> & CommandTokens, HYPERTRACE_PT_OPERATION_PACKETS * PtRequest)
{
    if (CommandTokens.size() < 3)
    {
        ShowMessages("err, 'packet' requires at least one option\n\n");
        CommandPtHelp();
        return;
    }

    BOOLEAN PktPsb  = FALSE;
    BOOLEAN PktPip  = FALSE;
    BOOLEAN PktTsc  = FALSE;
    BOOLEAN PktMtc  = FALSE;
    BOOLEAN PktCyc  = FALSE;
    BOOLEAN PktTnt  = FALSE;
    BOOLEAN PktTip  = FALSE;
    BOOLEAN PktFup  = FALSE;
    BOOLEAN PktMode = FALSE;

    for (SIZE_T i = 2; i < CommandTokens.size(); i++)
    {
        if (CompareLowerCaseStrings(CommandTokens.at(i), "psb"))
            PktPsb = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "pip"))
            PktPip = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "tsc"))
            PktTsc = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "mtc"))
            PktMtc = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "cyc"))
            PktCyc = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "tnt"))
            PktTnt = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "tip"))
            PktTip = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "fup"))
            PktFup = TRUE;
        else if (CompareLowerCaseStrings(CommandTokens.at(i), "mode"))
            PktMode = TRUE;
        else
        {
            ShowMessages("err, unknown 'packet' option '%s'\n\n",
                         GetCaseSensitiveStringFromCommandToken(CommandTokens.at(i)).c_str());
            CommandPtHelp();
            return;
        }
    }

    //
    // Show parsed packet options
    //
    ShowMessages("PT packet filter:\n");

    if (PktPsb)
    {
        ShowMessages("  packet type: PSB  (Packet Stream Boundary)\n");

        PtRequest->PacketOptions.PSB = 1;
    }
    if (PktPip)
    {
        ShowMessages("  packet type: PIP  (Paging Information Packet)\n");

        PtRequest->PacketOptions.PIP = 1;
    }
    if (PktTsc)
    {
        ShowMessages("  packet type: TSC  (Timestamp Counter)\n");

        PtRequest->PacketOptions.TSC = 1;
    }
    if (PktMtc)
    {
        ShowMessages("  packet type: MTC  (Mini Timestamp Counter)\n");

        PtRequest->PacketOptions.MTC = 1;
    }
    if (PktCyc)
    {
        ShowMessages("  packet type: CYC  (Cycle Counter)\n");

        PtRequest->PacketOptions.CYC = 1;
    }
    if (PktTnt)
    {
        ShowMessages("  packet type: TNT  (Taken/Not-Taken)\n");

        PtRequest->PacketOptions.TNT = 1;
    }
    if (PktTip)
    {
        ShowMessages("  packet type: TIP  (Target IP)\n");

        PtRequest->PacketOptions.TNT = 1;
    }
    if (PktFup)
    {
        ShowMessages("  packet type: FUP  (Flow Update Packet)\n");

        PtRequest->PacketOptions.FUP = 1;
    }
    if (PktMode)
    {
        ShowMessages("  packet type: MODE (Mode packet)\n");

        PtRequest->PacketOptions.MODE = 1;
    }

    //
    // Set the PtRequest structure for packet operation
    //
    PtRequest->PtOperationType = HYPERTRACE_PT_OPERATION_REQUEST_TYPE_PACKET;
}

/**
 * @brief !pt command handler
 *
 * @param CommandTokens
 * @param Command
 *
 * @return VOID
 */
VOID
CommandPt(vector<CommandToken> CommandTokens, string Command)
{
    HYPERTRACE_PT_OPERATION_PACKETS PtRequest = {};

    if (CommandTokens.size() == 1)
    {
        ShowMessages("incorrect use of the '%s'\n\n",
                     GetCaseSensitiveStringFromCommandToken(CommandTokens.at(0)).c_str());
        CommandPtHelp();
        return;
    }

    //
    // Parse subcommands
    //
    if (CompareLowerCaseStrings(CommandTokens.at(1), "enable"))
    {
        //
        // Parse and display enable options for !pt enable command
        //
        CommandPtParseEnable(CommandTokens, &PtRequest);
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "disable"))
    {
        if (CommandTokens.size() != 2)
        {
            ShowMessages("err, 'disable' takes no arguments\n\n");
            CommandPtHelp();
            return;
        }

        //
        // Parse and display disable options for !pt disable command
        //
        CommandPtSendDisable();
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "pause"))
    {
        if (CommandTokens.size() != 2)
        {
            ShowMessages("err, 'pause' takes no arguments\n\n");
            CommandPtHelp();
            return;
        }

        //
        // Parse and display pause options for !pt pause command
        //
        CommandPtSendPause();
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "resume"))
    {
        if (CommandTokens.size() != 2)
        {
            ShowMessages("err, 'resume' takes no arguments\n\n");
            CommandPtHelp();
            return;
        }

        //
        // Parse and display resume options for !pt resume command
        //
        CommandPtSendResume();
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "flush"))
    {
        if (CommandTokens.size() != 2)
        {
            ShowMessages("err, 'flush' takes no arguments\n\n");
            CommandPtHelp();
            return;
        }

        //
        // Parse and display flush options for !pt flush command
        //
        CommandPtSendFlush();
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "dump"))
    {
        //
        // Parse and display dump options for !pt dump command
        //
        CommandPtParseDump(CommandTokens, &PtRequest);
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "filter"))
    {
        //
        // Parse and display filter options for !pt filter command
        //
        CommandPtParseFilter(CommandTokens, &PtRequest);
    }
    else if (CompareLowerCaseStrings(CommandTokens.at(1), "packet"))
    {
        //
        // Parse and display packet options for !pt packet command
        //
        CommandPtParsePacket(CommandTokens, &PtRequest);
    }
    else
    {
        ShowMessages("incorrect use of the '%s'\n\n",
                     GetCaseSensitiveStringFromCommandToken(CommandTokens.at(0)).c_str());
        CommandPtHelp();
    }

    //
    // Send the PT request to the debugger
    //
    CommandPtSendRequest(&PtRequest);
}
