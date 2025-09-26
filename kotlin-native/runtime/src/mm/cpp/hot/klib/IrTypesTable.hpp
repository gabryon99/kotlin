//
// Created by Gabriele.Pappalardo on 11/07/2025.
//

#ifndef IRTYPESTABLE_HPP
#define IRTYPESTABLE_HPP

#include "IrLazyArrayReader.hpp"
#include "KotlinIr.pb.h"

namespace kotlin::ir {
    using ProtoType = org::jetbrains::kotlin::backend::common::serialization::proto::IrType;
    class IrTypesTable : public IrLazyArrayReader {};
}

#endif //IRTYPESTABLE_HPP
