#include <iostream>
#include <Windows.h>
#include <math.h>
#include <chrono>
#include <iomanip>

using namespace std;

const int N = 100000000;
const int STUD_NUM = 431409;

double sum = 0.0;
volatile int current_block = 0;
volatile int completed_blocks = 0;

int blocks;
int block_size;
int thr;

CRITICAL_SECTION cs;
bool* thread_sleeping;

double summerise(int block_num) {
    double part_res = 0.0;

    int start = block_num * block_size;
    int end = start + block_size;

    if (end > N) {
        end = N;
    }

    for (int i = start; i < end; i++) {
        double x = (i + 0.5) / N;
        part_res += 4.0 / (1.0 + x * x);
    }

    return part_res;
}

DWORD WINAPI ThreadProc(LPVOID lpParameter) {
    int idx = *(int*)lpParameter;

    while (true) {
        int block_num;

        EnterCriticalSection(&cs);
        block_num = current_block;
        if (block_num < blocks) {
            current_block++;
        }
        LeaveCriticalSection(&cs);

        if (block_num >= blocks) {
            thread_sleeping[idx] = true;
            SuspendThread(GetCurrentThread());
            continue;
        }

        double part = summerise(block_num);

        EnterCriticalSection(&cs);
        sum += part;
        completed_blocks++;
        LeaveCriticalSection(&cs);

        thread_sleeping[idx] = true;
        SuspendThread(GetCurrentThread());
    }

    return 0;
}

int main() {
    cout << "Enter threads count: ";
    cin >> thr;

    if (thr <= 0) {
        cout << "Incorrect threads count." << endl;
        return 1;
    }

    block_size = 10 * STUD_NUM;
    blocks = (N + block_size - 1) / block_size;

    cout << "N = " << N << ", block_size = " << block_size << ", blocks = " << blocks << endl;
    cout << "Testing with " << thr << " threads" << endl;

    InitializeCriticalSection(&cs);

    sum = 0.0;
    current_block = 0;
    completed_blocks = 0;

    HANDLE* threads = new HANDLE[thr];
    thread_sleeping = new bool[thr];
    int* indices = new int[thr];

    for (int i = 0; i < thr; i++) {
        indices[i] = i;
        thread_sleeping[i] = true;

        threads[i] = CreateThread(NULL, 0, ThreadProc, &indices[i], CREATE_SUSPENDED, NULL);

        if (threads[i] == NULL) {
            cout << "Thread " << i << " creating error." << endl;
            return 1;
        }
    }

    auto start_time = chrono::high_resolution_clock::now();

    for (int i = 0; i < thr && i < blocks; i++) {
        thread_sleeping[i] = false;
        ResumeThread(threads[i]);
    }

    while (completed_blocks < blocks) {
        for (int i = 0; i < thr; i++) {
            if (thread_sleeping[i] && current_block < blocks) {
                thread_sleeping[i] = false;
                ResumeThread(threads[i]);
            }
        }
        Sleep(1);
    }

    auto end_time = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);

    double pi = sum / N;

    cout << "Threads: " << thr
        << ", Time: " << duration.count() << " ms"
        << ", PI = " << setprecision(17) << pi << endl;

    for (int i = 0; i < thr; i++) {
        TerminateThread(threads[i], 0);
        CloseHandle(threads[i]);
    }

    delete[] threads;
    delete[] thread_sleeping;
    delete[] indices;

    DeleteCriticalSection(&cs);

    return 0;
}