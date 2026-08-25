#pragma once
#include <cstdint>

namespace vilab {
constexpr wchar_t kTargetClass[] = L"VirtualInputLab.Target";
constexpr wchar_t kPipePrefix[] = L"\\\\.\\pipe\\VirtualInputLab.";

enum class CommandType : std::uint32_t {
    Move = 1,
    LeftDown = 2,
    LeftUp = 3,
    Click = 4,
    Reset = 5,
};

#pragma pack(push, 1)
struct Command {
    std::uint32_t magic{0x56494C31}; // "VIL1"
    CommandType type{CommandType::Move};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t holdMs{30};
    std::uint64_t sequence{};
};
#pragma pack(pop)
}

