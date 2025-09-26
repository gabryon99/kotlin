//
// Created by Gabriele.Pappalardo on 01/08/2025.
//

#ifndef IRLAZYARRAYREADER_HPP
#define IRLAZYARRAYREADER_HPP

#include <optional>
#include <functional>
#include <memory>

#include "Utility.hpp"
#include "google/protobuf/io/coded_stream.h"

namespace kotlin::ir {

class IrLazyArrayReader {
public:
    void loadFromFile(const std::string& filePath) {
        const auto fileContent = readFileContent(filePath);
        _fileBuffer = std::move(fileContent);
        parse();
    }

    // TODO: we're always assuming to access entry 3 of file table
    template <typename MessageType>
    [[nodiscard]] std::unique_ptr<MessageType> getElementAt(const uint32_t id) const {

        const auto& base = _irOuterArrays[MAGIC_INDEX];
        const auto& [startIndex, size] = _irInnerArrays[MAGIC_INDEX][id]; // FIXME!!! (constant index)

        const uint8_t* basePointer = _fileBuffer.data() + base.startIndex + startIndex;

        google::protobuf::io::CodedInputStream input(basePointer, size);
        input.SetRecursionLimit(65535);

        auto protoMessage = std::make_unique<MessageType>();
        if (!protoMessage->ParseFromCodedStream(&input)) {
            return std::unique_ptr<MessageType>(nullptr);
        }

        return protoMessage;
    }

private:
    struct IrArrayIndex {
        uint32_t startIndex{0};
        uint32_t size{0};
    };

    static std::vector<IrArrayIndex> extractIrArrayIndices(const uint8_t* buffer, const uint32_t irArraysCount) {
        constexpr uint32_t FILE_HEADER_SIZE = sizeof(uint32_t);

        std::vector<IrArrayIndex> irArrays{};
        irArrays.reserve(irArraysCount);

        uint32_t byteOffsetToNextArray = FILE_HEADER_SIZE + (irArraysCount * sizeof(uint32_t)); // after the 4-bytes of how many arrays

        for (size_t i = 0; i < irArraysCount; i++) {
            const uint8_t* basePointer = buffer + FILE_HEADER_SIZE + (i * sizeof(uint32_t));

            const uint32_t byteArraySize = readBigEndian32(basePointer);

            IrArrayIndex arrayIndex{};
            arrayIndex.size = byteArraySize;
            arrayIndex.startIndex = byteOffsetToNextArray;

            irArrays.push_back(arrayIndex);
            byteOffsetToNextArray += byteArraySize;
        }

        return irArrays;
    }

    void parse() {
        if (_fileBuffer.size() < 4) {
            throw std::runtime_error("File is too small to contain a declaration count.");
        }

        const uint32_t irOuterArrayCount = readBigEndian32(_fileBuffer.data());
        const std::vector<IrArrayIndex> irOuterArrays = extractIrArrayIndices(_fileBuffer.data(), irOuterArrayCount);

        _irInnerArrays.reserve(irOuterArrayCount);

        for (const auto& [startIndex, size] : irOuterArrays) {
            const uint8_t* buffer = _fileBuffer.data() + startIndex;
            const uint32_t irInnerArrayCounter = readBigEndian32(buffer);
            const std::vector<IrArrayIndex> irInnerArray = extractIrArrayIndices(buffer, irInnerArrayCounter);
            _irInnerArrays.push_back(irInnerArray);
        }

        _irOuterArrays = irOuterArrays;

        // ARRAY OF ARRAY!
        // std::vector<std::vector<IrArray>>
    }

    std::vector<uint8_t> _fileBuffer;
    std::vector<IrArrayIndex> _irOuterArrays;
    std::vector<std::vector<IrArrayIndex>> _irInnerArrays;
};
} // namespace kotlin::ir


#endif //IRLAZYARRAYREADER_HPP
