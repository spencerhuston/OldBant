#include "interpreter.hpp"

Interpreter::Interpreter(const ExpPtr & rootExpression)
: rootExpression(rootExpression),
  errorNullValue(std::make_shared<Values::NullValue>(std::make_shared<Types::NullType>())) 
{ }

void
Interpreter::run() {
    Values::Environment environment = std::make_shared<std::map<std::string, Values::ValuePtr>>();
    interpret(rootExpression, environment);
    environment->clear();
}

Values::ValuePtr
Interpreter::interpret(const ExpPtr & expression, Values::Environment & environment) {
    if (expression->expType == ExpressionTypes::PROG)
        return interpretProgram(expression, environment);
    else if (expression->expType == ExpressionTypes::LIT)
        return interpretLiteral(expression, environment);
    else if (expression->expType == ExpressionTypes::PRIM)
        return interpretPrimitive(expression, environment);
    else if (expression->expType == ExpressionTypes::LET)
        return interpretLet(expression, environment);
    else if (expression->expType == ExpressionTypes::REF)
        return interpretReference(expression, environment);
    else if (expression->expType == ExpressionTypes::BRANCH)
        return interpretBranch(expression, environment);
    else if (expression->expType == ExpressionTypes::TYPECLASS)
        return interpretTypeclass(expression, environment);
    else if (expression->expType == ExpressionTypes::APP)
        return interpretApplication(expression, environment);
    else if (expression->expType == ExpressionTypes::LIST_DEF)
        return interpretListDefinition(expression, environment);
    else if (expression->expType == ExpressionTypes::TUPLE_DEF)
        return interpretTupleDefinition(expression, environment);
    else if (expression->expType == ExpressionTypes::MATCH)
        return interpretMatch(expression, environment);
    else if (expression->expType == ExpressionTypes::END)
        return errorNullValue;

    printError(expression->token, std::string("Unknown expression type: ") + expression->token.text);

    return errorNullValue;
}

Values::ValuePtr
Interpreter::interpretProgram(const ExpPtr & expression, const Values::Environment & environment) {
    auto program = std::static_pointer_cast<Program>(expression);

    Values::Environment programEnvironment = environment;

    for (auto & function : program->functions) {
        std::vector<std::string> parameterNames{};
        std::transform(function->parameters.begin(), function->parameters.end(), std::back_inserter(parameterNames),
                        [](const std::shared_ptr<Argument> & parameter) -> std::string { return parameter->name; });
        
        auto functionValue = std::make_shared<Values::FunctionValue>(function->returnType, 
                                                                    parameterNames, 
                                                                    function->functionBody, 
                                                                    std::make_shared<std::map<std::string, Values::ValuePtr>>());

        if (BuiltinDefinitions::isBuiltin(function->name)) {
            functionValue->isBuiltin = true;
            functionValue->builtinEnum = BuiltinDefinitions::getBuiltin(function->name);
        } else {
            functionValue->functionBodyEnvironment = std::make_shared<std::map<std::string, Values::ValuePtr>>(*programEnvironment);
            functionValue->functionBodyEnvironment->erase(function->name);
        }

        addName(programEnvironment, function->name, functionValue);
    }

    return interpret(program->body, programEnvironment);
}

Values::ValuePtr
Interpreter::interpretLiteral(const ExpPtr & expression, const Values::Environment & environment) {
    auto literal = std::static_pointer_cast<Literal>(expression);

    if (literal->returnType->dataType == Types::DataTypes::INT) {
        return std::make_shared<Values::IntValue>(literal->returnType, std::get<int>(literal->data));
    } else if (literal->returnType->dataType == Types::DataTypes::CHAR) {
        return std::make_shared<Values::CharValue>(literal->returnType, std::get<char>(literal->data));
    } else if (literal->returnType->dataType == Types::DataTypes::STRING) {
        return std::make_shared<Values::StringValue>(literal->returnType, std::get<std::string>(literal->data));
    } else if (literal->returnType->dataType == Types::DataTypes::BOOL) {
        return std::make_shared<Values::BoolValue>(literal->returnType, std::get<bool>(literal->data));
    } else if (literal->returnType->dataType == Types::DataTypes::NULLVAL) {
        return std::make_shared<Values::NullValue>(literal->returnType);
    }

    printError(literal->token, std::string("Error: Unknown literal type: ") + 
               literal->token.position.currentLineText);
    return errorNullValue;
}

Values::ValuePtr
Interpreter::interpretPrimitive(const ExpPtr & expression, Values::Environment & environment) {
    auto primitive = std::static_pointer_cast<Primitive>(expression);
    auto leftValue = interpret(primitive->leftSide, environment);
    auto rightValue = interpret(primitive->rightSide, environment);

    if (leftValue->type->dataType == Types::DataTypes::INT) {
        return doOperation<Values::IntValue>(primitive->token, primitive->op, leftValue, rightValue);
    } else if (leftValue->type->dataType == Types::DataTypes::CHAR) {
        return doOperation<Values::CharValue>(primitive->token, primitive->op, leftValue, rightValue);
    } else if (leftValue->type->dataType == Types::DataTypes::STRING) {
        return doOperation<Values::StringValue>(primitive->token, primitive->op, leftValue, rightValue);
    } else if (leftValue->type->dataType == Types::DataTypes::BOOL) {
        return doOperation<Values::BoolValue>(primitive->token, primitive->op, leftValue, rightValue);
    }

    printError(primitive->token, std::string("Error: Binary operator requires primitive types: ")  + 
               primitive->token.position.currentLineText);
    return errorNullValue;
}

Values::ValuePtr
Interpreter::interpretLet(const ExpPtr & expression, Values::Environment & environment) {
    auto let = std::static_pointer_cast<Let>(expression);

    auto letValue = interpret(let->value, environment);
    Values::Environment afterLetEnvironment = environment;
    addName(afterLetEnvironment, let->ident, letValue);
    return interpret(let->afterLet, afterLetEnvironment);
}

Values::ValuePtr
Interpreter::interpretReference(const ExpPtr & expression, Values::Environment & environment) {
    auto reference = std::static_pointer_cast<Reference>(expression);

    auto referenceValue = getName(reference->token, environment, reference->ident);
    if (referenceValue->type->dataType == Types::DataTypes::TUPLE && !reference->fieldIdent.empty()) {
        auto tupleValue = std::static_pointer_cast<Values::TupleValue>(referenceValue);
        int tupleIndex = std::stoi(reference->fieldIdent);
        return tupleValue->tupleData.at(tupleIndex);
    } else if (referenceValue->type->dataType == Types::DataTypes::TYPECLASS && !reference->fieldIdent.empty()) {
        auto typeclassValue = std::static_pointer_cast<Values::TypeclassValue>(referenceValue);
        auto fieldValue = typeclassValue->fields->find(reference->fieldIdent);
        if (fieldValue == environment->end()) {
            printError(reference->token, std::string("Error: typeclass ") +
                       reference->ident + std::string(" has no field ") +
                       reference->fieldIdent);
            return errorNullValue;
        }
        return fieldValue->second;
    }

    return referenceValue;
}

Values::ValuePtr
Interpreter::interpretBranch(const ExpPtr & expression, Values::Environment & environment) {
    auto branch = std::static_pointer_cast<Branch>(expression);

    auto conditionValue = std::static_pointer_cast<Values::BoolValue>(interpret(branch->condition, environment));
    if (conditionValue->data) {
        return interpret(branch->ifBranch, environment);
    } 

    return interpret(branch->elseBranch, environment);
}

Values::ValuePtr
Interpreter::interpretTypeclass(const ExpPtr & expression, Values::Environment & environment) {
    auto typeclass = std::static_pointer_cast<Typeclass>(expression);

    Values::Environment fields = std::make_shared<std::map<std::string, Values::ValuePtr>>();
    for (unsigned int fieldIndex = 0; fieldIndex < typeclass->fields.size(); ++fieldIndex) {
        auto initValue = std::make_shared<Values::Value>(std::make_shared<Types::UnknownType>());
        addName(fields, typeclass->fields.at(fieldIndex)->name, initValue);
    }

    auto typeclassValue = std::make_shared<Values::TypeclassValue>(typeclass->returnType, fields);
    addName(environment, typeclass->ident, typeclassValue);

    return typeclassValue;
}

Values::ValuePtr
Interpreter::interpretApplication(const ExpPtr & expression, Values::Environment & environment) {
    auto application = std::static_pointer_cast<Application>(expression);

    auto ident = interpret(application->ident, environment);
    if (ident->type->dataType == Types::DataTypes::TYPECLASS) {
        auto typeclassValue = std::static_pointer_cast<Values::TypeclassValue>(ident);
        auto typeclassType = std::static_pointer_cast<Types::TypeclassType>(typeclassValue->type);
        auto typeclassFields = std::make_shared<std::map<std::string, Values::ValuePtr>>(*typeclassValue->fields);
        for (unsigned int argumentIndex = 0; argumentIndex < application->arguments.size(); ++argumentIndex) {
            addName(typeclassFields, typeclassType->fieldTypes.at(argumentIndex).first, interpret(application->arguments.at(argumentIndex), environment));
        }
        return std::make_shared<Values::TypeclassValue>(typeclassType, typeclassFields);
    } else if (ident->type->dataType == Types::DataTypes::LIST) {
        unsigned int index = std::static_pointer_cast<Values::IntValue>(interpret(application->arguments.at(0), environment))->data;
        auto listValue = std::static_pointer_cast<Values::ListValue>(ident);
        if (index >= listValue->listData.size()) {
            printError(application->token, "Error: Out of bounds list access: " + application->token.position.currentLineText);
            return errorNullValue;
        }
        return listValue->listData.at(index);
    }
    
    // else has to be a function type

    if (application->ident->expType == ExpressionTypes::REF) {
        auto funcIdent = std::static_pointer_cast<Reference>(application->ident);
        callStack.push_back(std::make_pair(funcIdent->ident, funcIdent->token));
    }

    auto functionValue = std::static_pointer_cast<Values::FunctionValue>(ident);
    Values::Environment functionEnvironment = std::make_shared<std::map<std::string, Values::ValuePtr>>(*environment);
    for (unsigned int argumentIndex = 0; argumentIndex < application->arguments.size(); ++argumentIndex) {
        addName(functionEnvironment, functionValue->parameterNames.at(argumentIndex), interpret(application->arguments.at(argumentIndex), environment));
    }

    for (auto & environmentVariable : *(functionValue->functionBodyEnvironment)) { // STILL REACHABLE LEAK
        if (!BuiltinDefinitions::isBuiltin(environmentVariable.first)) {
            addName(functionEnvironment, environmentVariable.first, environmentVariable.second);
        }
    }

    if (functionValue->isBuiltin) {
        return BuiltinImplementations::runBuiltin(application->token, functionValue, functionEnvironment);
    }

    return interpret(functionValue->functionBody, functionEnvironment);
}

Values::ValuePtr
Interpreter::interpretListDefinition(const ExpPtr & expression, Values::Environment & environment) {
    auto listDefinition = std::static_pointer_cast<ListDefinition>(expression);

    std::vector<Values::ValuePtr> listData;
    std::transform(listDefinition->values.begin(), listDefinition->values.end(), std::back_inserter(listData),
                    [this, &environment](const ExpPtr & element) -> Values::ValuePtr { return interpret(element, environment); });

    return std::make_shared<Values::ListValue>(listDefinition->returnType, listData);
}

Values::ValuePtr
Interpreter::interpretTupleDefinition(const ExpPtr & expression, Values::Environment & environment) {
    auto tupleDefinition = std::static_pointer_cast<TupleDefinition>(expression);

    std::vector<Values::ValuePtr> tupleData;
    std::transform(tupleDefinition->values.begin(), tupleDefinition->values.end(), std::back_inserter(tupleData),
                    [this, &environment](const ExpPtr & element) -> Values::ValuePtr { return interpret(element, environment); });

    return std::make_shared<Values::TupleValue>(tupleDefinition->returnType, tupleData);
}

Values::ValuePtr
Interpreter::interpretMatch(const ExpPtr & expression, Values::Environment & environment) {
    auto match = std::static_pointer_cast<Match>(expression);
    auto matchValue = getName(match->token, environment, match->ident);

    for (auto & casePtr : match->cases) {
        if (casePtr->ident->expType == ExpressionTypes::REF &&
            std::static_pointer_cast<Reference>(casePtr->ident)->ident == std::string("$any")) {
            return interpret(casePtr->body, environment);
        }

        auto caseValue = interpret(casePtr->ident, environment);
        
        Values::ValuePtr resultValue;
        if (matchValue->type->dataType == Types::DataTypes::INT) {
            resultValue = doOperation<Values::IntValue>(match->token, Operator::OperatorTypes::EQ, matchValue, caseValue);
        } else if (matchValue->type->dataType == Types::DataTypes::CHAR) {
            resultValue = doOperation<Values::CharValue>(match->token, Operator::OperatorTypes::EQ, matchValue, caseValue);
        } else if (matchValue->type->dataType == Types::DataTypes::STRING) {
            resultValue = doOperation<Values::StringValue>(match->token, Operator::OperatorTypes::EQ, matchValue, caseValue);
        } else if (matchValue->type->dataType == Types::DataTypes::BOOL) {
            resultValue = doOperation<Values::BoolValue>(match->token, Operator::OperatorTypes::EQ, matchValue, caseValue);
        }

        if (std::static_pointer_cast<Values::BoolValue>(resultValue)->data) {
            return interpret(casePtr->body, environment);
        }
    }

    return errorNullValue;
}

void
Interpreter::addName(Values::Environment & environment, std::string name, Values::ValuePtr value) {
    if (environment->count(name)) {
		environment->erase(name); // STILL REACHABLE LEAK
	}
	environment->insert(std::pair<std::string, Values::ValuePtr>(name, value)); // STILL REACHABLE LEAK
}

Values::ValuePtr
Interpreter::getName(const Token & token, Values::Environment & environment, std::string name) {
    auto value = environment->find(name);
    if (value == environment->end()) {
        printError(token, std::string("Error: ") + name + 
                          std::string(" does not exist in this scope"));
        return errorNullValue;
    }
    return value->second;
}

template<typename PrimitiveType>
Values::ValuePtr
Interpreter::doOperation(const Token & token, Operator::OperatorTypes op, const Values::ValuePtr & leftSide, const Values::ValuePtr & rightSide) {
    if (op == Operator::OperatorTypes::PLUS) {
        return std::make_shared<Values::IntValue>(std::make_shared<Types::IntType>(),
                                                  std::static_pointer_cast<Values::IntValue>(leftSide)->data +
                                                  std::static_pointer_cast<Values::IntValue>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::MINUS) {
        return std::make_shared<Values::IntValue>(std::make_shared<Types::IntType>(),
                                                  std::static_pointer_cast<Values::IntValue>(leftSide)->data -
                                                  std::static_pointer_cast<Values::IntValue>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::TIMES) {
        return std::make_shared<Values::IntValue>(std::make_shared<Types::IntType>(),
                                                  std::static_pointer_cast<Values::IntValue>(leftSide)->data *
                                                  std::static_pointer_cast<Values::IntValue>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::DIV) {
        if (std::static_pointer_cast<Values::IntValue>(rightSide)->data == 0) {
            error = true;

            printError(token, "Error: Division by zero!");
            return errorNullValue;
        } else {
            return std::make_shared<Values::IntValue>(std::make_shared<Types::IntType>(),
                                                  std::static_pointer_cast<Values::IntValue>(leftSide)->data /
                                                  std::static_pointer_cast<Values::IntValue>(rightSide)->data);
        }
    } else if (op == Operator::OperatorTypes::MOD) {
        return std::make_shared<Values::IntValue>(std::make_shared<Types::IntType>(),
                                                  std::static_pointer_cast<Values::IntValue>(leftSide)->data %
                                                  std::static_pointer_cast<Values::IntValue>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::GRT) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data >
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::LST) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data <
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::NOT) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data ==
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::EQ) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data ==
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::NOTEQ) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data !=
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::GRTEQ) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data >=
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::LSTEQ) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<PrimitiveType>(leftSide)->data <=
                                                   std::static_pointer_cast<PrimitiveType>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::AND) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<Values::BoolValue>(leftSide)->data &&
                                                   std::static_pointer_cast<Values::BoolValue>(rightSide)->data);
    } else if (op == Operator::OperatorTypes::OR) {
        return std::make_shared<Values::BoolValue>(std::make_shared<Types::BoolType>(),
                                                   std::static_pointer_cast<Values::BoolValue>(leftSide)->data ||
                                                   std::static_pointer_cast<Values::BoolValue>(rightSide)->data);
    }

    return errorNullValue;
}

const std::string
Interpreter::getStackTraceString() {
    std::stringstream stackStream;
    stackStream << "Fatal error occurred:\n";
    for (auto rIterator = callStack.rbegin(); rIterator != callStack.rend(); ++rIterator) {
        stackStream << "\tat \'" << rIterator->first << "\' (Line: " << rIterator->second.position.fileLine << ")\n";
    }
    return stackStream.str();
}

void
Interpreter::printError(const Token & token, const std::string & errorMessage) {
    error = true;

    std::stringstream errorStream;
    errorStream << "Line: " << token.position.fileLine - BuiltinDefinitions::builtinNumber()
                << ", Column: " << token.position.fileColumn << std::endl
                << errorMessage << std::endl 
                << token.position.currentLineText << std::endl;
    ERROR(errorStream.str());
    ERROR(getStackTraceString());

    throw RuntimeException();
}