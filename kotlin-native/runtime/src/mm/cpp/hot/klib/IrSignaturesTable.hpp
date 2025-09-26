//
// Created by Gabriele.Pappalardo on 11/07/2025.
//

#ifndef IRSIGNATURESTABLE_HPP
#define IRSIGNATURESTABLE_HPP

#include "IrArrayReader.hpp"
#include "KotlinIr.pb.h"

namespace kotlin::ir {
    using ProtoSignature = org::jetbrains::kotlin::backend::common::serialization::proto::IdSignature;
    class IrSignaturesTable : public IrLazyArrayReader {};
}

#endif //IRSIGNATURESTABLE_HPP
