#pragma once
#include <cstdint>

namespace autosync_ipc {
constexpr wchar_t kTargetClass[] = L"VirtualInputLab.Target.V3";
constexpr wchar_t kPipePrefix[] = L"\\\\.\\pipe\\VirtualInputLab.V3.";
constexpr std::uint32_t kMagic = 0x56494C33;
constexpr std::int32_t kCoordinateScale = 10000;

enum class CommandType : std::uint32_t { Move = 1, LeftDown = 2, LeftUp = 3, Reset = 4 };
enum CommandFlags : std::uint32_t { None = 0, NormalizedCoordinates = 1 };

#pragma pack(push, 1)
struct Command {
    std::uint32_t magic{kMagic};
    CommandType type{CommandType::Move};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t step{};
    std::uint64_t sequence{};
    std::uint32_t flags{NormalizedCoordinates};
};
#pragma pack(pop)
static_assert(sizeof(Command) == 32, "VirtualInputLab V3 protocol layout changed");
}
