//
// Created by Gabriele.Pappalardo on 09/07/2025.
//

#ifndef KLIB_HPP
#define KLIB_HPP

#include <optional>

#include "Utility.hpp"
#include "KotlinIr.pb.h"

#include "IrDeclarationsTable.hpp"
#include "IrBodiesTable.hpp"
#include "IrFilesTable.hpp"
#include "IrStringsTable.hpp"
#include "IrTypesTable.hpp"
#include "IrSignaturesTable.hpp"

/// INTERNAL NOTES
/// * `name_type` is a field containing two indexes (name and type, check BinaryLattice.kt)

namespace kotlin::ir {

using namespace org::jetbrains::kotlin::backend::common::serialization::proto;

class Klib {
public:
    
    struct KotlinFile {
        std::string fileName;
        std::string fqName;

        KotlinFile(const std::string& file_name, const std::string& fq_name) : fileName(file_name), fqName(fq_name) {}

        [[nodiscard]] const std::string& file_name() const { return fileName; }
        [[nodiscard]] const std::string& fq_name() const { return fqName; }
    };

    explicit Klib(const std::string& decompressedBasePath) {
        declarationTable_.loadFromFile(decompressedBasePath + kIrDirectory + kIrDeclarationsFile);
        stringsTable_.loadFromFile(decompressedBasePath + kIrDirectory + kStringsFile);
        typesTable_.loadFromFile(decompressedBasePath + kIrDirectory + kTypesFile);
        signaturesTable_.loadFromFile(decompressedBasePath + kIrDirectory + kSignaturesFile);
        bodiesTable_.loadFromFile(decompressedBasePath + kIrDirectory + kBodiesFile);
        filesTable_.loadFromFile(decompressedBasePath + kIrDirectory + kFilesFile);

        readContainedFilesFromTable();
    }

    [[nodiscard]]
    std::unique_ptr<IrExpression> getIrBodyAsExpression(const uint32_t exprBlockId) const {
        return bodiesTable_.getElementAt<IrExpression>(exprBlockId);
    }

    [[nodiscard]]
    std::unique_ptr<IrStatement> getIrBodyAsStatement(const uint32_t bodyBlockId) const {
        return bodiesTable_.getElementAt<IrStatement>(bodyBlockId);
    }

    [[nodiscard]]
    OptionalConstRef<const IrDeclaration> getIrDeclaration(const uint32_t declarationId) const {
        const auto decl = declarationTable_.getDeclaration(declarationId);
        if (decl == nullptr) return std::nullopt;
        return *decl;
    }

    [[nodiscard]]
    std::vector<uint32_t> getDeclarationIds() const { return declarationTable_.getDeclarationIds(); }

    [[nodiscard]]
    OptionalConstRef<std::string> getIrStringById(const uint32_t stringId) const {
        return stringsTable_.getString(stringId);
    }

    [[nodiscard]]
    std::unique_ptr<ProtoSignature> getSignatureBySym(const int64_t symbol) const {
        const uint32_t idx = static_cast<uint32_t>(symbol & 0xFFFFFFFFull);
        return signaturesTable_.getElementAt<ProtoSignature>(idx);
    }

    [[nodiscard]]
    std::unique_ptr<ProtoSignature> getSignatureById(const uint32_t signatureId) const {
        return signaturesTable_.getElementAt<ProtoSignature>(signatureId);
    }

    [[nodiscard]]
    std::unique_ptr<ProtoType> getIrType(const uint32_t typeId) const { return typesTable_.getElementAt<ProtoType>(typeId); }

    [[nodiscard]]
    OptionalConstRef<ProtoFile> getIrFile(const uint32_t fileIndex) const { return filesTable_.getElementAt(fileIndex); }

    std::string convertToFqnName(const google::protobuf::RepeatedField<google::protobuf::int32>& fields, const int32_t size) const {
        std::string result;
        result.reserve(size * 16); // Assuming average string length + dot
        for (int i = 0; i < size; i++) {
            const auto string = getIrStringById(fields.Get(i))->get();
            result += string;
            if (i != size - 1) {
                result += '.';
            }
        }
        return result;
    }

    uint32_t files() const { return filesTable_.getCount(); }

    std::optional<int> getFileIndexBySourceName(const std::string_view sourceName) const {
        for (size_t i = 0; i < containedFiles_.size(); ++i) {
            if (containedFiles_[i].fileName == sourceName) {
                return static_cast<int>(i);
            }
        }
        return std::nullopt;
    }

private:
    static constexpr auto kIrDirectory = "ir/";
    static constexpr auto kIrDeclarationsFile = "irDeclarations.knd";
    static constexpr auto kStringsFile = "strings.knt";
    static constexpr auto kTypesFile = "types.knt";
    static constexpr auto kSignaturesFile = "signatures.knt";
    static constexpr auto kBodiesFile = "bodies.knb";
    static constexpr auto kFilesFile = "files.knf";

    void readContainedFilesFromTable() {
        const auto fileCount = filesTable_.getCount();
        containedFiles_.reserve(fileCount);

        for (size_t i = 0; i < fileCount; i++) {
            if (const auto optionalFile = filesTable_.getElementAt(i)) {
                const auto& file = optionalFile.value().get();
                const auto fqName = convertToFqnName(file.fq_name(), file.fq_name_size());

                if (file.has_file_entry()) {
                    containedFiles_.push_back(KotlinFile{file.file_entry().name(), fqName});
                } else {
                    containedFiles_.push_back(KotlinFile{stringsTable_.getString(file.file_entry_id()).value().get(), fqName});
                }
            }
        }
    }

    IrDeclarationsTable declarationTable_;
    IrStringsTable stringsTable_;
    IrTypesTable typesTable_;
    IrSignaturesTable signaturesTable_;
    IrBodiesTable bodiesTable_;
    IrFilesTable filesTable_;

    /// What source files (both compiler-generated and non) are contained in the Klib.
    std::vector<KotlinFile> containedFiles_;
};
} // namespace kotlin::ir

#endif // KLIB_HPP
