//
// Created by Gabriele.Pappalardo on 01/08/2025.
//

#ifndef IRFILESTABLE_HPP
#define IRFILESTABLE_HPP

#include "IrArrayReader.hpp"
#include "KotlinIr.pb.h"

namespace kotlin::ir {
using ProtoFile = org::jetbrains::kotlin::backend::common::serialization::proto::IrFile;
class IrFilesTable : public IrArrayReader<ProtoFile> {};
}

#endif // IRFILESTABLE_HPP
