#pragma once
#include <cstdint>

namespace vilab {
constexpr wchar_t kTargetClass[] = L"VirtualInputLab.Target.V2";
constexpr wchar_t kPipePrefix[] = L"\\\\.\\pipe\\VirtualInputLab.V2.";
constexpr std::uint32_t kMagic = 0x56494C32; // "VIL2"
constexpr std::int32_t kCoordinateScale = 10000;

enum class CommandType : std::uint32_t {
    Move = 1,
    LeftDown = 2,
    LeftUp = 3,
    Click = 4,
    Reset = 5,
};

enum CommandFlags : std::uint32_t {
    None = 0,
    NormalizedCoordinates = 1,
};

#pragma pack(push, 1)
struct Command {
    std::uint32_t magic{kMagic};
    CommandType type{CommandType::Move};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t holdMs{30};
    std::uint64_t sequence{};
    std::uint32_t flags{None};
};
#pragma pack(pop)
}
