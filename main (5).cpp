#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

struct AioOperation
{
    struct aiocb aio;
    char *buffer;
    int write_operation;
    void *next_operation;
};

static int fdInput = -1;
static int fdOutput = -1;

static off_t fileSize = 0;
static size_t blockSize = 0;
static int operationsCount = 0;

static volatile sig_atomic_t copyFinished = 0;
static volatile sig_atomic_t hasError = 0;

static volatile int64_t pendingOperations = 0;
static volatile int64_t totalWritten = 0;
static volatile int64_t nextOffset = 0;

static AioOperation *readOperations = NULL;
static AioOperation *writeOperations = NULL;

static inline int64_t atomicAdd64(volatile int64_t *value, int64_t delta)
{
    return __sync_add_and_fetch(value, delta);
}

static inline int64_t atomicLoad64(volatile int64_t *value)
{
    return __sync_add_and_fetch(value, 0);
}

static inline int64_t atomicFetchAdd64(volatile int64_t *value, int64_t delta)
{
    return __sync_fetch_and_add(value, delta);
}

static void clearResources()
{
    if (readOperations != NULL)
    {
        for (int i = 0; i < operationsCount; i++)
        {
            if (readOperations[i].buffer != NULL)
            {
                free(readOperations[i].buffer);
                readOperations[i].buffer = NULL;
            }
        }

        free(readOperations);
        readOperations = NULL;
    }

    if (writeOperations != NULL)
    {
        free(writeOperations);
        writeOperations = NULL;
    }

    if (fdInput != -1)
    {
        close(fdInput);
        fdInput = -1;
    }

    if (fdOutput != -1)
    {
        close(fdOutput);
        fdOutput = -1;
    }
}

static int startRead(AioOperation *readOp, AioOperation *writeOp, off_t offset);
static int startWrite(AioOperation *writeOp, AioOperation *readOp, char *buffer, size_t bytesCount, off_t offset);
static int submitNextRead(AioOperation *readOp, AioOperation *writeOp);

static int submitNextRead(AioOperation *readOp, AioOperation *writeOp)
{
    int64_t offset = atomicFetchAdd64(&nextOffset, (int64_t)blockSize);

    if (offset >= (int64_t)fileSize)
    {
        return 0;
    }

    return startRead(readOp, writeOp, (off_t)offset);
}

static void aio_completion_handler(sigval_t sigval)
{
    AioOperation *currentOp = (AioOperation *)sigval.sival_ptr;
    ssize_t result = aio_return(&currentOp->aio);

    if (result < 0)
    {
        perror("aio_return");
        hasError = 1;
        atomicAdd64(&pendingOperations, -1);

        if (atomicLoad64(&pendingOperations) == 0)
        {
            copyFinished = 1;
        }

        return;
    }

    if (currentOp->write_operation)
    {
        size_t askedBytes = (size_t)currentOp->aio.aio_nbytes;

        if ((size_t)result < askedBytes)
        {
            char *nextBuffer = (char *)currentOp->aio.aio_buf + result;
            size_t leftBytes = askedBytes - (size_t)result;
            off_t nextWriteOffset = currentOp->aio.aio_offset + result;

            if (startWrite(currentOp,
                           (AioOperation *)currentOp->next_operation,
                           nextBuffer,
                           leftBytes,
                           nextWriteOffset) == -1)
            {
                hasError = 1;
                atomicAdd64(&pendingOperations, -1);

                if (atomicLoad64(&pendingOperations) == 0)
                {
                    copyFinished = 1;
                }
            }
            else
            {
                atomicAdd64(&pendingOperations, -1);
            }

            return;
        }

        atomicAdd64(&totalWritten, result);
        atomicAdd64(&pendingOperations, -1);

        AioOperation *nextReadOp = (AioOperation *)currentOp->next_operation;

        if (!hasError && nextReadOp != NULL)
        {
            if (submitNextRead(nextReadOp, currentOp) == -1)
            {
                hasError = 1;
            }
        }

        if ((atomicLoad64(&totalWritten) >= (int64_t)fileSize &&
             atomicLoad64(&pendingOperations) == 0) || hasError)
        {
            if (atomicLoad64(&pendingOperations) == 0)
            {
                copyFinished = 1;
            }
        }
    }
    else
    {
        AioOperation *writeOp = (AioOperation *)currentOp->next_operation;

        if (writeOp == NULL)
        {
            fprintf(stderr, "Ошибка: для чтения не найдена связанная запись\n");
            hasError = 1;
            atomicAdd64(&pendingOperations, -1);

            if (atomicLoad64(&pendingOperations) == 0)
            {
                copyFinished = 1;
            }

            return;
        }

        if (startWrite(writeOp,
                       currentOp,
                       currentOp->buffer,
                       (size_t)result,
                       currentOp->aio.aio_offset) == -1)
        {
            hasError = 1;
            atomicAdd64(&pendingOperations, -1);

            if (atomicLoad64(&pendingOperations) == 0)
            {
                copyFinished = 1;
            }

            return;
        }

        atomicAdd64(&pendingOperations, -1);
    }
}

static int startRead(AioOperation *readOp, AioOperation *writeOp, off_t offset)
{
    if (offset >= fileSize)
    {
        return 0;
    }

    size_t bytesToRead = blockSize;

    if (offset + (off_t)bytesToRead > fileSize)
    {
        bytesToRead = (size_t)(fileSize - offset);
    }

    memset(&readOp->aio, 0, sizeof(struct aiocb));

    readOp->write_operation = 0;
    readOp->next_operation = writeOp;

    readOp->aio.aio_fildes = fdInput;
    readOp->aio.aio_buf = readOp->buffer;
    readOp->aio.aio_nbytes = bytesToRead;
    readOp->aio.aio_offset = offset;

    readOp->aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    readOp->aio.aio_sigevent.sigev_notify_function = aio_completion_handler;
    readOp->aio.aio_sigevent.sigev_notify_attributes = NULL;
    readOp->aio.aio_sigevent.sigev_value.sival_ptr = readOp;

    if (aio_read(&readOp->aio) == -1)
    {
        perror("aio_read");
        return -1;
    }

    atomicAdd64(&pendingOperations, 1);
    return 1;
}

static int startWrite(AioOperation *writeOp, AioOperation *readOp, char *buffer, size_t bytesCount, off_t offset)
{
    memset(&writeOp->aio, 0, sizeof(struct aiocb));

    writeOp->write_operation = 1;
    writeOp->next_operation = readOp;

    writeOp->aio.aio_fildes = fdOutput;
    writeOp->aio.aio_buf = buffer;
    writeOp->aio.aio_nbytes = bytesCount;
    writeOp->aio.aio_offset = offset;

    writeOp->aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    writeOp->aio.aio_sigevent.sigev_notify_function = aio_completion_handler;
    writeOp->aio.aio_sigevent.sigev_notify_attributes = NULL;
    writeOp->aio.aio_sigevent.sigev_value.sival_ptr = writeOp;

    if (aio_write(&writeOp->aio) == -1)
    {
        perror("aio_write");
        return -1;
    }

    atomicAdd64(&pendingOperations, 1);
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr,
                "Использование: %s <исходный_файл> <новый_файл> <размер_блока> <число_операций>\n",
                argv[0]);
        return 1;
    }

    const char *inputFileName = argv[1];
    const char *outputFileName = argv[2];

    blockSize = (size_t)strtoull(argv[3], NULL, 10);
    operationsCount = atoi(argv[4]);

    if (blockSize == 0)
    {
        fprintf(stderr, "Ошибка: размер блока должен быть больше 0\n");
        return 1;
    }

    if (operationsCount <= 0)
    {
        fprintf(stderr, "Ошибка: число операций должно быть больше 0\n");
        return 1;
    }

    fdInput = open(inputFileName, O_RDONLY | O_NONBLOCK, 0666);
    if (fdInput == -1)
    {
        perror("open input");
        return 1;
    }

    fdOutput = open(outputFileName, O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, 0666);
    if (fdOutput == -1)
    {
        perror("open output");
        clearResources();
        return 1;
    }

    struct stat fileInfo;
    if (fstat(fdInput, &fileInfo) == -1)
    {
        perror("fstat");
        clearResources();
        return 1;
    }

    fileSize = fileInfo.st_size;

    if (fileSize == 0)
    {
        printf("Исходный файл пустой, копировать нечего.\n");
        clearResources();
        return 0;
    }

    readOperations = (AioOperation *)calloc((size_t)operationsCount, sizeof(AioOperation));
    writeOperations = (AioOperation *)calloc((size_t)operationsCount, sizeof(AioOperation));

    if (readOperations == NULL || writeOperations == NULL)
    {
        perror("calloc");
        clearResources();
        return 1;
    }

    for (int i = 0; i < operationsCount; i++)
    {
        readOperations[i].buffer = (char *)malloc(blockSize);
        if (readOperations[i].buffer == NULL)
        {
            perror("malloc");
            clearResources();
            return 1;
        }

        writeOperations[i].buffer = NULL;
        writeOperations[i].next_operation = &readOperations[i];
    }

    struct timespec startTime;
    struct timespec endTime;

    if (clock_gettime(CLOCK_MONOTONIC, &startTime) == -1)
    {
        perror("clock_gettime");
        clearResources();
        return 1;
    }

    for (int i = 0; i < operationsCount; i++)
    {
        int result = submitNextRead(&readOperations[i], &writeOperations[i]);

        if (result == -1)
        {
            hasError = 1;
            break;
        }

        if (result == 0)
        {
            break;
        }
    }

    while (!copyFinished && !hasError)
    {
        const struct aiocb *activeList[256];
        int activeCount = 0;

        if (operationsCount * 2 > 256)
        {
            fprintf(stderr, "Слишком большое число операций\n");
            hasError = 1;
            break;
        }

        for (int i = 0; i < operationsCount; i++)
        {
            if (aio_error(&readOperations[i].aio) == EINPROGRESS)
            {
                activeList[activeCount++] = &readOperations[i].aio;
            }

            if (aio_error(&writeOperations[i].aio) == EINPROGRESS)
            {
                activeList[activeCount++] = &writeOperations[i].aio;
            }
        }

        if (activeCount == 0)
        {
            usleep(1000);
            continue;
        }

        if (aio_suspend(activeList, activeCount, NULL) == -1 && errno != EINTR)
        {
            perror("aio_suspend");
            hasError = 1;
            break;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC, &endTime) == -1)
    {
        perror("clock_gettime");
        clearResources();
        return 1;
    }

    double elapsedTime =
        (double)(endTime.tv_sec - startTime.tv_sec) +
        (double)(endTime.tv_nsec - startTime.tv_nsec) / 1000000000.0;

    if (!hasError && atomicLoad64(&totalWritten) == (int64_t)fileSize)
    {
        printf("Копирование завершено успешно.\n");
        printf("Размер файла: %jd байт\n", (intmax_t)fileSize);
        printf("Размер блока: %zu байт\n", blockSize);
        printf("Число одновременных операций: %d\n", operationsCount);
        printf("Время копирования: %.6f сек\n", elapsedTime);

        if (elapsedTime > 0.0)
        {
            printf("Скорость: %.2f байт/сек\n", (double)fileSize / elapsedTime);
        }
    }
    else
    {
        fprintf(stderr, "Копирование завершилось с ошибкой.\n");
        fprintf(stderr, "Записано: %jd из %jd байт\n",
                (intmax_t)atomicLoad64(&totalWritten),
                (intmax_t)fileSize);
        clearResources();
        return 1;
    }

    clearResources();
    return 0;
}