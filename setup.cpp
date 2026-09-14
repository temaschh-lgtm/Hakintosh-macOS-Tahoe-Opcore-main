#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <array>

namespace fs = std::filesystem;

// Запуск команды с перехватом вывода
std::string exec_capture(const std::string& cmd) {
    std::array<char, 256> buf;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe)) {
        result += buf.data();
    }
    pclose(pipe);
    return result;
}

// Запуск команды с прямым выводом в консоль (видно прогресс)
int exec_live(const std::string& cmd) {
    return std::system(cmd.c_str());
}

// Проверка, что мы на macOS
bool check_platform() {
    std::string uname = exec_capture("uname -s");
    return uname.find("Darwin") != std::string::npos;
}

// Поиск установочного образа macOS
std::vector<std::string> find_installers() {
    std::vector<std::string> found;
    std::vector<std::string> apps = {
        "/Applications/Install macOS Ventura.app",
        "/Applications/Install macOS Monterey.app",
        "/Applications/Install macOS Big Sur.app",
        "/Applications/Install macOS Sonoma.app",
        "/Applications/Install macOS Sequoia.app"
    };
    for (const auto& path : apps) {
        if (fs::exists(path)) {
            found.push_back(path);
        }
    }
    return found;
}

// Список доступных дисков
void list_disks() {
    std::cout << "\n--- Доступные диски ---\n";
    std::string output = exec_capture("diskutil list external");
    std::cout << output << "\n";
}

int main() {
    std::cout << "=== Установка macOS — загрузочная флешка ===\n\n";

    // 1. Проверка платформы
    if (!check_platform()) {
        std::cerr << "[ОШИБКА] Эта программа работает только на macOS.\n";
        std::cerr << "Запустите её на Mac.\n";
        return 1;
    }
    std::cout << "[OK] Платформа macOS подтверждена.\n\n";

    // 2. Проверка прав root
    if (std::system("test $(id -u) -eq 0") != 0) {
        std::cerr << "[ОШИБКА] Нужны права администратора.\n";
        std::cerr << "Запустите через: sudo ./установка_macos\n";
        return 1;
    }
    std::cout << "[OK] Права root подтверждены.\n\n";

    // 3. Поиск установочного образа
    auto installers = find_installers();
    if (installers.empty()) {
        std::cerr << "[ОШИБКА] Установочный образ macOS не найден в /Applications.\n";
        std::cerr << "Скачайте его из App Store: https://support.apple.com/ru-ru/HT201372\n";
        return 1;
    }

    std::cout << "Найденные установочные образы:\n";
    for (size_t i = 0; i < installers.size(); ++i) {
        std::cout << "  " << i + 1 << ". " << installers[i] << "\n";
    }

    size_t choice = 0;
    std::cout << "\nВыберите номер образа: ";
    std::cin >> choice;
    if (choice < 1 || choice > installers.size()) {
        std::cerr << "[ОШИБКА] Неверный выбор.\n";
        return 1;
    }
    std::string installer = installers[choice - 1];
    std::cout << "\nВыбран: " << installer << "\n\n";

    // 4. Выбор USB-накопителя
    list_disks();
    std::string disk_id;
    std::cout << "Введите идентификатор диска (например disk4): ";
    std::cin >> disk_id;

    // Проверка, что диск существует
    std::string check = exec_capture("diskutil info " + disk_id + " 2>&1");
    if (check.find("Could not find") != std::string::npos || check.empty()) {
        std::cerr << "[ОШИБКА] Диск " << disk_id << " не найден.\n";
        return 1;
    }
    std::cout << "[OK] Диск " << disk_id << " найден.\n\n";

    // 5. Подтверждение
    std::cout << "========================================\n";
    std::cout << "  ВНИМАНИЕ! Все данные на " << disk_id << "\n";
    std::cout << "  будут полностью удалены!\n";
    std::cout << "========================================\n";
    std::cout << "\nПродолжить? (yes/no): ";
    std::string confirm;
    std::cin >> confirm;
    if (confirm != "yes") {
        std::cout << "Отменено пользователем.\n";
        return 0;
    }

    // 6. Форматирование флешки
    std::cout << "\n[1/3] Форматирование " << disk_id << "...\n";
    std::string erase_cmd = "diskutil eraseDisk APFS \"InstallMedia\" GPT " + disk_id;
    if (exec_live(erase_cmd) != 0) {
        std::cerr << "[ОШИБКА] Не удалось отформатировать диск.\n";
        return 1;
    }
    std::cout << "[OK] Форматирование завершено.\n\n";

    // 7. Запуск createinstallmedia
    std::cout << "[2/3] Создание загрузочного установщика...\n";
    std::cout << "Это может занять 15-30 минут. Не извлекайте флешку!\n\n";

    std::string cim_cmd = "\"" + installer +
        "/Contents/Resources/createinstallmedia\" --volume /Volumes/InstallMedia";

    if (exec_live(cim_cmd) != 0) {
        std::cerr << "\n[ОШИБКА] createinstallmedia завершился с ошибкой.\n";
        std::cerr << "Проверьте, что образ целый и флешка не занята.\n";
        return 1;
    }

    // 8. Готово
    std::cout << "\n[3/3] Готово!\n";
    std::cout << "========================================\n";
    std::cout << "  Загрузочная флешка macOS создана.\n";
    std::cout << "  Для установки:\n";
    std::cout << "  1. Перезагрузите Mac с зажатой клавишей Option\n";
    std::cout << "  2. Выберите Install macOS\n";
    std::cout << "  3. Следуйте инструкциям установщика\n";
    std::cout << "========================================\n";

    return 0;
}
