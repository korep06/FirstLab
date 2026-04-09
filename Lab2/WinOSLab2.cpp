#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

LPVOID g_address = NULL;
SIZE_T g_size = 4096;
bool g_reserved = false;
bool g_committed = false;

string GetStateName(DWORD state) {
    switch (state) {
    case MEM_COMMIT: return "MEM_COMMIT";
    case MEM_RESERVE: return "MEM_RESERVE";
    case MEM_FREE: return "MEM_FREE";
    default: return "UNKNOWN";
    }
}

string GetTypeName(DWORD type) {
    switch (type) {
    case MEM_PRIVATE: return "MEM_PRIVATE";
    case MEM_MAPPED: return "MEM_MAPPED";
    case MEM_IMAGE: return "MEM_IMAGE";
    default: return "UNKNOWN";
    }
}

string GetProtectName(DWORD protect) {
    DWORD base = protect & 0xFF;

    switch (base) {
    case PAGE_NOACCESS: return "PAGE_NOACCESS";
    case PAGE_READONLY: return "PAGE_READONLY";
    case PAGE_READWRITE: return "PAGE_READWRITE";
    case PAGE_WRITECOPY: return "PAGE_WRITECOPY";
    case PAGE_EXECUTE: return "PAGE_EXECUTE";
    case PAGE_EXECUTE_READ: return "PAGE_EXECUTE_READ";
    case PAGE_EXECUTE_READWRITE: return "PAGE_EXECUTE_READWRITE";
    case PAGE_EXECUTE_WRITECOPY: return "PAGE_EXECUTE_WRITECOPY";
    default: return "UNKNOWN";
    }
}

bool CanWrite(DWORD protect) {
    if (protect & PAGE_GUARD) return false;

    DWORD base = protect & 0xFF;
    return base == PAGE_READWRITE ||
        base == PAGE_WRITECOPY ||
        base == PAGE_EXECUTE_READWRITE ||
        base == PAGE_EXECUTE_WRITECOPY;
}

void ShowSystemInfo() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    cout << "\n=== GetSystemInfo ===\n";
    cout << "Processor architecture: ";
    switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL: cout << "x86"; break;
    case PROCESSOR_ARCHITECTURE_AMD64: cout << "x64"; break;
    case PROCESSOR_ARCHITECTURE_ARM: cout << "ARM"; break;
    case PROCESSOR_ARCHITECTURE_ARM64: cout << "ARM64"; break;
    default: cout << "Unknown";
    }
    cout << endl;

    cout << "Page size: " << si.dwPageSize << " bytes" << endl;
    cout << "Allocation granularity: " << si.dwAllocationGranularity << " bytes" << endl;
    cout << "Min application address: " << si.lpMinimumApplicationAddress << endl;
    cout << "Max application address: " << si.lpMaximumApplicationAddress << endl;
    cout << "Number of processors: " << si.dwNumberOfProcessors << endl;
}

void ShowMemoryStatus() {
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);

    cout << "\n=== GlobalMemoryStatus ===\n";
    cout << "Memory load: " << ms.dwMemoryLoad << "%" << endl;
    cout << "Total physical memory: " << ms.dwTotalPhys / (1024 * 1024) << " MB" << endl;
    cout << "Available physical memory: " << ms.dwAvailPhys / (1024 * 1024) << " MB" << endl;
    cout << "Total page file: " << ms.dwTotalPageFile / (1024 * 1024) << " MB" << endl;
    cout << "Available page file: " << ms.dwAvailPageFile / (1024 * 1024) << " MB" << endl;
    cout << "Total virtual memory: " << ms.dwTotalVirtual / (1024 * 1024) << " MB" << endl;
    cout << "Available virtual memory: " << ms.dwAvailVirtual / (1024 * 1024) << " MB" << endl;
}

void CheckMemoryState() {
    LPVOID addr;
    cout << "\nEnter address in hex: ";
    cin >> hex >> addr;
    cin >> dec;

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQuery(addr, &mbi, sizeof(mbi));

    if (result == 0) {
        cout << "VirtualQuery failed. Error: " << GetLastError() << endl;
        return;
    }

    cout << "\n=== VirtualQuery ===\n";
    cout << "BaseAddress: " << mbi.BaseAddress << endl;
    cout << "AllocationBase: " << mbi.AllocationBase << endl;
    cout << "RegionSize: " << mbi.RegionSize << " bytes" << endl;
    cout << "State: " << GetStateName(mbi.State) << endl;
    cout << "Protect: " << GetProtectName(mbi.Protect) << endl;
    cout << "Type: " << GetTypeName(mbi.Type) << endl;
}

void AllocateSeparate() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    int mode;
    LPVOID addr = NULL;

    cout << "\n=== Separate allocation ===\n";
    cout << "Enter size in bytes: ";
    cin >> g_size;

    if (g_size % si.dwPageSize != 0) {
        g_size = ((g_size + si.dwPageSize - 1) / si.dwPageSize) * si.dwPageSize;
        cout << "Size aligned to page size: " << g_size << " bytes\n";
    }

    cout << "Address mode: 0 - automatic, 1 - manual: ";
    cin >> mode;

    if (mode == 1) {
        cout << "Enter address in hex: ";
        cin >> hex >> addr;
        cin >> dec;
    }

    g_address = VirtualAlloc(addr, g_size, MEM_RESERVE, PAGE_READWRITE);

    if (!g_address) {
        cout << "Reserve failed. Error: " << GetLastError() << endl;
        return;
    }

    g_reserved = true;
    g_committed = false;

    cout << "Memory reserved at address: " << g_address << endl;

    LPVOID commitAddr = VirtualAlloc(g_address, g_size, MEM_COMMIT, PAGE_READWRITE);

    if (!commitAddr) {
        cout << "Commit failed. Error: " << GetLastError() << endl;
        VirtualFree(g_address, 0, MEM_RELEASE);
        g_address = NULL;
        g_reserved = false;
        return;
    }

    g_committed = true;
    cout << "Memory committed successfully.\n";
}

void AllocateCombined() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    int mode;
    LPVOID addr = NULL;

    cout << "\n=== Combined allocation ===\n";
    cout << "Enter size in bytes: ";
    cin >> g_size;

    if (g_size % si.dwPageSize != 0) {
        g_size = ((g_size + si.dwPageSize - 1) / si.dwPageSize) * si.dwPageSize;
        cout << "Size aligned to page size: " << g_size << " bytes\n";
    }

    cout << "Address mode: 0 - automatic, 1 - manual: ";
    cin >> mode;

    if (mode == 1) {
        cout << "Enter address in hex: ";
        cin >> hex >> addr;
        cin >> dec;
    }

    g_address = VirtualAlloc(addr, g_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (!g_address) {
        cout << "Allocation failed. Error: " << GetLastError() << endl;
        return;
    }

    g_reserved = true;
    g_committed = true;

    cout << "Memory allocated at address: " << g_address << endl;
}

void WriteToMemory() {
    LPVOID addr;
    cout << "\n=== Write to memory ===\n";
    cout << "Enter address in hex: ";
    cin >> hex >> addr;
    cin >> dec;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
        cout << "VirtualQuery failed. Error: " << GetLastError() << endl;
        return;
    }

    if (mbi.State != MEM_COMMIT) {
        cout << "Memory is not committed.\n";
        return;
    }

    if (!CanWrite(mbi.Protect)) {
        cout << "Current protection does not allow writing.\n";
        return;
    }

    char value;
    cout << "Enter symbol to write: ";
    cin >> value;

    char* ptr = (char*)addr;
    *ptr = value;

    cout << "Data written successfully.\n";
    cout << "Read check: " << *ptr << endl;
}

void ChangeProtection() {
    LPVOID addr;
    SIZE_T size;

    cout << "\n=== VirtualProtect ===\n";
    cout << "Enter address in hex: ";
    cin >> hex >> addr;
    cin >> dec;

    cout << "Enter region size in bytes: ";
    cin >> size;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
        cout << "VirtualQuery failed. Error: " << GetLastError() << endl;
        return;
    }

    if (mbi.State != MEM_COMMIT) {
        cout << "Protection can be changed only for committed memory.\n";
        return;
    }

    cout << "Current protection: " << GetProtectName(mbi.Protect) << endl;
    cout << "Choose new protection:\n";
    cout << "1 - PAGE_READONLY\n";
    cout << "2 - PAGE_READWRITE\n";
    cout << "3 - PAGE_NOACCESS\n";

    int choice;
    cin >> choice;

    DWORD newProtect;
    switch (choice) {
    case 1: newProtect = PAGE_READONLY; break;
    case 2: newProtect = PAGE_READWRITE; break;
    case 3: newProtect = PAGE_NOACCESS; break;
    default:
        cout << "Wrong choice.\n";
        return;
    }

    DWORD oldProtect;
    if (!VirtualProtect(addr, size, newProtect, &oldProtect)) {
        cout << "VirtualProtect failed. Error: " << GetLastError() << endl;
        return;
    }

    cout << "Protection changed successfully.\n";
    cout << "Old protection: " << GetProtectName(oldProtect) << endl;
    cout << "New protection: " << GetProtectName(newProtect) << endl;

    VirtualQuery(addr, &mbi, sizeof(mbi));
    cout << "Check after change: " << GetProtectName(mbi.Protect) << endl;
}

void FreeMemory() {
    cout << "\n=== VirtualFree ===\n";

    if (!g_address) {
        cout << "No allocated region.\n";
        return;
    }

    if (g_committed) {
        if (VirtualFree(g_address, g_size, MEM_DECOMMIT)) {
            cout << "Memory decommitted successfully.\n";
            g_committed = false;
        }
        else {
            cout << "MEM_DECOMMIT failed. Error: " << GetLastError() << endl;
            return;
        }
    }

    if (g_reserved) {
        if (VirtualFree(g_address, 0, MEM_RELEASE)) {
            cout << "Memory released successfully.\n";
            g_reserved = false;
            g_address = NULL;
            g_size = 4096;
        }
        else {
            cout << "MEM_RELEASE failed. Error: " << GetLastError() << endl;
        }
    }
}

int main() {
    int choice;

    do {
        cout << "\n========================================\n";
        cout << "Lab work 2. Memory Management (Win32)\n";
        cout << "========================================\n";
        cout << "1. System information (GetSystemInfo)\n";
        cout << "2. Virtual memory status (GlobalMemoryStatus)\n";
        cout << "3. Check memory state (VirtualQuery)\n";
        cout << "4. Allocate memory separately (RESERVE -> COMMIT)\n";
        cout << "5. Allocate memory together (RESERVE | COMMIT)\n";
        cout << "6. Write data to memory\n";
        cout << "7. Change memory protection (VirtualProtect)\n";
        cout << "8. Free memory (VirtualFree)\n";
        cout << "0. Exit\n";
        cout << "Your choice: ";

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(32767, '\n');
            cout << "Input error.\n";
            continue;
        }

        switch (choice) {
        case 1: ShowSystemInfo(); break;
        case 2: ShowMemoryStatus(); break;
        case 3: CheckMemoryState(); break;
        case 4: AllocateSeparate(); break;
        case 5: AllocateCombined(); break;
        case 6: WriteToMemory(); break;
        case 7: ChangeProtection(); break;
        case 8: FreeMemory(); break;
        case 0: cout << "Exit.\n"; break;
        default: cout << "Wrong menu item.\n";
        }

    } while (choice != 0);

    return 0;
}