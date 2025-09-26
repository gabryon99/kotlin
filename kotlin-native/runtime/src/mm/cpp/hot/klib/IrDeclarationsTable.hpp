//
// Created by Gabriele.Pappalardo on 09/07/2025.
//

#ifndef IRDECLARATION_HPP
#define IRDECLARATION_HPP

#include <vector>
#include <map>

#include "KotlinIr.pb.h"

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace kotlin::ir {

    using ProtoDeclaration = org::jetbrains::kotlin::backend::common::serialization::proto::IrDeclaration;

    class IrDeclarationsTable {
    public:
        void loadFromFile(const std::string& filePath);
        [[nodiscard]] const ProtoDeclaration* getDeclaration(uint32_t id) const;
        [[nodiscard]] size_t getDeclarationCount() const;
        [[nodiscard]] std::vector<uint32_t> getDeclarationIds() const;

    private:

        /**
         * @brief Represents a single entry in the file's index table.
         */
        struct DeclarationIndex {
            uint32_t id;
            uint32_t dataOffset;
            uint32_t size;
        };

        // Struct defining how big is an array and its start offset within the file
        struct IrArrayIndex {
            uint32_t startIndex{0};
            uint32_t size{0};
        };

        void parse();
        std::vector<uint8_t> _fileBuffer;
        std::map<uint32_t, ProtoDeclaration> _declarations;

        std::vector<IrArrayIndex> extractIrArrayIndices(uint32_t irArraysCount) const;
    };
}


#endif //IRDECLARATION_HPP
