#include <iostream>
#include <iomanip>
#include <omp.h>

using namespace std;

const int N = 100000000;
const int STUD_NUM = 431409;
const int BLOCK_SIZE = 10 * STUD_NUM;

int main() {
    int thr;
    double sum = 0.0;

    cout << "Enter threads count: ";
    cin >> thr;

    if (thr <= 0) {
        cout << "Incorrect threads count." << endl;
        return 1;
    }

    cout << "N = " << N << ", block_size = " << BLOCK_SIZE << endl;
    cout << "blocks = " << (N + BLOCK_SIZE - 1) / BLOCK_SIZE << endl;
    cout << "Testing with " << thr << " threads..." << endl;

    omp_set_num_threads(thr);

    double start_time = omp_get_wtime();

#pragma omp parallel for schedule(dynamic, BLOCK_SIZE) reduction(+:sum)
    for (int i = 0; i < N; i++) {
        double x = (i + 0.5) / N;
        sum += 4.0 / (1.0 + x * x);
    }

    double end_time = omp_get_wtime();
    double duration = (end_time - start_time) * 1000.0;

    double pi = sum / N;

    cout << "Threads: " << thr
        << ", Time: " << duration << " ms"
        << ", PI = " << setprecision(17) << pi << endl;

    return 0;
}