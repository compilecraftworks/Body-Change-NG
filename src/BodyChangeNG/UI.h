#pragma once

#include <string>

namespace bcn::ui
{
    [[nodiscard]] const char* Localize(const char* a_korean, const char* a_english, const char* a_chineseSimplified);
    void Initialize();
    void Draw();
    void OnOpened();
    void OnClosed();
    void OnLoadStart();
    void Notify(std::string a_message);
}
