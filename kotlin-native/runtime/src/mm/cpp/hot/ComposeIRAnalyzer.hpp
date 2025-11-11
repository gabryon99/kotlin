//
// Created by Gabriele.Pappalardo on 31/07/2025.
//

#ifndef COMPOSEIRANALYZER_HPP
#define COMPOSEIRANALYZER_HPP

#include "klib/Utility.hpp"
#include "klib/Klib.hpp"

#include "klib/KotlinIr.pb.h"

#include <string>
#include <vector>

namespace kotlin::hot {

using namespace org::jetbrains::kotlin::backend::common::serialization::proto;

struct ComposeGroup {
    std::string functionName;
    int groupKey;
};

// static const CommonIdSignature* asCommon(const IdSignature& sig, const std::function<const IdSignature&(int32_t)>& idSigByIndex) {
//     switch (sig.idSig_case()) {
//         case IdSignature::kPublicSig:
//             return &sig.public_sig();
//         case IdSignature::kCompositeSig: {
//             const auto& comp = sig.composite_sig();
//             const IdSignature& inner = idSigByIndex(comp.inner_sig());
//             return asCommon(inner, idSigByIndex);
//         }
//         default:
//             return nullptr;
//     }
// }
//
// static bool isComposableAnnotationSignature(const CommonIdSignature& csig, const std::function<std::string(int32_t)>& str) {
//     // annotation interface androidx.compose.runtime.Composable {}
//
//     for (int i = 0; i < csig.package_fq_name_size(); i++) {
//         std::cout << str(csig.package_fq_name(i)) << ".";
//     }
//     std::cout << "\n";
//
//     if (csig.package_fq_name_size() != 3) return false;
//     if (str(csig.package_fq_name(0)) != "androidx") return false;
//     if (str(csig.package_fq_name(1)) != "compose") return false;
//     if (str(csig.package_fq_name(2)) != "runtime") return false;
//
//     const int n = csig.declaration_fq_name_size();
//     if (n == 0) return false;
//
//     const std::string last = str(csig.declaration_fq_name(n - 1));
//     if (last == "Composable") {
//         // Points directly at the class
//         return true;
//     }
//     if (last == "<init>" && n >= 2) {
//         const std::string owner = str(csig.declaration_fq_name(n - 2));
//         return owner == "Composable";
//     }
//
//     // Fallback: accept any path segment named Composable
//     for (int i = 0; i < n; ++i) {
//         if (str(csig.declaration_fq_name(i)) == "Composable") return true;
//     }
//
//     return false;
// }
//
// static bool hasComposableAnnotation(
//         const IrFunction& fun,
//         const std::function<const IdSignature&(int64_t)>& idSigBySymbol,
//         const std::function<const IdSignature&(int32_t)>& idSigByIndex,
//         const std::function<std::string(int32_t)>& idToStrFun) {
//
//     const auto& annos = fun.base().base().annotation();
//
//     for (const IrConstructorCall& anno : annos) {
//         const IdSignature& ctorSig = idSigBySymbol(anno.symbol());
//         const CommonIdSignature* csig = asCommon(ctorSig, idSigByIndex);
//         if (!csig) continue;
//
//         if (isComposableAnnotationSignature(*csig, idToStrFun)) return true;
//     }
//     return false;
// }

/// Find all the available Compose group keys defined in a given Klib.
inline std::vector<ComposeGroup> findComposeGroups(const ir::Klib& klib) {
    using namespace org::jetbrains::kotlin::backend::common::serialization::proto;

    auto strIdToString = [&klib](const int32_t id) -> std::string { return klib.getIrStringById(id)->get(); };
    auto getValueArgs = [](const MemberAccessCommon& mac) -> const google::protobuf::RepeatedPtrField<NullableIrExpression>& {
        if (mac.argument_size() > 0) return mac.argument(); // 2.2.0+
        return mac.regular_argument(); // older serialization
    };

    std::vector<ComposeGroup> groups{};

    const auto declarationIds = klib.getDeclarationIds();
    for (auto declId : declarationIds) {
        const auto& declaration = klib.getIrDeclaration(declId)->get();
        if (!declaration.has_ir_function()) continue; // skip everything that is not a function declaration

        const auto irFunction = declaration.ir_function();
        const auto& irFunctionBase = irFunction.base();
        const auto [funcNameId, _] = ir::decodeNameAndType(irFunctionBase.name_type());
        const auto& funcName = strIdToString(funcNameId);

        utility::log("Looking for Compose group inside: " + funcName, utility::LogLevel::DEBUG);
        if (funcName != "App") continue;

        // if (!hasComposableAnnotation(
        //             irFunction,
        //             [&](int64_t sym) -> const IdSignature& { return klib.getSignatureBySym(sym); },
        //             [&](int32_t idx) -> const IdSignature& { return klib.getSignatureById(idx); }, strIdToString)) {
        //     utility::log("No @Composable annotation found for function: " + funcName, utility::LogLevel::DEBUG);
        //     // continue;
        // }

        // TODO: I think a Composable function can be an expression only(?)
        // TODO: Probably yes, but let's ignore them atm.

        const auto irBody = klib.getIrBodyAsStatement(irFunctionBase.body());
        if (!irBody) continue;

        if (!irBody->has_block_body()) {
            utility::log("Function '" + funcName + "' does not have a block body.", utility::LogLevel::WARN);
            continue;
        }

        const auto& blockBody = irBody->block_body();
        for (const auto& stmt : blockBody.statement()) {
            if (stmt.statement_case() != IrStatement::kExpression) continue;

            const auto& op = stmt.expression().operation();
            if (op.operation_case() != IrOperation::kBlock) continue;

            for (const auto& innerStmt : op.block().statement()) {
                if (innerStmt.statement_case() != IrStatement::kExpression) continue;

                const auto& innerOp = innerStmt.expression().operation();
                if (innerOp.operation_case() != IrOperation::kSetValue) continue;

                const auto& innerInnerOp = innerOp.set_value().value().operation();
                if (innerInnerOp.operation_case() != IrOperation::kCall) continue;

                const auto& irCall = innerInnerOp.call();
                const auto& mac = irCall.member_access();
                const auto& args = getValueArgs(mac);

                if (args.size() != 2) continue;

                const NullableIrExpression& narg1 = args.Get(1); // index 0 (not 1)
                const IrExpression& arg1 = narg1.expression();
                const IrOperation& argOp = arg1.operation();

                if (argOp.operation_case() != IrOperation::kConst) continue;

                const IrConst& c = argOp.const_();
                if (c.value_case() != IrConst::kInt) continue;

                const int composeGroupKey = c.int_();

                ComposeGroup newGroup{};
                newGroup.functionName = funcName;
                newGroup.groupKey = composeGroupKey;

                groups.push_back(newGroup);
            }
        }
    }

    return groups;
}

inline std::optional<long> parseComposeGroupKeyFromSingletonLambda(const std::string_view s) {
    // 1. Find the position of the '$'
    if (auto pos1 = s.find('$'); pos1 != std::string_view::npos) {

        // 2. Find the position of the '>' after the '$'
        if (auto pos2 = s.find('>', pos1); pos2 != std::string_view::npos) {

            // 3. Create a view of just the number part
            auto number_sv = s.substr(pos1 + 1, pos2 - pos1 - 1);

            // 4. Convert the string view to a number
            long value{};
            auto [ptr, ec] = std::from_chars(number_sv.data(),
                                             number_sv.data() + number_sv.size(),
                                             value);

            if (ec == std::errc()) { // Check for errors
                return value;
            }
        }
    }
    return std::nullopt; // Return empty if not found or conversion fails
}

} // namespace kotlin::hot

#endif // COMPOSEIRANALYZER_HPP
