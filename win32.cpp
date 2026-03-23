#include <Windows.h>
#include <iostream>
#include <bitset>
#include <cstring>

void PrintLastError(const char* msg);

static void PrintSystemTimeLocal(const FILETIME& ft) {
    SYSTEMTIME stUTC{}, stLocal{};
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

    std::cout << stLocal.wDay << "."
        << stLocal.wMonth << "."
        << stLocal.wYear << " "
        << stLocal.wHour << "ч:"
        << stLocal.wMinute << "м:"
        << stLocal.wSecond << "с:"
        << stLocal.wMilliseconds << "мс" << std::endl;
}

static void PrintAttributesDetailed(DWORD attrs) {
    std::cout << "Общая маска (DWORD): " << std::bitset<32>(attrs) << "\n\n";

    struct AttrInfo {
        DWORD flag;
        const char* name;
    };

    AttrInfo list[] = {
        {FILE_ATTRIBUTE_READONLY,  "READONLY"},
        {FILE_ATTRIBUTE_HIDDEN,    "HIDDEN"},
        {FILE_ATTRIBUTE_SYSTEM,    "SYSTEM"},
        {FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY"},
        {FILE_ATTRIBUTE_ARCHIVE,   "ARCHIVE"},
        {FILE_ATTRIBUTE_NORMAL,    "NORMAL"}
    };

    std::cout << "Атрибуты:\n";

    for (const auto& attr : list) {
        std::cout << attr.name << ":\n";
        std::cout << "  Бит: " << std::bitset<32>(attr.flag) << "\n";
        std::cout << "  Состояние: "
            << ((attrs & attr.flag) ? "ВКЛ" : "ВЫКЛ") << "\n\n";
    }
}

void ShowFileInfoByHandle() {
    char path[260];

    std::cout << "Введите путь к файлу: ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        PrintLastError("Не удалось открыть файл");
        return;
    }

    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(hFile, &info)) {
        PrintLastError("GetFileInformationByHandle не сработала");
        CloseHandle(hFile);
        return;
    }

    unsigned long long fileSize = (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;

    std::cout << "\nИнформация о файле (через HANDLE):\n";
    std::cout << "Путь: " << path << "\n";
    std::cout << "Размер: " << fileSize << " байт\n";
    std::cout << "Кол-во ссылок (hard links): " << info.nNumberOfLinks << "\n";
    std::cout << "Серийный номер тома: " << info.dwVolumeSerialNumber << "\n";

    unsigned long long fileIndex = (static_cast<unsigned long long>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
    std::cout << "FileIndex (ID файла на томе): " << fileIndex << "\n";

    std::cout << "Атрибуты файла:\n";
    PrintAttributesDetailed(info.dwFileAttributes);

    std::cout << "Время создания: ";
    PrintSystemTimeLocal(info.ftCreationTime);
    std::cout << "\n";

    std::cout << "Последний доступ: ";
    PrintSystemTimeLocal(info.ftLastAccessTime);
    std::cout << "\n";

    std::cout << "Последняя запись: ";
    PrintSystemTimeLocal(info.ftLastWriteTime);
    std::cout << "\n";

    CloseHandle(hFile);
}

void ChangeFileTimes() {
    char path[260];

    std::cout << "Введите путь к файлу: ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    HANDLE hFile = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        PrintLastError("Не удалось открыть файл");
        return;
    }

    SYSTEMTIME st;
    FILETIME ft;

    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);
    if (!SetFileTime(hFile, &ft, &ft, &ft))
        PrintLastError("Не удалось изменить время");

    std::cout << "Время файла успешно обновлено!\n";

    CloseHandle(hFile);
}

void ShowFileTimes() {
    char path[260];

    std::cout << "Введите путь к файлу: ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        PrintLastError("Не удалось открыть файл");
        return;
    }

    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (!GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
        PrintLastError("Не удалось получить времена файла");
        CloseHandle(hFile);
        return;
    }

    std::cout << "Время создания:";
    PrintSystemTimeLocal(creationTime);

    std::cout << "Последний доступ: ";
    PrintSystemTimeLocal(lastAccessTime);

    std::cout << "Последняя запись: ";
    PrintSystemTimeLocal(lastWriteTime);

    CloseHandle(hFile);
}

void ChangeFileAttributes() {
    char path[260];

    std::cout << "Введите путь к файлу или каталогу: ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        PrintLastError("Не удалось получить текущие атрибуты");
        return;
    }

    std::cout << "Текущие атрибуты:\n";
    PrintAttributesDetailed(attrs);

    char choice;
    std::cout << "Сделать файл Только для чтения? (y/n): ";
    std::cin >> choice;
    if (choice == 'y' || choice == 'Y')
        attrs |= FILE_ATTRIBUTE_READONLY;
    else
        attrs &= ~FILE_ATTRIBUTE_READONLY;

    std::cout << "Сделать файл Скрытый? (y/n): ";
    std::cin >> choice;
    if (choice == 'y' || choice == 'Y')
        attrs |= FILE_ATTRIBUTE_HIDDEN;
    else
        attrs &= ~FILE_ATTRIBUTE_HIDDEN;

    std::cout << "Сделать файл Архивный? (y/n): ";
    std::cin >> choice;
    if (choice == 'y' || choice == 'Y')
        attrs |= FILE_ATTRIBUTE_ARCHIVE;
    else
        attrs &= ~FILE_ATTRIBUTE_ARCHIVE;

    if (SetFileAttributesA(path, attrs)) {
        std::cout << "Атрибуты успешно изменены!\n";
        DWORD newAttrs = GetFileAttributesA(path);
        if (newAttrs != INVALID_FILE_ATTRIBUTES) {
            std::cout << "Новые атрибуты:\n";
            PrintAttributesDetailed(newAttrs);
        }
    }
    else {
        PrintLastError("Не удалось изменить атрибуты");
    }
}

void ShowFileAttributes() {
    char path[260];

    std::cout << "Введите путь к файлу или каталогу: ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        PrintLastError("Не удалось получить атрибуты");
        return;
    }

    std::cout << "Атрибуты файла " << path << ":\n";
    PrintAttributesDetailed(attrs);
}

void MoveFileWithCheck() {
    char path_cur[260];
    char path_move[260];

    std::cout << "Введите путь исходного файла: ";
    std::cin.ignore();
    std::cin.getline(path_cur, 260);

    std::cout << "Введите путь назначения (папка + имя файла): ";
    std::cin.getline(path_move, 260);

    if (MoveFileA(path_cur, path_move)) {
        std::cout << "Файл успешно перемещён!\n";
    }
    else {
        DWORD error = GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            std::cout << "Ошибка: файл назначения уже существует!\n";
        }
        else if (error == ERROR_FILE_NOT_FOUND) {
            std::cout << "Ошибка: исходный файл не найден!\n";
        }
        else {
            PrintLastError("Не удалось переместить файл");
        }
    }
}

void MoveFileExWithCheck() {
    char path_cur[260], path_move[260];

    std::cout << "Введите путь исходного файла: ";
    std::cin.ignore();
    std::cin.getline(path_cur, 260);

    std::cout << "Введите путь назначения: ";
    std::cin.getline(path_move, 260);

    if (MoveFileExA(path_cur, path_move, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
        std::cout << "Файл успешно перемещён через MoveFileEx!\n";
    }
    else {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
            std::cout << "Исходный файл не найден!\n";
        else
            PrintLastError("Не удалось переместить файл через MoveFileEx");
    }
}

void CopyFileWithCheck() {
    char path_cur[260];
    char path_copy[260];

    std::cout << "Введите путь исходного файла: ";
    std::cin.ignore();
    std::cin.getline(path_cur, 260);

    std::cout << "Введите путь файла назначения: ";
    std::cin.getline(path_copy, 260);

    if (CopyFileA(path_cur, path_copy, TRUE)) {
        std::cout << "Файл успешно скопирован!\n";
    }
    else {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS) {
            std::cout << "Ошибка: файл назначения уже существует!\n";
        }
        else if (error == ERROR_FILE_NOT_FOUND) {
            std::cout << "Ошибка: исходный файл не найден!\n";
        }
        else {
            PrintLastError("Не удалось скопировать файл");
        }
    }
}

void CreateFileInFolder() {
    char path[260];

    std::cout << "Введите полный путь и имя файла для создания (например C:\\Temp\\file.txt): ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    HANDLE new_file = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (new_file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS) {
            std::cout << "Файл уже существует!\n";
        }
        else {
            PrintLastError("Не удалось создать файл");
        }
    }
    else {
        std::cout << "Файл успешно создан: " << path << "\n";
        CloseHandle(new_file);
    }
}

void RemoveFolder() {
    char path[260];

    std::cout << "Введите путь каталога для удаления: ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    if (RemoveDirectoryA(path)) {
        std::cout << "Каталог успешно удалён: " << path << "\n";
    }
    else {
        DWORD error = GetLastError();
        if (error == ERROR_DIR_NOT_EMPTY) {
            std::cout << "Не удалось удалить каталог: он не пустой!\n";
        }
        else {
            PrintLastError("Не удалось удалить каталог");
        }
    }
}

void CreateFolder() {
    char path[260];

    std::cout << "Введите путь для создания каталога, например C:\\NewFolder): ";
    std::cin.ignore();
    std::cin.getline(path, 260);

    if (CreateDirectoryA(path, nullptr)) {
        std::cout << "Каталог успешно создан: " << path << "\n";
    }
    else {
        PrintLastError("Не удалось создать каталог");
    }
}

void ShowDiskInfo(const char* drive) {
    DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;

    if (!GetDiskFreeSpaceA(drive, &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters)) {
        PrintLastError("Ошибка получения свободного места диска");
        return;
    }

    unsigned long long freeBytes = (unsigned long long)freeClusters * sectorsPerCluster * bytesPerSector;
    unsigned long long totalBytes = (unsigned long long)totalClusters * sectorsPerCluster * bytesPerSector;

    std::cout << "\nИнформация о диске " << drive << " :\n";
    std::cout << "Всего места: " << totalBytes / (1024 * 1024) << " МБ\n";
    std::cout << "Свободно места: " << freeBytes / (1024 * 1024) << " МБ\n";

    char volumeName[256];
    char fsName[256];

    if (!GetVolumeInformationA(drive, volumeName, sizeof(volumeName),
        nullptr, nullptr, nullptr, fsName, sizeof(fsName))) {
        PrintLastError("Ошибка получения информации о томе");
        return;
    }

    std::cout << "Метка тома: " << (volumeName[0] ? volumeName : "нет метки") << "\n";
    std::cout << "Файловая система: " << fsName << "\n";
}

void PrintLastError(const char* msg = "") {
    DWORD errorCode = GetLastError();
    if (errorCode == 0) {
        return;
    }

    char* errorText = nullptr;

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        (LPSTR)&errorText,
        0,
        nullptr
    );

    std::cout << msg;
    if (msg[0]) std::cout << ": ";
    if (errorText) {
        std::cout << errorText;
        LocalFree(errorText);
    }
    else {
        std::cout << "Неизвестная ошибка, код: " << errorCode;
    }
    std::cout << std::endl;
}

void ShowLogicalDrivesMask() {
    DWORD mask = GetLogicalDrives();
    if (mask == 0) {
        std::cout << "Ошибка получения списка дисков! Код ошибки: " << GetLastError() << "\n";
        return;
    }

    std::cout << "Список дисков (через GetLogicalDrives):\n";

    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        DWORD bitMask = 1 << (drive - 'A');

        if (mask & bitMask) {
            char drivePath[4];
            drivePath[0] = drive;
            drivePath[1] = ':';
            drivePath[2] = '\\';
            drivePath[3] = '\0';

            std::cout << drivePath;

            UINT type = GetDriveTypeA(drivePath);
            switch (type) {
            case DRIVE_REMOVABLE: std::cout << " — Съемный диск"; break;
            case DRIVE_FIXED:     std::cout << " — Жесткий диск"; break;
            case DRIVE_REMOTE:    std::cout << " — Сетевой диск"; break;
            case DRIVE_CDROM:     std::cout << " — CD/DVD"; break;
            case DRIVE_RAMDISK:   std::cout << " — RAM-диск"; break;
            default:              std::cout << " — Неизвестный тип"; break;
            }
            std::cout << "\n";
        }
    }
}

void ShowLogicalDrivesString() {
    char buffer[256];
    DWORD len = GetLogicalDriveStringsA(sizeof(buffer), buffer);

    if (len == 0) {
        std::cout << "Ошибка получения списка дисков! Код ошибки: " << GetLastError() << "\n";
        return;
    }

    std::cout << "Список дисков: (через GetLogicalDriveString)\n";

    char* drive = buffer;
    while (*drive) {
        UINT type = GetDriveTypeA(drive);
        std::cout << drive << " — ";

        switch (type) {
        case DRIVE_REMOVABLE:
            std::cout << "Съемный диск";
            break;
        case DRIVE_FIXED:
            std::cout << "Жесткий диск";
            break;
        case DRIVE_REMOTE:
            std::cout << "Сетевой диск";
            break;
        case DRIVE_CDROM:
            std::cout << "CD/DVD";
            break;
        case DRIVE_RAMDISK:
            std::cout << "RAM диск";
            break;
        default:
            std::cout << "Неизвестный тип";
            break;
        }
        std::cout << "\n";

        drive += strlen(drive) + 1;
    }
}

int main() {
    int choice;
    do {
        std::cout << "\nПрактическая работа №1. Задание первое." << std::endl;
        std::cout << "--- Меню ---\n";
        std::cout << "1. Показать список дисков через GetLogicalDriveString\n";
        std::cout << "2. Показать список дисков через GetLogicalDrives\n";
        std::cout << "3. Показать информацию о диске\n";
        std::cout << "4. Создать каталог\n";
        std::cout << "5. Удалить пустой каталог\n";
        std::cout << "6. Создать файл в каталоге\n";
        std::cout << "7. Копирование файла\n";
        std::cout << "8. Перемещение файла MoveFile\n";
        std::cout << "9. Перемещение файла MoveFileEx\n";
        std::cout << "10. Получение атрибутов файла или каталога\n";
        std::cout << "11. Получение информации о файле через дескриптор\n";
        std::cout << "12. Изменение атрибутов\n";
        std::cout << "13. Получение времён файла\n";
        std::cout << "14. Изменение времен файла\n";
        std::cout << "0. Выход\n";
        std::cout << "Выберите пункт: ";

        std::cin >> choice;

        switch (choice) {
        case 1:
            ShowLogicalDrivesString();
            break;
        case 2:
            ShowLogicalDrivesMask();
            break;
        case 3: {
            char drive[4];
            std::cout << "Введите букву диска, например C,D,F...\n";
            std::cin >> drive[0];
            drive[1] = ':';
            drive[2] = '\\';
            drive[3] = '\0';
            ShowDiskInfo(drive);
            break;
        }
        case 4:
            CreateFolder();
            break;
        case 5:
            RemoveFolder();
            break;
        case 6:
            CreateFileInFolder();
            break;
        case 7:
            CopyFileWithCheck();
            break;
        case 8:
            MoveFileWithCheck();
            break;
        case 9:
            MoveFileExWithCheck();
            break;
        case 10:
            ShowFileAttributes();
            break;
        case 11:
            ShowFileInfoByHandle();
            break;
        case 12:
            ChangeFileAttributes();
            break;
        case 13:
            ShowFileTimes();
            break;
        case 14:
            ChangeFileTimes();
            break;
        case 0:
            std::cout << "Выход...\n";
            break;
        default:
            std::cout << "Неверный пункт меню!\n";
            break;
        }
    } while (choice != 0);

    return 0;
}
