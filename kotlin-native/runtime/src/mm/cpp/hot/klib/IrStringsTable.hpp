//
// Created by Gabriele.Pappalardo on 09/07/2025.
//

#ifndef IRSTRINGSTABLE_HPP
#define IRSTRINGSTABLE_HPP

#include <map>
#include <string>
#include <optional>
#include <functional>
#include <vector>

namespace kotlin::ir {

    class IrStringsTable {
    public:
        void loadFromFile(const std::string& filePath);
        [[nodiscard]] std::optional<std::reference_wrapper<const std::string>> getString(uint32_t index) const;

    private:
        using FileIndex = uint32_t;
        using StringIndex = uint32_t;

        struct ArrayIndex {
            uint32_t startIndex{0};
            uint32_t size{0};
        };

        void parse();

        std::vector<ArrayIndex> extractArrayIndices(uint32_t arraysCount) const;

        std::unordered_map<uint32_t, std::vector<std::string>> _strings;
        std::vector<uint8_t> _fileBuffer{};
    };

};

#endif //IRSTRINGSTABLE_HPP
