#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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

    while (running) {
        cout << "\n--- Server Menu ---\n";
        cout << "1. Выполнить проецирование\n";
        cout << "2. Записать данные\n";
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

            fd = open(FILENAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            if (fd == -1) {
                perror("open");
                break;
            }

            if (ftruncate(fd, FILESIZE) == -1) {
                perror("ftruncate");
                close(fd);
                fd = -1;
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
            shared->ready = 0;
            shared->read_confirm = 0;
            shared->data[0] = '\0';

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

            cout << "Введите сообщение для клиента: ";
            {
                string message;
                getline(cin, message);

                strncpy(shared->data, message.c_str(), sizeof(shared->data) - 1);
                shared->data[sizeof(shared->data) - 1] = '\0';
            }

            shared->read_confirm = 0;
            shared->ready = 1;

            cout << "Данные записаны в общую память.\n";
            cout << "Ожидание чтения клиентом...\n";

            while (shared->ready == 1 && shared->read_confirm == 0) {
                usleep(100000);
            }

            if (shared->read_confirm == 1) {
                cout << "Клиент прочитал сообщение.\n";
                shared->ready = 0;
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

    if (unlink(FILENAME) == -1) {
        perror("unlink");
    }

    cout << "Сервер завершил работу.\n";
    return 0;
}