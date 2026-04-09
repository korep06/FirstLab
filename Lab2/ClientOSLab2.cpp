#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>

using namespace std;

const char* FILENAME = "shared_mem_file";
const int FILESIZE = 4096;

struct SharedData {
    int ready;
    int read_confirm;
    char data[256];
};

int main() {
    int fd = -1;
    void* ptr = nullptr;
    SharedData* shared = nullptr;
    bool mapped = false;
    bool running = true;
    int choice;

    fd_set read_fds;
    timeval timeout{};

    while (running) {
        cout << "\n--- Client Menu ---\n";
        cout << "1. Выполнить проецирование\n";
        cout << "2. Прочитать данные\n";
        cout << "3. Завершить работу\n";
        cout << "Выбор: ";
        cin >> choice;
        cin.ignore();

        switch (choice) {
        case 1:
            if (mapped) {
                cout << "Файл уже спроецирован.\n";
                break;
            }

            fd = open(FILENAME, O_RDWR);
            if (fd == -1) {
                perror("open");
                cout << "Сначала нужно запустить сервер и выполнить проецирование на сервере.\n";
                break;
            }

            ptr = mmap(nullptr, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (ptr == MAP_FAILED) {
                perror("mmap");
                close(fd);
                fd = -1;
                ptr = nullptr;
                break;
            }

            shared = static_cast<SharedData*>(ptr);
            mapped = true;

            cout << "Проецирование выполнено успешно.\n";
            cout << "Имя файла: " << FILENAME << '\n';
            cout << "Адрес отображения: " << ptr << '\n';
            cout << "Размер: " << FILESIZE << " байт\n";
            break;

        case 2:
            if (!mapped) {
                cout << "Сначала выполните проецирование.\n";
                break;
            }

            cout << "Ожидание доступности чтения...\n";

            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            select(fd + 1, &read_fds, nullptr, nullptr, &timeout);

            if (shared->ready == 1) {
                cout << "Client received: " << shared->data << '\n';
                shared->read_confirm = 1;
                shared->ready = 0;
            } else {
                cout << "Данные пока недоступны.\n";
            }
            break;

        case 3:
            running = false;
            break;

        default:
            cout << "Неверный пункт меню.\n";
        }
    }

    if (mapped) {
        if (munmap(ptr, FILESIZE) == -1) {
            perror("munmap");
        }
    }

    if (fd != -1) {
        close(fd);
    }

    cout << "Клиент завершил работу.\n";
    return 0;
}