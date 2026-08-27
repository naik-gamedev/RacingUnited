#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "../../ThirdParty/stb/stb_vorbis.c"

namespace
{
void writeU16(std::ostream& stream, std::uint16_t value)
{
    const char bytes[] = {
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8U) & 0xffU)
    };
    stream.write(bytes, sizeof(bytes));
}

void writeU32(std::ostream& stream, std::uint32_t value)
{
    const char bytes[] = {
        static_cast<char>(value & 0xffU),
        static_cast<char>((value >> 8U) & 0xffU),
        static_cast<char>((value >> 16U) & 0xffU),
        static_cast<char>((value >> 24U) & 0xffU)
    };
    stream.write(bytes, sizeof(bytes));
}
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: OggToWav <input.ogg> <output.wav>\n";
        return 2;
    }

    const std::filesystem::path inputPath(argv[1]);
    const std::filesystem::path outputPath(argv[2]);
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "Could not open " << inputPath << '\n';
        return 1;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff byteLength = input.tellg();
    input.seekg(0, std::ios::beg);
    if (byteLength <= 0 || byteLength > (std::numeric_limits<int>::max)())
    {
        std::cerr << "Invalid input length\n";
        return 1;
    }
    std::vector<unsigned char> compressed(static_cast<std::size_t>(byteLength));
    input.read(reinterpret_cast<char*>(compressed.data()), byteLength);

    int channels = 0;
    int sampleRate = 0;
    short* decoded = nullptr;
    const int frames = stb_vorbis_decode_memory(
        compressed.data(), static_cast<int>(compressed.size()),
        &channels, &sampleRate, &decoded);
    if (frames <= 0 || !decoded || channels <= 0 || sampleRate <= 0)
    {
        std::free(decoded);
        std::cerr << "Vorbis decode failed\n";
        return 1;
    }

    const std::uint64_t dataBytes64 = static_cast<std::uint64_t>(frames)
        * static_cast<std::uint64_t>(channels) * sizeof(short);
    if (dataBytes64 > (std::numeric_limits<std::uint32_t>::max)() - 36U)
    {
        std::free(decoded);
        std::cerr << "Decoded WAV is too large\n";
        return 1;
    }
    const auto dataBytes = static_cast<std::uint32_t>(dataBytes64);
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        std::free(decoded);
        std::cerr << "Could not create " << outputPath << '\n';
        return 1;
    }

    output.write("RIFF", 4);
    writeU32(output, 36U + dataBytes);
    output.write("WAVEfmt ", 8);
    writeU32(output, 16U);
    writeU16(output, 1U);
    writeU16(output, static_cast<std::uint16_t>(channels));
    writeU32(output, static_cast<std::uint32_t>(sampleRate));
    writeU32(output, static_cast<std::uint32_t>(sampleRate * channels * sizeof(short)));
    writeU16(output, static_cast<std::uint16_t>(channels * sizeof(short)));
    writeU16(output, 16U);
    output.write("data", 4);
    writeU32(output, dataBytes);
    output.write(reinterpret_cast<const char*>(decoded), dataBytes);
    std::free(decoded);
    if (!output)
    {
        std::cerr << "Could not finish " << outputPath << '\n';
        return 1;
    }

    std::cout << "Decoded " << frames << " frames, " << channels
        << " channels at " << sampleRate << " Hz\n";
    return 0;
}
