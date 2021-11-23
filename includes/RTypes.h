#pragma once
#include <memory>

namespace RA {
    struct ShiftState {
        bool ctrl;
        bool shift;
        bool mouse_btn[5];
    };

    template<typename T>
    using UPtr = std::unique_ptr<T>;

    template<typename T, typename... Args>
    UPtr<T> UPtrMake(Args&&... args) { return std::make_unique<T>(args...); }

    template<typename T>
    using SPtr = std::shared_ptr<T>;

    template<typename T, typename... Args>
    SPtr<T> SPtrMake(Args&&... args) { return std::make_shared<T>(args...); }

    template<typename T, typename... Args>
    SPtr<T> SPtrCast(Args&&... args) { return std::static_pointer_cast<T>(args...); }

    template<typename T, typename... Args>
    SPtr<T> SPtrDynCast(Args&&... args) { return std::dynamic_pointer_cast<T>(args...); }

    template<typename T>
    using WPtr = std::weak_ptr<T>;
}