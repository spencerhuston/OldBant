#pragma once

class Builtin;

#include "expressions.hpp"
#include "../core/builtin/builtinDefinitions.hpp"
#include "types.hpp"

#include <map>

namespace Values {
    class Value {
        public:
            Types::TypePtr type;

            explicit Value(const Types::TypePtr & type)
            : type(type) { }
    };

    using ValuePtr = std::shared_ptr<Value>;
    
    using Environment = std::shared_ptr<std::map<std::string, Values::ValuePtr>>;

    class IntValue : public Value {
        public:
            int data = 0;

            IntValue(const Types::TypePtr & type,
                     const int & data)
            : Value(type),
              data(data) { }
    };

    using IntValuePtr = std::shared_ptr<IntValue>;
    
    class CharValue : public Value {
        public:
            char data = (char)0;

            CharValue(const Types::TypePtr & type,
                      const char & data)
            : Value(type),
              data(data) { }
    };

    using CharValuePtr = std::shared_ptr<CharValue>;

    class StringValue : public Value {
        public:
            std::string data;

            StringValue(const Types::TypePtr & type,
                        const std::string & data)
            : Value(type),
              data(data) { }
    };

    using StringValuePtr = std::shared_ptr<StringValue>;

    class BoolValue : public Value {
        public:
            bool data = false;
            
            BoolValue(const Types::TypePtr & type,
                      const bool & data)
            : Value(type),
              data(data) { }
    };

    using BoolValuePtr = std::shared_ptr<BoolValue>;

    class NullValue : public Value {
        public:
            explicit NullValue(const Types::TypePtr & type)
            : Value(type) { }
    };

    using NullValuePtr = std::shared_ptr<NullValue>;

    class ListValue : public Value {
        public:
            std::vector<ValuePtr> listData;

            ListValue(const Types::TypePtr & type,
                      const std::vector<ValuePtr> & listData)
            : Value(type),
              listData(listData) { }
    };

    using ListValuePtr = std::shared_ptr<ListValue>;

    class TupleValue : public Value {
        public:
            std::vector<ValuePtr> tupleData;

            TupleValue(const Types::TypePtr & type,
                       const std::vector<ValuePtr> & tupleData)
            : Value(type),
              tupleData(tupleData) { }
    };

    using TupleValuePtr = std::shared_ptr<TupleValue>;

    class FunctionValue : public Value {
        public:
            std::vector<std::string> parameterNames;
            Expressions::ExpPtr functionBody;
            Environment functionBodyEnvironment;

            bool isBuiltin = false;
            BuiltinDefinitions::BuiltinEnums builtinEnum = BuiltinDefinitions::BuiltinEnums::BUILTINNUM;

            FunctionValue(const Types::TypePtr & type,
                          const std::vector<std::string> & parameterNames,
                          const Expressions::ExpPtr & functionBody,
                          const Environment & functionBodyEnvironment,
                          const bool & isBuiltin = false)
            : Value(type),
              parameterNames(parameterNames),
              functionBody(functionBody),
              functionBodyEnvironment(functionBodyEnvironment) { }
            
            ~FunctionValue() {
                if (type->dataType == Types::DataTypes::FUNC) {
                    if (std::static_pointer_cast<Types::FuncType>(type)->functionInnerEnvironment) {
                        std::static_pointer_cast<Types::FuncType>(type)->functionInnerEnvironment->clear();
                    }
                }
            }
    };

    using FunctionValuePtr = std::shared_ptr<FunctionValue>;

    class TypeclassValue : public Value {
        public:
            Environment fields;

            TypeclassValue(const Types::TypePtr & type,
                           const Environment & fields)
            : Value(type),
              fields(fields) { }
    };

    using TypeclassValuePtr = std::shared_ptr<TypeclassValue>;
}