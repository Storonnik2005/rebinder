#define _CRT_SECURE_NO_WARNINGS // Отключение предупреждений о небезопасных функциях

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <ctime>
#include <direct.h> // Для mkdir
#include <io.h>     // Для доступа к файлам
#include <algorithm>
#include <locale> // Для локализации
#include <windows.h> // Для настройки кодовой страницы консоли

// Интерфейс для объектов резервного копирования
class BackupObject {
public:
    // Виртуальный деструктор по умолчанию
    virtual ~BackupObject() = default;
    // Чисто виртуальный метод для получения пути к объекту
    virtual std::string getPath() const = 0;
    // Чисто виртуальный метод для получения имени объекта
    virtual std::string getName() const = 0;
    // Чисто виртуальный метод для получения размера объекта
    virtual size_t getSize() const = 0;
    // Чисто виртуальный метод для проверки существования объекта
    virtual bool exists() const = 0;
};

// Функция для проверки существования файла
bool fileExists(const std::string& path) {
    return _access(path.c_str(), 0) == 0;
}

// Функция для получения размера файла
size_t getFileSize(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return 0;
    return static_cast<size_t>(file.tellg());
}

// Функция для создания директории
bool createDirectory(const std::string& path) {
    return _mkdir(path.c_str()) == 0;
}

// Функция для получения имени файла из пути
std::string getFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

// Функция для копирования файла
bool copyFile(const std::string& sourcePath, const std::string& destPath) {
    std::ifstream src(sourcePath, std::ios::binary);
    if (!src.is_open()) return false;

    std::ofstream dst(destPath, std::ios::binary);
    if (!dst.is_open()) return false;

    dst << src.rdbuf();

    return true;
}

// Реализация объекта резервного копирования для файлов
class FileBackupObject : public BackupObject {
private:
    // Путь к файлу
    std::string filePath;

public:
    // Конструктор, принимающий путь к файлу
    explicit FileBackupObject(const std::string& path) : filePath(path) {}

    // Реализация метода получения пути
    std::string getPath() const override {
        return filePath;
    }

    // Реализация метода получения имени файла
    std::string getName() const override {
        return getFileName(filePath);
    }

    // Реализация метода получения размера файла
    size_t getSize() const override {
        if (!exists()) return 0;
        return getFileSize(filePath);
    }

    // Реализация метода проверки существования файла
    bool exists() const override {
        return fileExists(filePath);
    }
};

// Интерфейс для стратегии хранения резервных копий
class StorageAlgorithm {
public:
    // Виртуальный деструктор по умолчанию
    virtual ~StorageAlgorithm() = default;
    // Чисто виртуальный метод для сохранения объектов
    virtual void store(const std::vector<std::shared_ptr<BackupObject>>& objects,
        const std::string& destinationPath,
        const std::string& pointName) = 0;
};

// Реализация стратегии раздельного хранения
class SplitStorageAlgorithm : public StorageAlgorithm {
public:
    // Реализация метода сохранения объектов с раздельным хранением
    void store(const std::vector<std::shared_ptr<BackupObject>>& objects,
        const std::string& destinationPath,
        const std::string& pointName) override {
        // Формируем путь к точке восстановления
        std::string pointPath = destinationPath + "\\" + pointName;

        // Создаем директорию для точки восстановления, если она не существует
        if (!fileExists(pointPath)) {
            createDirectory(pointPath);
        }

        // Обрабатываем каждый объект
        for (const auto& object : objects) {
            // Проверяем существование объекта
            if (!object->exists()) {
                throw std::runtime_error("Объект не существует: " + object->getPath());
            }

            // Создаем отдельную директорию для каждого объекта
            std::string objectDir = pointPath + "\\" + object->getName();
            createDirectory(objectDir);

            // Получаем пути источника и назначения
            std::string sourcePath = object->getPath();
            std::string destPath = objectDir + "\\" + object->getName();

            // Копируем файл
            if (copyFile(sourcePath, destPath)) {
                std::cout << "Файл скопирован: " << sourcePath << " -> " << destPath << std::endl;
            }
            else {
                throw std::runtime_error("Ошибка копирования файла: " + sourcePath);
            }
        }
    }
};

// Реализация стратегии общего хранилища
class SingleStorageAlgorithm : public StorageAlgorithm {
public:
    // Реализация метода сохранения объектов с общим хранилищем
    void store(const std::vector<std::shared_ptr<BackupObject>>& objects,
        const std::string& destinationPath,
        const std::string& pointName) override {
        // Формируем путь к точке восстановления
        std::string pointPath = destinationPath + "\\" + pointName;

        // Создаем директорию для точки восстановления, если она не существует
        if (!fileExists(pointPath)) {
            createDirectory(pointPath);
        }

        // Создаем одну общую директорию для всех файлов
        std::string commonDir = pointPath + "\\common";
        createDirectory(commonDir);

        // Обрабатываем каждый объект
        for (const auto& object : objects) {
            // Проверяем существование объекта
            if (!object->exists()) {
                throw std::runtime_error("Объект не существует: " + object->getPath());
            }

            // Получаем пути источника и назначения
            std::string sourcePath = object->getPath();
            std::string destPath = commonDir + "\\" + object->getName();

            // Копируем файл
            if (copyFile(sourcePath, destPath)) {
                std::cout << "Файл скопирован: " << sourcePath << " -> " << destPath << std::endl;
            }
            else {
                throw std::runtime_error("Ошибка копирования файла: " + sourcePath);
            }
        }
    }
};

// Класс точки восстановления
class RestorePoint {
private:
    // Имя точки восстановления
    std::string name;
    // Время создания точки восстановления
    time_t creationTime;
    // Список объектов в точке восстановления
    std::vector<std::shared_ptr<BackupObject>> objects;

public:
    // Конструктор, принимающий имя точки и список объектов
    RestorePoint(const std::string& pointName,
        const std::vector<std::shared_ptr<BackupObject>>& backupObjects)
        : name(pointName),
        creationTime(time(nullptr)),
        objects(backupObjects) {
    }

    // Метод для получения имени точки восстановления
    std::string getName() const {
        return name;
    }

    // Метод для получения времени создания в виде строки (исправленный для безопасности)
    std::string getCreationTimeString() const {
        char buffer[80];
        struct tm timeinfo;
        localtime_s(&timeinfo, &creationTime);
        strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
        return std::string(buffer);
    }

    // Метод для получения списка объектов
    const std::vector<std::shared_ptr<BackupObject>>& getObjects() const {
        return objects;
    }
};

// Класс репозитория для хранения резервных копий
class BackupRepository {
private:
    // Путь к репозиторию
    std::string repositoryPath;

public:
    // Конструктор, принимающий путь к репозиторию
    explicit BackupRepository(const std::string& path) : repositoryPath(path) {
        // Создаем директорию репозитория, если она не существует
        if (!fileExists(path)) {
            if (!createDirectory(path)) {
                throw std::runtime_error("Не удалось создать репозиторий: " + path);
            }
        }
    }

    // Метод для получения пути к репозиторию
    std::string getPath() const {
        return repositoryPath;
    }
};

// Класс джобы резервного копирования
class BackupJob {
private:
    // Имя джобы
    std::string name;
    // Список объектов резервного копирования
    std::vector<std::shared_ptr<BackupObject>> backupObjects;
    // Список точек восстановления
    std::vector<RestorePoint> restorePoints;
    // Алгоритм хранения
    std::shared_ptr<StorageAlgorithm> storageAlgorithm;
    // Репозиторий для хранения
    std::shared_ptr<BackupRepository> repository;

public:
    // Конструктор, принимающий имя джобы, алгоритм хранения и репозиторий
    BackupJob(const std::string& jobName,
        std::shared_ptr<StorageAlgorithm> algorithm,
        std::shared_ptr<BackupRepository> repo)
        : name(jobName),
        storageAlgorithm(algorithm),
        repository(repo) {
    }

    // Метод для добавления объекта резервного копирования
    void addBackupObject(std::shared_ptr<BackupObject> object) {
        // Проверяем существование объекта
        if (!object->exists()) {
            throw std::runtime_error("Невозможно добавить несуществующий объект: " + object->getPath());
        }

        // Проверяем, что объект еще не добавлен
        auto it = std::find_if(backupObjects.begin(), backupObjects.end(),
            [&object](const std::shared_ptr<BackupObject>& obj) {
                return obj->getPath() == object->getPath();
            });

        // Если объект не найден, добавляем его
        if (it == backupObjects.end()) {
            backupObjects.push_back(object);
            std::cout << "Объект добавлен в джобу: " << object->getPath() << std::endl;
        }
        else {
            std::cout << "Объект уже существует в джобе: " << object->getPath() << std::endl;
        }
    }

    // Метод для удаления объекта резервного копирования
    void removeBackupObject(const std::string& objectPath) {
        // Ищем объект по пути
        auto it = std::find_if(backupObjects.begin(), backupObjects.end(),
            [&objectPath](const std::shared_ptr<BackupObject>& obj) {
                return obj->getPath() == objectPath;
            });

        // Если объект найден, удаляем его
        if (it != backupObjects.end()) {
            backupObjects.erase(it);
            std::cout << "Объект удален из джобы: " << objectPath << std::endl;
        }
        else {
            std::cout << "Объект не найден в джобе: " << objectPath << std::endl;
        }
    }

    // Метод для создания точки восстановления
    void createRestorePoint(const std::string& pointName) {
        // Проверяем, что есть объекты для резервного копирования
        if (backupObjects.empty()) {
            throw std::runtime_error("Невозможно создать точку восстановления без объектов резервного копирования");
        }

        // Проверяем существование всех объектов перед созданием точки
        for (const auto& object : backupObjects) {
            if (!object->exists()) {
                throw std::runtime_error("Объект не существует: " + object->getPath());
            }
        }

        // Создаем точку восстановления
        RestorePoint point(pointName, backupObjects);
        restorePoints.push_back(point);

        // Выполняем резервное копирование с использованием выбранного алгоритма
        storageAlgorithm->store(backupObjects, repository->getPath(), pointName);

        std::cout << "Создана точка восстановления: " << pointName
            << " (" << point.getCreationTimeString() << ")" << std::endl;
    }

    // Метод для изменения алгоритма хранения
    void setStorageAlgorithm(std::shared_ptr<StorageAlgorithm> algorithm) {
        storageAlgorithm = algorithm;
    }

    // Метод для получения списка точек восстановления
    const std::vector<RestorePoint>& getRestorePoints() const {
        return restorePoints;
    }

    // Метод для получения списка объектов резервного копирования
    const std::vector<std::shared_ptr<BackupObject>>& getBackupObjects() const {
        return backupObjects;
    }

    // Метод для получения имени джобы
    std::string getName() const {
        return name;
    }

    // Метод для вывода информации о джобе
    void printInfo() const {
        std::cout << "Джоба: " << name << std::endl;
        std::cout << "Количество объектов: " << backupObjects.size() << std::endl;
        std::cout << "Объекты:" << std::endl;
        // Выводим информацию о каждом объекте
        for (const auto& obj : backupObjects) {
            std::cout << "  - " << obj->getPath() << " (" << obj->getSize() << " байт)" << std::endl;
        }

        std::cout << "Количество точек восстановления: " << restorePoints.size() << std::endl;
        std::cout << "Точки восстановления:" << std::endl;
        // Выводим информацию о каждой точке восстановления
        for (const auto& point : restorePoints) {
            std::cout << "  - " << point.getName() << " (" << point.getCreationTimeString() << ")" << std::endl;
        }
    }
};

// Фабрика для создания объектов резервного копирования
class BackupObjectFactory {
public:
    // Статический метод для создания объекта резервного копирования файла
    static std::shared_ptr<BackupObject> createFileBackupObject(const std::string& path) {
        return std::make_shared<FileBackupObject>(path);
    }
};

// Демонстрация работы системы
int main() {
    // Настройка локализации для корректного отображения русских символов
    setlocale(LC_ALL, "Russian");
    // Устанавливаем кодовую страницу консоли на UTF-8
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    try {
        // Создаем репозиторий для хранения резервных копий
        auto repository = std::make_shared<BackupRepository>(".\\backup_repository");

        // Создаем алгоритмы хранения
        auto splitStorage = std::make_shared<SplitStorageAlgorithm>();
        auto singleStorage = std::make_shared<SingleStorageAlgorithm>();

        // Создаем джобу резервного копирования с раздельным хранением
        auto backupJob = std::make_shared<BackupJob>("DocumentsBackup", splitStorage, repository);

        // Создаем тестовую директорию
        std::string testDir = ".\\test_files";
        if (!fileExists(testDir)) {
            createDirectory(testDir);
        }

        // Задаем пути к тестовым файлам
        std::string file1Path = testDir + "\\file1.txt";
        std::string file2Path = testDir + "\\file2.txt";

        // Создаем тестовые файлы с содержимым
        {
            // Создаем и записываем в первый файл
            std::ofstream file1(file1Path);
            file1 << "Это содержимое первого тестового файла для резервного копирования.";
            file1.close();

            // Создаем и записываем во второй файл
            std::ofstream file2(file2Path);
            file2 << "Это содержимое второго тестового файла для резервного копирования.";
            file2.close();
        }

        // Добавляем объекты в джобу
        auto fileObject1 = BackupObjectFactory::createFileBackupObject(file1Path);
        auto fileObject2 = BackupObjectFactory::createFileBackupObject(file2Path);

        backupJob->addBackupObject(fileObject1);
        backupJob->addBackupObject(fileObject2);

        // Выводим информацию о джобе
        backupJob->printInfo();

        // Создаем точку восстановления с раздельным хранением
        backupJob->createRestorePoint("SplitStoragePoint");

        // Меняем алгоритм хранения на общее хранилище
        backupJob->setStorageAlgorithm(singleStorage);

        // Создаем точку восстановления с общим хранилищем
        backupJob->createRestorePoint("SingleStoragePoint");

        // Удаляем один объект и создаем новую точку восстановления
        backupJob->removeBackupObject(file2Path);
        backupJob->createRestorePoint("ReducedPoint");

        // Выводим обновленную информацию о джобе
        backupJob->printInfo();

        // Демонстрация обработки исключений
        try {
            // Пытаемся добавить несуществующий файл
            auto nonExistentFile = BackupObjectFactory::createFileBackupObject(".\\non_existent_file.txt");
            backupJob->addBackupObject(nonExistentFile);
        }
        catch (const std::exception& e) {
            // Перехватываем и выводим исключение
            std::cout << "Перехвачено исключение: " << e.what() << std::endl;
        }

        // Выводим сообщение об успешном завершении
        std::cout << "Система резервного копирования успешно продемонстрирована!" << std::endl;

    }
    catch (const std::exception& e) {
        // Перехватываем и выводим любые необработанные исключения
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }

    // Возвращаем код успешного завершения
    return 0;
}
