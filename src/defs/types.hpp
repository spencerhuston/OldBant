#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace Expressions {
    class Expression;
}

namespace Types {
    enum class DataTypes {
        INT,
        CHAR,
        STRING,
        BOOL,
        NULLVAL,
        LIST,
        TUPLE,
        FUNC,
        GEN,
        TYPECLASS,
        UNKNOWN
    };

    class Type {
        public:
            DataTypes dataType = DataTypes::UNKNOWN;
            bool resolved = false;

            Type(const DataTypes dataType)
            : dataType(dataType) { }

            virtual const std::string toString() const = 0;

            virtual bool compare(const std::shared_ptr<Type> & otherType) {
                if (otherType == nullptr)
                    return false;
                else if (otherType->dataType == DataTypes::UNKNOWN) {
                    otherType->dataType = dataType;
                    return true;
                }

                auto dataTypeEnum = static_cast<int>(dataType);
                auto otherTypeEnum = static_cast<int>(otherType->dataType);
                
                return (dataTypeEnum == otherTypeEnum);
            }
    };

    using TypePtr = std::shared_ptr<Type>;

    class IntType : public Type {
        public:
            IntType() : Type(DataTypes::INT) { }

            const std::string toString() const override {
                return "int";
            }
    };

    using IntTypePtr = std::shared_ptr<IntType>;

    class CharType : public Type {
        public:
            CharType() : Type(DataTypes::CHAR) { }

            const std::string toString() const override {
                return "char";
            }
    };

    using CharTypePtr = std::shared_ptr<CharType>;

    class StringType : public Type {
        public:
            StringType() : Type(DataTypes::STRING) { }

            const std::string toString() const override {
                return "string";
            }
    };

    using StringTypePtr = std::shared_ptr<StringType>;

    class BoolType : public Type {
        public:
            BoolType() : Type(DataTypes::BOOL) { }

            const std::string toString() const override {
                return "bool";
            }
    };

    using BoolTypePtr = std::shared_ptr<BoolType>;

    class NullType : public Type {
        public:
            NullType() : Type(DataTypes::NULLVAL) { }

            const std::string toString() const override {
                return "null";
            }
    };

    using NullTypePtr = std::shared_ptr<NullType>;

    class ListType : public Type {
        public:
            TypePtr listType;

            explicit ListType(const TypePtr & listType)
            : Type(DataTypes::LIST),
              listType(listType) { }

            ListType()
            : Type(DataTypes::UNKNOWN),
              listType(nullptr) { }

            const std::string toString() const override {
                return std::string("List[") + listType->toString() + std::string("]");
            }

            bool compare(const std::shared_ptr<Type> & otherType) override {
                if (otherType == nullptr) {
                    return false;
                } else if (otherType->dataType == DataTypes::UNKNOWN) {
                    otherType->dataType = dataType;
                    return true;
                } else if (dataType == DataTypes::UNKNOWN &&
                           otherType->dataType == DataTypes::LIST) {
                    listType = std::static_pointer_cast<ListType>(otherType)->listType;
                    dataType = DataTypes::LIST;
                    return true;
                }

                auto dataTypeEnum = static_cast<int>(dataType);
                auto otherTypeEnum = static_cast<int>(otherType->dataType);

                if (dataTypeEnum == otherTypeEnum &&
                    listType->dataType == DataTypes::UNKNOWN) {
                    listType = std::static_pointer_cast<ListType>(otherType)->listType;
                    return true;
                }

                return (dataTypeEnum == otherTypeEnum) &&
                       (listType->compare(std::static_pointer_cast<ListType>(otherType)->listType));
            }
    };

    using ListTypePtr = std::shared_ptr<ListType>;

    class TupleType : public Type {
        public:
            std::vector<TypePtr> tupleTypes;

            explicit TupleType(const std::vector<TypePtr> & tupleTypes)
            : Type(DataTypes::TUPLE),
              tupleTypes(tupleTypes) { }

            TupleType()
            : Type(DataTypes::UNKNOWN),
              tupleTypes({}) { }

            const std::string toString() const override {
                std::string typeString("Tuple[");

                if (tupleTypes.empty()) {
                    typeString += std::string("]");
                } else if (tupleTypes.size() == 1) {
                    typeString += tupleTypes.at(0)->toString() + std::string("]");
                } else {
                    for (unsigned int typesIndex = 0; typesIndex < tupleTypes.size() - 1; ++typesIndex) {
                        typeString += tupleTypes.at(typesIndex)->toString() + std::string(", ");
                    }
                    typeString += tupleTypes.at(tupleTypes.size() - 1)->toString() + std::string("]");
                }

                return typeString;
            }

            bool compare(const std::shared_ptr<Type> & otherType) override {
                if (otherType == nullptr) {
                    return false;
                } else if (dataType == DataTypes::UNKNOWN &&
                           otherType->dataType == DataTypes::TUPLE) {
                    tupleTypes = std::static_pointer_cast<TupleType>(otherType)->tupleTypes;
                    dataType = DataTypes::TUPLE;
                    return true;
                }

                auto dataTypeEnum = static_cast<int>(dataType);
                auto otherTypeEnum = static_cast<int>(otherType->dataType);

                if (dataTypeEnum != otherTypeEnum)
                    return false;

                auto otherTupleType = std::static_pointer_cast<TupleType>(otherType);
                
                if (tupleTypes.size() != otherTupleType->tupleTypes.size()) {
                    return false;
                }

                for (unsigned int typeIndex = 0; typeIndex < tupleTypes.size(); ++typeIndex) {
                    auto type = tupleTypes.at(typeIndex);
                    auto otherTupleElementType = otherTupleType->tupleTypes.at(typeIndex);

                    if (type->dataType == DataTypes::UNKNOWN) {
                        tupleTypes.at(typeIndex) = otherTupleType->tupleTypes.at(typeIndex);
                        continue;
                    }

                    if (!type->compare(otherTupleElementType)) {
                        return false;
                    }
                }

                return true;
            }
    };

    using TupleTypePtr = std::shared_ptr<TupleType>;

    class GenType : public Type {
        public:
            const std::string identifier;
            
            explicit GenType(const std::string & identifier)
            : Type(DataTypes::GEN),
              identifier(identifier) { }
            
            const std::string toString() const override {
                return identifier;
            }
    };

    using GenTypePtr = std::shared_ptr<GenType>;
    
    class FuncType : public Type {
        public:
            std::vector<GenTypePtr> genericTypes;
            std::vector<TypePtr> argumentTypes;
            std::vector<std::string> argumentNames;
            TypePtr returnType;

            std::shared_ptr<Expressions::Expression> functionBody;
            std::shared_ptr<std::map<std::string, TypePtr>> functionInnerEnvironment;

            bool isBuiltin = false;

            FuncType(const std::vector<GenTypePtr> & genericTypes,
                     const std::vector<TypePtr> & argumentTypes,
                     const TypePtr & returnType)
            : Type(DataTypes::FUNC),
              genericTypes(genericTypes),
              argumentTypes(argumentTypes),
              returnType(returnType) { }

            ~FuncType() {
                if (functionInnerEnvironment)
                    functionInnerEnvironment->clear();
            }
            
            const std::string toString() const override {
                std::string typeString("[");

                if (genericTypes.empty()) {
                    typeString += std::string("]");
                } else if (genericTypes.size() == 1) {
                    typeString += genericTypes.at(0)->toString() + std::string("]");
                } else {
                    for (unsigned int typesIndex = 0; typesIndex < genericTypes.size() - 1; ++typesIndex) {
                        typeString += genericTypes.at(typesIndex)->toString() + std::string(", ");
                    }
                    typeString += genericTypes.at(genericTypes.size() - 1)->toString() + std::string("]");
                }

                typeString += std::string("(");
                if (argumentTypes.empty()) {
                    typeString += std::string(")");
                } else if (argumentTypes.size() == 1) {
                    typeString += argumentTypes.at(0)->toString() + std::string(")");
                } else {
                    for (unsigned int typesIndex = 0; typesIndex < argumentTypes.size() - 1; ++typesIndex) {
                        typeString += argumentTypes.at(typesIndex)->toString() + std::string(", ");
                    }
                    typeString += argumentTypes.at(argumentTypes.size() - 1)->toString() + std::string(")");
                }
                
                typeString += std::string("->");
                typeString += returnType->toString();

                return typeString;
            }

            bool compare(const std::shared_ptr<Type> & otherType) override {
                if (otherType == nullptr) {
                    return false;
                } else if (dataType == DataTypes::UNKNOWN &&
                           otherType->dataType == DataTypes::FUNC) {
                    auto funcType = std::static_pointer_cast<FuncType>(otherType);
                    genericTypes = funcType->genericTypes;
                    argumentTypes = funcType->argumentTypes;
                    returnType = funcType->returnType;
                    functionBody = funcType->functionBody;
                    functionInnerEnvironment = funcType->functionInnerEnvironment;
                    dataType = DataTypes::FUNC;
                    return true;
                } else if (dataType == DataTypes::FUNC &&
                           otherType->dataType == DataTypes::FUNC) {
                    auto otherFuncType = std::static_pointer_cast<FuncType>(otherType);

                    if (argumentTypes.size() != otherFuncType->argumentTypes.size()) {
                        return false;
                    }

                    for (unsigned int argumentIndex = 0; argumentIndex < argumentTypes.size(); ++argumentIndex) {
                        if (!argumentTypes.at(argumentIndex)->compare(otherFuncType->argumentTypes.at(argumentIndex))) {
                            return false;
                        }
                    }

                    if (!returnType->compare(otherFuncType->returnType)) {
                        return false;
                    }

                    return true;
                }

                return true;
            }
    };

    using FuncTypePtr = std::shared_ptr<FuncType>;

    class TypeclassType : public Type {
        public:
            const std::string ident;
            std::vector<std::pair<std::string, TypePtr>> fieldTypes{};

            explicit TypeclassType(const std::string & ident,
                                   const std::vector<std::pair<std::string, TypePtr>> & fieldTypes)
            : Type(DataTypes::TYPECLASS),
              ident(ident),
              fieldTypes(fieldTypes) { }

            explicit TypeclassType(const std::string & ident)
            : Type(DataTypes::TYPECLASS),
              ident(ident),
              fieldTypes({}) { }
            
            const std::string toString() const override {
                return ident;
            }

            bool compare(const std::shared_ptr<Type> & otherType) override {
                if (otherType == nullptr)
                    return false;

                auto dataTypeEnum = static_cast<int>(dataType);
                auto otherTypeEnum = static_cast<int>(otherType->dataType);

                return (dataTypeEnum == otherTypeEnum) &&
                       (ident == std::static_pointer_cast<TypeclassType>(otherType)->ident);
            }
    };

    using TypeclassTypePtr = std::shared_ptr<TypeclassType>;

    class UnknownType : public Type {
        public:
            UnknownType()
            : Type(DataTypes::UNKNOWN) { }
        
        const std::string toString() const override {
            return "unknown";
        }
    };

    using UnknownTypePtr = std::shared_ptr<UnknownType>;

    inline bool
    isPrimitiveType(const TypePtr type) {
        if (type->dataType == DataTypes::INT ||
            type->dataType == DataTypes::CHAR ||
            type->dataType == DataTypes::STRING ||
            type->dataType == DataTypes::BOOL ||
            type->dataType == DataTypes::GEN) {
            return true;
        }
        return false;
    }
}

using EnvironmentRaw = std::map<std::string, Types::TypePtr>;
using Environment = std::shared_ptr<EnvironmentRaw>;