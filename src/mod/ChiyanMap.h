#pragma once
#include "ll/api/mod/NativeMod.h"

namespace chiyan_map {
    class ChiyanMap {
    public:
        static ChiyanMap& getInstance();
        ChiyanMap() : mSelf(*ll::mod::NativeMod::current()) {}
        [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
        bool load();
        bool enable();
        bool disable();
    private:
        ll::mod::NativeMod& mSelf;
    };
}
