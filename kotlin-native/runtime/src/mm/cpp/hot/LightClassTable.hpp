//
// Created by Gabriele.Pappalardo on 11/07/2025.
//

#ifndef CLASSESTABLE_HPP
#define CLASSESTABLE_HPP

#include <map>
#include <string>
#include <functional>
#include <optional>

#include "klib/Utility.hpp"
#include "klib/Klib.hpp"

namespace kotlin::hot {

class KotlinFunction {
public:
    KotlinFunction(const std::string& name, const std::string& return_type, const std::vector<std::string>& params) :
        name_(name), returnType_(return_type), params_(params) {}

    /// Given a Kotlin class fully-qualified-name, mangle the function name using the following schema:
    /// <code>
    /// kfun:#App(kotlin.Int;androidx.compose.runtime.Composer?;kotlin.Int){}
    /// </code>
    std::string mangledName(const std::string& fqnKotlinClassName) const {
        std::string result = kMangledPrefix;
        result += fqnKotlinClassName;
        result += kFunctionDelimiter;
        result += name_;
        result += kParamsPrefix;
        result += joinParameters();
        result += kParamsSuffix;
        if (returnType_ != kUnitType) result += returnType_;
        return result;
    }

    [[nodiscard]] std::string name() const { return name_; }
    [[nodiscard]] std::string return_type() const { return returnType_; }
    [[nodiscard]] std::string param(const size_t i) const { return i < params_.size() ? params_[i] : ""; }
    [[nodiscard]] size_t params_size() const { return params_.size(); }

private:
    static constexpr auto kMangledPrefix = "kfun:";
    static constexpr auto kFunctionDelimiter = "#";
    static constexpr auto kParamsPrefix = '(';
    static constexpr auto kParamsSuffix = "){}";
    static constexpr auto kParamSeparator = ';';
    static constexpr auto kUnitType = "kotlin.Unit";

    std::string name_;
    std::string returnType_;
    std::vector<std::string> params_;

    std::string joinParameters() const {
        if (params_.empty()) return "";
        std::string result = params_[0];
        for (size_t i = 1; i < params_.size(); ++i) {
            result += kParamSeparator;
            result += params_[i];
        }
        return result;
    }
};

class KotlinClass {
public:
    static constexpr auto kRootClassName = "";

    KotlinClass() {}

    explicit KotlinClass(
            const std::string& name,
            const std::string& fqn,
            const std::map<std::string, std::string>& properties,
            const std::vector<KotlinFunction>& functions) :
        name_(name), fqn_(fqn), properties_(properties), functions_(functions) {}

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const std::string& fqn() const { return fqn_; }
    [[nodiscard]] const std::map<std::string, std::string>& properties() const { return properties_; }
    [[nodiscard]] const std::vector<KotlinFunction>& functions() const { return functions_; }
    [[nodiscard]] const std::vector<KotlinClass>& super() const { return super_; }


private:
    std::string name_{};
    std::string fqn_{};
    std::map<std::string, std::string> properties_;
    std::vector<KotlinFunction> functions_;
    std::vector<KotlinClass> super_;
};

class LightClassTable {

public:
    explicit LightClassTable(const ir::Klib& klib) {
        const auto declarationIds = klib.getDeclarationIds();
        // 0000000000000638 T _kfun:#App(kotlin.Int;androidx.compose.runtime.Composer?;kotlin.Int){}
        // Start by placing root class which has no name...
        std::vector<KotlinFunction> rootFunctions;
        std::map<std::string, std::string> rootProperties;

        for (const auto declId : declarationIds) {
            auto& declaration = klib.getIrDeclaration(declId)->get();

            if (declaration.has_ir_class()) {
                visitClass(klib, declaration);
            } else if (declaration.has_ir_function()) {
                visitFunction(klib, declaration, rootFunctions);
            } else if (declaration.has_ir_property()) {
                visitProperty(klib, declaration, rootProperties);
            }
        }

        classes_[""] = KotlinClass("", "", rootProperties, rootFunctions);

        // TODO: visit parent classes :)
    }

    [[nodiscard]]
    OptionalConstRef<KotlinClass> getKotlinClass(const std::string& className) const {
        if (const auto it = classes_.find(className); it != classes_.end()) {
            return std::cref(it->second);
        }
        return std::nullopt;
    }

    std::string dumpTableAsString() const {
        if (classes_.empty()) return "<no defined classes>";

        std::stringstream ss{};

        for (auto& [className, clazz] : classes_) {
            if (className != "") {
                ss << "class " << clazz.name() << "(";
            } else {
                ss << "<root>\n";
            }

            for (auto& [name, type] : clazz.properties()) {
                ss << name << ":" << type << ",";
            }

            if (className != "") ss << ")\n";

            for (auto& func : clazz.functions()) {
                ss << "\t" << func.mangledName(clazz.name()) << "\n";
            }

            if (clazz.super().size() > 0) {
                ss << " : ";
                for (auto& superClazz : clazz.super()) {
                    ss << superClazz.name() << ", ";
                }
            }
            ss << "\n";
        }
        return ss.str();
    }

    std::vector<std::string> getFqnKotlinClassNames() const {
        std::vector<std::string> names{};
        names.reserve(classes_.size());
        for (auto& [name, _] : classes_) {
            names.push_back(name);
        }
        return names;
    }

    std::vector<std::string> getKotlinClassNames() const {
        std::vector<std::string> names{};
        names.reserve(classes_.size());
        for (auto& [_, clazz] : classes_) {
            if (clazz.name() == "") continue;
            names.push_back(clazz.name());
        }
        return names;
    }

private:
    std::map<std::string, KotlinClass> classes_;

    static std::string combinePackageNameWithDeclNameAndTypeArgs(const std::string& packageName, const std::string& declName, bool isNullable, const std::vector<std::string>& args) {
        std::string name{};
        name.reserve(packageName.size() + declName.size());

        if (packageName.empty()) {
            name += declName;
        } else {
            name += packageName + "." + declName;
        }

        if (!args.empty()) {
            name += "<";
            for (const auto& arg : args) {
                name += arg;
            }
            name += ">";
        }

        if (isNullable) {
            name += "?";
        }

        return name;
    }

    static std::string convertTypeToString(const ir::Klib& klib, const ir::ProtoType& protoType) {
        using namespace org::jetbrains::kotlin::backend::common::serialization::proto;

        if (!protoType.has_simple()) {
            utility::log("Cannot find type arguments on not IrSimpleType");
            return {};
        }

        const auto signatureId = ir::ushr(protoType.simple().classifier(), 8); // check BinarySymbolData.kt
        const auto& signature = klib.getSignatureById(signatureId);

        const bool isNullable = protoType.simple().has_nullability() ? protoType.simple().nullability() == MARKED_NULLABLE : false;

        const auto typePackageName =
                klib.convertToFqnName(signature->public_sig().package_fq_name(), signature->public_sig().package_fq_name_size());
        const auto typeDeclName =
                klib.convertToFqnName(signature->public_sig().declaration_fq_name(), signature->public_sig().declaration_fq_name_size());

        std::vector<std::string> typeArguments{};
        typeArguments.reserve(protoType.simple().argument_size());

        for (const auto arg : protoType.simple().argument()) {
            // repeated int64 argument = 4 [packed=true]; // 0 - STAR, otherwise [63..2 - IrType index | 1..0 - Variance]
            const uint64_t typeIndex = arg >> 2;
            const auto argType = klib.getIrType(typeIndex);
            typeArguments.push_back(convertTypeToString(klib, *argType));
        }

        return combinePackageNameWithDeclNameAndTypeArgs(typePackageName, typeDeclName, isNullable, typeArguments);
    }

    static void visitFunction(const ir::Klib& klib, const ir::ProtoDeclaration& decl, std::vector<KotlinFunction>& functions) {
        using namespace org::jetbrains::kotlin::backend::common::serialization::proto;

        if (!decl.has_ir_function()) return; // base case

        const auto& irFunctionBase = decl.ir_function().base();
        auto [nameId, typeId] = ir::decodeNameAndType(irFunctionBase.name_type());

        const auto functionNameOpt = klib.getIrStringById(nameId);
        if (!functionNameOpt.has_value()) {
            utility::log("Cannot find string with nameId=" + std::to_string(nameId));
            return;
        }

        const auto functionName = functionNameOpt->get(); // TODO: crash here??
        const auto type = klib.getIrType(typeId);

        std::vector<std::string> params;
        params.reserve(irFunctionBase.regular_parameter_size());

        for (auto& irParam : irFunctionBase.regular_parameter()) {
            auto [paramNameId, paramTypeId] = ir::decodeNameAndType(irParam.name_type());
            // const auto paramName = klib.getIrString(paramNameId)->get();
            const auto& paramType = klib.getIrType(paramTypeId);
            if (!paramType) throw std::runtime_error{"The type yielded by " + std::to_string(paramTypeId) + " is null"};
            params.push_back(convertTypeToString(klib, *paramType));
        }

        functions.push_back(KotlinFunction{functionName, convertTypeToString(klib, *type), params});
    }

    static void visitProperty(const ir::Klib& klib, const ir::ProtoDeclaration& decl, std::map<std::string, std::string>& properties) {
        if (!decl.has_ir_property()) return;

        const auto& irProperty = decl.ir_property();
        if (!irProperty.has_backing_field()) return; // we only care about property with backing fields

        const std::string propertyName = klib.getIrStringById(irProperty.name())->get();

        auto& backingField = irProperty.backing_field();
        auto [nameId, typeId] = ir::decodeNameAndType(backingField.name_type());

        const auto type = klib.getIrType(typeId);
        properties[propertyName] = convertTypeToString(klib, *type);
    }

    void visitClass(const ir::Klib& klib, std::reference_wrapper<const org::jetbrains::kotlin::backend::common::serialization::proto::IrDeclaration>::type& declaration) {
        auto& irClass = declaration.ir_class();
        const std::string className = klib.getIrStringById(irClass.name())->get();
        std::string fqn = "." + className;
        std::map<std::string, std::string> properties;
        std::vector<KotlinFunction> functions;
        for (auto& d : irClass.declaration()) {
            visitProperty(klib, d, properties);
            visitFunction(klib, d, functions);
        }
        classes_.emplace(fqn, KotlinClass(className, fqn, properties, functions));
    }
};
} // namespace kotlin::hot

#endif // CLASSESTABLE_HPP
