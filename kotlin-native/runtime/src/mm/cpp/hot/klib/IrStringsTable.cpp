//
// Created by Gabriele.Pappalardo on 09/07/2025.
//

#include "IrStringsTable.hpp"
#include "Utility.hpp"

static constexpr uint32_t FILE_HEADER_SIZE = sizeof(uint32_t);

std::vector<kotlin::ir::IrStringsTable::ArrayIndex> kotlin::ir::IrStringsTable::extractArrayIndices(const uint32_t arraysCount) const {
    std::vector<ArrayIndex> irArrays{};
    irArrays.reserve(arraysCount);

    uint32_t byteOffsetToNextArray = FILE_HEADER_SIZE + (arraysCount * sizeof(uint32_t)); // after the 4-bytes of how many arrays

    for (size_t i = 0; i < arraysCount; i++) {
        const uint8_t* basePointer = _fileBuffer.data() + FILE_HEADER_SIZE + (i * sizeof(uint32_t));

        if (basePointer >= _fileBuffer.data() + _fileBuffer.size()) {
            throw std::runtime_error("cannot ready byteArraySize since it'd overflow...");
        }

        const uint32_t byteArraySize = readBigEndian32(basePointer);

        ArrayIndex arrayIndex{};
        arrayIndex.size = byteArraySize;
        arrayIndex.startIndex = byteOffsetToNextArray;

        irArrays.push_back(arrayIndex);
        byteOffsetToNextArray += byteArraySize;
    }

    // correctness check:
    if (irArrays[arraysCount - 1].startIndex + irArrays[arraysCount - 1].size != _fileBuffer.size()) {
        throw std::runtime_error("correctness check for file failed.");
    }

    return irArrays;
}

/**
 * @brief Checks if a byte is a valid UTF-8 continuation byte (i.e., starts with "10").
 * * @param byte The byte to check.
 * @return True if it is a valid continuation byte, false otherwise.
 */
inline bool isValidContinuation(uint8_t byte) {
    return (byte & 0xC0) == 0x80; // Checks for the pattern 10xxxxxx
}

/**
 * @brief Decodes a "wobbly" UTF-8 byte stream from a portion of a byte vector into a std::string.
 * * This function interprets a sequence of bytes as a modified UTF-8 stream. It handles
 * 1, 2, 3, and 4-byte sequences. Unlike the original Kotlin code which produces UTF-16 characters
 * (including surrogate pairs), this C++ version produces a standard, valid UTF-8 encoded std::string.
 * Malformed or incomplete sequences are replaced by the Unicode replacement character (U+FFFD).
 * * @param data The source vector of bytes.
 * @param size The number of bytes to process from the `offset`.
 * @param offset The starting index in the `data` vector.
 * @return A std::string containing the decoded UTF-8 text.
 */
std::string decodeWobblyTF8(const std::vector<uint8_t>& data, const uint32_t size, const uint32_t offset) {
    if (size == 0 || offset >= data.size()) {
        return "";
    }

    std::string result;
    result.reserve(size); // Pre-allocate memory to improve performance.

    // Calculate the end index, ensuring it doesn't exceed the vector's bounds.
    const uint32_t end = std::min(static_cast<uint32_t>(data.size()), offset + size);
    uint32_t index = offset;

    while (index < end) {
        const uint8_t byte1 = data[index++];

        // 1-byte sequence: 0xxxxxxx (U+0000..U+007F)
        if ((byte1 & 0x80) == 0) {
            result.push_back(static_cast<char>(byte1));
            continue;
        }

        // 2-byte sequence: 110xxxxx 10xxxxxx (U+0080..U+07FF)
        else if ((byte1 & 0xE0) == 0xC0) { // Check for 110xxxxx pattern
            if (index < end && isValidContinuation(data[index])) {
                result.push_back(static_cast<char>(byte1));
                result.push_back(static_cast<char>(data[index]));
                index++;
                continue;
            }
        }

        // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx (U+0800..U+FFFF)
        else if ((byte1 & 0xF0) == 0xE0) { // Check for 1110xxxx pattern
            if (index + 1 < end && isValidContinuation(data[index]) && isValidContinuation(data[index + 1])) {
                result.push_back(static_cast<char>(byte1));
                result.push_back(static_cast<char>(data[index]));
                result.push_back(static_cast<char>(data[index + 1]));
                index += 2;
                continue;
            }
        }

        // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (U+10000..U+10FFFF)
        else if ((byte1 & 0xF8) == 0xF0) { // Check for 11110xxx pattern
            if (index + 2 < end && isValidContinuation(data[index]) && isValidContinuation(data[index + 1]) && isValidContinuation(data[index + 2])) {
                result.push_back(static_cast<char>(byte1));
                result.push_back(static_cast<char>(data[index]));
                result.push_back(static_cast<char>(data[index + 1]));
                result.push_back(static_cast<char>(data[index + 2]));
                index += 3;
                continue;
            }
        }

        // If we fall through, the byte sequence was invalid or incomplete.
        // Append the Unicode replacement character U+FFFD, which is 0xEF, 0xBF, 0xBD in UTF-8.
        result.push_back(static_cast<char>(0xEF));
        result.push_back(static_cast<char>(0xBF));
        result.push_back(static_cast<char>(0xBD));
    }

    return result;
}

void kotlin::ir::IrStringsTable::loadFromFile(const std::string &filePath) {
    const auto fileContent = readFileContent(filePath);
    _fileBuffer = fileContent;
    parse();
}

std::optional<std::reference_wrapper<const std::string>> kotlin::ir::IrStringsTable::getString(const uint32_t index) const {
    if (index > _strings.at(MAGIC_INDEX).size()) return {};
    return std::optional{std::cref(_strings.at(MAGIC_INDEX)[index])};
}

void kotlin::ir::IrStringsTable::parse() {

    const uint32_t arraysCount = readBigEndian32(_fileBuffer.data());
    const auto arrayIndices = extractArrayIndices(arraysCount);

    for (size_t i = 0; i < arraysCount; i++) {
        const auto& arrayIndex = arrayIndices[i];

        const uint8_t* basePointer = _fileBuffer.data() + arrayIndex.startIndex;
        // An array represents the serialized bytes of IrStringWriter
        // 1. How many strings are contained in the file
        // 2. Sequence of string sizes
        // 3. Sequence of string data
        constexpr uint32_t STRING_HEADER_SIZE = sizeof(uint32_t);

        std::vector<uint32_t> stringSizes{};

        const uint32_t numberOfStrings = readBigEndian32(basePointer);
        stringSizes.reserve(numberOfStrings);

        _strings[i] = std::vector<std::string>{};
        _strings[i].reserve(numberOfStrings);

        for (size_t j = 0; j < numberOfStrings; j++) {
            const uint32_t size = readBigEndian32(basePointer + STRING_HEADER_SIZE + (j * sizeof(uint32_t)));
            stringSizes.push_back(size);
        }

        uint32_t offset = arrayIndex.startIndex + sizeof(uint32_t) + (sizeof(uint32_t) * numberOfStrings);

        for (size_t j = 0; j < numberOfStrings; j++) {
            const auto stringSize = stringSizes[j];
            auto str = decodeWobblyTF8(_fileBuffer, stringSize, offset);
            _strings[i].push_back(str);
            offset += stringSize;
        }

    }
}
