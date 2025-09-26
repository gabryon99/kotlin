//
// Created by Gabriele.Pappalardo on 09/07/2025.
//

#include "IrDeclarationsTable.hpp"
#include "Utility.hpp"
#include "hot/HotReloadUtility.hpp"

#include <cassert>

static constexpr uint32_t FILE_HEADER_SIZE = sizeof(uint32_t);

void kotlin::ir::IrDeclarationsTable::loadFromFile(const std::string& filePath) {
    const auto fileContent = readFileContent(filePath);
    _fileBuffer = fileContent;
    parse();
}

const kotlin::ir::ProtoDeclaration* kotlin::ir::IrDeclarationsTable::getDeclaration(const uint32_t id) const {
    const auto it = _declarations.find(id);
    if (it != _declarations.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t kotlin::ir::IrDeclarationsTable::getDeclarationCount() const {
    return _declarations.size();
}

std::vector<uint32_t> kotlin::ir::IrDeclarationsTable::getDeclarationIds() const {
    std::vector<uint32_t> keys;
    keys.reserve(_declarations.size());
    for (const auto& [key, _] : _declarations) {
        keys.push_back(key);
    }
    return keys;
}

std::vector<kotlin::ir::IrDeclarationsTable::IrArrayIndex> kotlin::ir::IrDeclarationsTable::extractIrArrayIndices(
        const uint32_t irArraysCount) const {

    std::vector<IrArrayIndex> irArrays{};
    irArrays.reserve(irArraysCount);

    uint32_t byteOffsetToNextArray = FILE_HEADER_SIZE + (irArraysCount * sizeof(uint32_t)); // after the 4-bytes of how many arrays

    // dataOutput.writeInt(data.size)
    //
    // data.forEach { dataOutput.writeInt(it.size) }
    // data.forEach { dataOutput.write(it) }

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

void kotlin::ir::IrDeclarationsTable::parse() {

    // class IrArrayWriter(private val data: List<ByteArray>) : IrDataWriter() {
    //     override fun writeData(dataOutput: DataOutput) {
    //         dataOutput.writeInt(data.size)
    //
    //         data.forEach { dataOutput.writeInt(it.size) }
    //         data.forEach { dataOutput.write(it) }
    //     }
    // }

    // 1. How many arrays are contained inside the file
    // 2. For each array, write the size of the array
    // 3. For each array, write its content

    if (_fileBuffer.size() < 4) {
        throw std::runtime_error("File is too small to contain a declaration count.");
    }

    // How many ByteArray(s) are serialized in the file
    const uint32_t irArraysCount = readBigEndian32(_fileBuffer.data());

    const std::vector<IrArrayIndex> irArrays = extractIrArrayIndices(irArraysCount);

    for (const auto& irArray : irArrays) {

        const uint8_t* basePointer = _fileBuffer.data() + irArray.startIndex;

        if (basePointer >= _fileBuffer.data() + _fileBuffer.size()) {
            throw std::runtime_error("the basePointer cannot point outside the file");
        }

        const uint32_t declCount = readBigEndian32(basePointer); // how many declarations are contained
        // for each declaration, write
        // 1. the id
        // 2. the data offset
        // 3. the declaration size

        // for each declaration write its data content

        std::vector<DeclarationIndex> indexTable;
        indexTable.reserve(declCount);

        constexpr size_t INDEX_HEADER_SIZE = sizeof(uint32_t);
        constexpr size_t SINGLE_INDEX_RECORD_SIZE = 3 * sizeof(uint32_t);

        for (uint32_t i = 0; i < declCount; ++i) {

            const uint8_t* recordPtr = (basePointer + INDEX_HEADER_SIZE) + (i * SINGLE_INDEX_RECORD_SIZE);

            DeclarationIndex index{};
            index.id = readBigEndian32(recordPtr);
            index.dataOffset = readBigEndian32(recordPtr + 4);
            index.size = readBigEndian32(recordPtr + 8);
            indexTable.push_back(index);
        }

        for (const auto& [id, dataOffset, size] : indexTable) {

            google::protobuf::io::CodedInputStream input(basePointer + dataOffset, size);

            ProtoDeclaration protoMessage;
            if (!protoMessage.ParseFromCodedStream(&input)) {
                throw std::runtime_error("Failed to parse protobuf message for declaration ID: " + std::to_string(id));
            }

            // Store the parsed message in our map, keyed by its ID.
            _declarations[id] = protoMessage;
        }
    }
}
