#include <iostream>
#include <cstring>
#include <windows.h>
#include <tlhelp32.h>
#include <thread>
#include <set>

// Здесь хранятся поинтеры на различные области памяти в клиенте
// Получить эти магические числа можно путем гугления
namespace Pointers
{
static const std::ptrdiff_t p_localPlayer = 0xCBD6B4;

static const std::ptrdiff_t p_glowObjectManager = 0x520DA28;

static const std::ptrdiff_t p_glowObjectManagerSize = p_glowObjectManager + 0xC;

static const std::ptrdiff_t m_iTeamNum = 0xF4;
}

// Данные о процессе
struct ProcessData
{
    HANDLE handle = nullptr;
    DWORD pid = 0;

    bool check()
    {
        return pid != 0 && handle != nullptr;
    }
};

// Получаем процесс по имени
ProcessData getProcess(const char *name)
{
    // Делаем снапшот (получаем все процессы)
    HANDLE process = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    // Точка входа в процесс
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    // Если есть процессы, то начинаем обход
    if (Process32First(process, &entry)) {
        do {
            // Сравниваем имя в стиле Си
            if (strcmp(entry.szExeFile, name) == 0) {
                // Сохраняем ID процесса
                DWORD pid = entry.th32ProcessID;
                // Сохраняем хэндл процесса
                HANDLE resultProc = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

                // Закрываем снапшот
                if (process != INVALID_HANDLE_VALUE) {
                    CloseHandle(process);
                }

                return {resultProc, pid};
            }
        } while (Process32Next(process, &entry));
    }

    // Закрываем снапшот
    if (process != INVALID_HANDLE_VALUE) {
        CloseHandle(process);
    }

    return {nullptr, 0};
}

uintptr_t getModule(const char *name, const ProcessData &data)
{
    // Делаем снапшот модулей процесса
    HANDLE module = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, data.pid);

    // Точка входа (сюда летит информация о модуле)
    MODULEENTRY32 entry;
    entry.dwSize = sizeof(entry);

    // Проходим по модулям
    if (Module32First(module, &entry)) {
        do {
            // Сравниваем имя в стиле Си
            if (strcmp(entry.szModule, name) == 0) {
                // Закрываем снапшот
                if (module != INVALID_HANDLE_VALUE)
                    CloseHandle(module);

                // Возвращаем указатель (преобразованный в число)
                return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
            }
        } while (Module32Next(module, &entry));
    }

    return 0;
}

int mainExit()
{
    system("pause");
    return 0;
}

// Чтобы CLion не ругался на бесконечный цикл
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

// В этой функции происходит обнаружение противника
void mainThread(const ProcessData * const data, const uintptr_t * const panoramaLib)
{
    while (true) {
        // Указатель на объект
        // Почему он 4 Байта (а не 8, как в х64 системах)? А потому что ксочка 32-х битная
        DWORD glow;

        // Количество подсвечиваемых объектов
        DWORD glowEntities;

        // Инициализируем указатель glow (читаем из памяти)
        ReadProcessMemory(
            data->handle,                                                 // Из какого хэндла читать
            (LPVOID) (*panoramaLib + Pointers::p_glowObjectManager),      // Откуда читать
            &glow,                                                        // Куда читать
            sizeof(DWORD),                                                // Сколько читать
            nullptr                           // не знаю, что это, но оно не используется в данном контексте
        );
        // Аналогично
        ReadProcessMemory(
            data->handle,
            (LPVOID) (*panoramaLib + Pointers::p_glowObjectManagerSize),
            &glowEntities,
            sizeof(DWORD),
            nullptr
        );

        // Указатель на локального игрока
        DWORD localPlayer;
        ReadProcessMemory(
            data->handle,
            (LPVOID) (*panoramaLib + Pointers::p_localPlayer),
            &localPlayer,
            sizeof(DWORD),
            nullptr
        );

        // Указатель на команду локального игрока
        DWORD localTeam;
        ReadProcessMemory(data->handle, (LPVOID) (localPlayer + Pointers::m_iTeamNum), &localTeam, sizeof(DWORD), 0);

        for (int i = 0; i < glowEntities; i++) {
            // Указатель на вход в glow
            DWORD entry;




            // 56 - размер структуры GlowObject_t
            // Гуглится
            DWORD glowPtr = glow + i * 56;

            ReadProcessMemory(data->handle, (LPVOID) glowPtr, &entry, sizeof(DWORD), 0);

            // Буффер (не разбирался, что в нем)
            // Магические числа, используемые далее, не должны меняться при обновлении КС
            DWORD buffer;
            ReadProcessMemory(data->handle, (LPVOID) (entry + 0x8), &buffer, sizeof(DWORD), 0);
            ReadProcessMemory(data->handle, (LPVOID) (buffer + 0x8), &buffer, sizeof(DWORD), 0);
            ReadProcessMemory(data->handle, (LPVOID) (buffer + 0x1), &buffer, sizeof(DWORD), 0);

            // Тип подсвечиваемого элемента
            DWORD type;
            ReadProcessMemory(data->handle, (LPVOID) (buffer + 0x14), &type, sizeof(DWORD), 0);

            // Получаем команду подсвечиваемого элемента
            DWORD team;
            ReadProcessMemory(data->handle, (LPVOID) (entry + Pointers::m_iTeamNum), &team, sizeof(DWORD), 0);

            // Ожидание: подсвечивает игроков
            // Реальность: подсвечивает игроков и различный статичный мусор
            // Найдено перебором
            if (type == 38) {
                // Сюда положим цвет
                FLOAT floats[4];
                // А сюда данные о подсветке
                BOOL bools[3] = {true, false, false};

                if (team != localTeam) {
                    FLOAT tmp1[4] = {1, 0, 0, 0.6};
                    std::memcpy(floats, tmp1, sizeof(FLOAT) * 4);
                } else {
                    FLOAT tmp1[4] = {0, 1, 0, 0.5};
                    std::memcpy(floats, tmp1, sizeof(FLOAT) * 4);
                }

                // Теперь пишем то, что получилось в память КС ГО
                // Писать необходимо постоянно. Каждая итерация подсвечивает игроков на 1 кадр
                WriteProcessMemory(data->handle, (LPVOID) (glowPtr + 0x4), floats, sizeof(FLOAT) * 4, 0);
                WriteProcessMemory(data->handle, (LPVOID) (glowPtr + 0x24), bools, sizeof(BOOL) * 3, 0);
            }

        }
    }
}

#pragma clang diagnostic pop

int main(int argc, char *argv[])
{
    // Находим процесс ксго
    ProcessData data = getProcess("csgo.exe");
    if (!data.check()) {
        std::cout << "CSGO not found!" << std::endl;
        return mainExit();
    }
    std::cout << "CSGO has been found!!" << std::endl;

    // Находим указатель на модуль панорамы
    uintptr_t panoramaLib = getModule("client_panorama.dll", data);
    if (panoramaLib == 0) {
        std::cout << "panorama not found!" << std::endl;
        return mainExit();
    }
    std::cout << "panorama has been found !! 1 esketit" << std::endl;

    // Выкидываем основную функцию в отдельный поток
    // (Этот потом будет хорошенько насиловать процессор из-за бесконечного цикла. Я предупредил)
    std::thread thread(mainThread, &data, &panoramaLib);
    thread.join();

    return mainExit();
}
