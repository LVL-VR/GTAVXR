#pragma once
// GTAVXR 1.0.2's patched game exe carries an export-directory entry of RVA 0x277c0fc, size 4 that points into its
// import table. Read as IMAGE_EXPORT_DIRECTORY it claims ~41M names at RVA 0, so any GetProcAddress on the exe
// (d3d12.dll's D3D12SDKVersion probe at load, and again at exit) faults in ntdll!LdrpResolveProcedureAddress:
// d3d12.dll then fails with ERROR_DLL_INIT_FAILED and the DLSS chain falls back to a bilinear blit.
// An entry smaller than one IMAGE_EXPORT_DIRECTORY cannot describe exports, so it is cleared in memory only
// (as in retail GTA5.exe, which has none). The file on disk, and any valid export directory, are left alone.
#include <windows.h>
#include <string>

namespace gx_exe {
// Returns a log line when the entry was repaired, empty otherwise.
inline std::string repair_export_directory() {
    auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!base) return {};
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return {};
    IMAGE_DATA_DIRECTORY& e = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (e.VirtualAddress == 0 || e.Size >= sizeof(IMAGE_EXPORT_DIRECTORY)) return {};
    const DWORD rva = e.VirtualAddress, size = e.Size;
    DWORD old = 0;
    if (!VirtualProtect(&e, sizeof(e), PAGE_READWRITE, &old)) return "EXE_EXPORT_DIR_REPAIR_FAILED win32=" + std::to_string(GetLastError());
    e.VirtualAddress = 0; e.Size = 0;
    VirtualProtect(&e, sizeof(e), old, &old);
    char text[96];
    sprintf_s(text, "EXE_EXPORT_DIR_REPAIRED rva=0x%lx size=%lu", static_cast<unsigned long>(rva), static_cast<unsigned long>(size));
    return text;
}
}
