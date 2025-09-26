//
// Created by Gabriele.Pappalardo on 11/07/2025.
//

#ifndef IRARRAYREADER_HPP
#define IRARRAYREADER_HPP

#include <optional>
#include <functional>

#include "Utility.hpp"
#include "google/protobuf/io/coded_stream.h"

namespace kotlin::ir {

// TODO: reimplement this class as MIX-IN
template <typename MessageType>
class IrArrayReader {
public:
    void loadFromFile(const std::string& filePath) {
        const auto fileContent = readFileContent(filePath);
        _fileBuffer = fileContent;
        parse();
    }

    [[nodiscard]] OptionalConstRef<MessageType> getElementAt(uint32_t id) const { return _protos[id]; }

    [[nodiscard]] size_t getCount() const { return _protos.size(); }

private:
    struct IrArrayIndex {
        uint32_t startIndex{0};
        uint32_t size{0};
    };

    std::vector<IrArrayIndex> extractIrArrayIndices(const uint32_t irArraysCount) const {
        constexpr uint32_t FILE_HEADER_SIZE = sizeof(uint32_t);

        std::vector<IrArrayIndex> irArrays{};
        irArrays.reserve(irArraysCount);

        uint32_t byteOffsetToNextArray = FILE_HEADER_SIZE + (irArraysCount * sizeof(uint32_t)); // after the 4-bytes of how many arrays

        for (size_t i = 0; i < irArraysCount; i++) {
            const uint8_t* basePointer = _fileBuffer.data() + FILE_HEADER_SIZE + (i * sizeof(uint32_t));

            if (basePointer >= _fileBuffer.data() + _fileBuffer.size()) {
                throw std::runtime_error("cannot ready byteArraySize since it'd overflow...");
            }

            const uint32_t byteArraySize = readBigEndian32(basePointer);

            IrArrayIndex arrayIndex{};
            arrayIndex.size = byteArraySize;
            arrayIndex.startIndex = byteOffsetToNextArray;

            irArrays.push_back(arrayIndex);
            byteOffsetToNextArray += byteArraySize;
        }

        // correctness check:
        if (irArrays[irArraysCount - 1].startIndex + irArrays[irArraysCount - 1].size != _fileBuffer.size()) {
            throw std::runtime_error("correctness check for file failed.");
        }

        return irArrays;
    }

    void parse() {
        if (_fileBuffer.size() < 4) {
            throw std::runtime_error("File is too small to contain a declaration count.");
        }

        const uint32_t irArraysCount = readBigEndian32(_fileBuffer.data());
        _protos.reserve(irArraysCount);

        const std::vector<IrArrayIndex> irArrays = extractIrArrayIndices(irArraysCount);

        for (const auto& irArray: irArrays) {
            const uint8_t* basePointer = _fileBuffer.data() + irArray.startIndex;

            google::protobuf::io::CodedInputStream input(basePointer, irArray.size);

            MessageType protoMessage;
            if (!protoMessage.ParseFromCodedStream(&input)) {
                throw std::runtime_error("Failed to parse protobuf message for type");
            }

            // Store the parsed message in our map, keyed by its ID.
            _protos.push_back(protoMessage);
        }
    }

    std::vector<uint8_t> _fileBuffer; // TODO: we don't need to keep the file buffer in memory
    std::vector<MessageType> _protos;
};
} // namespace kotlin::ir

#endif // IRARRAYREADER_HPP
