#include "typeChecker.hpp"

TypeChecker::TypeChecker(const ExpPtr & rootExpression)
: rootExpression(rootExpression) { }

void
TypeChecker::check() {
    HEADER("Type checking/inference");
    Environment environment = std::make_shared<EnvironmentRaw>();
    auto temp = std::make_shared<Temp>(rootExpression->token, std::make_shared<Types::UnknownType>());
    eval(rootExpression, environment, temp->returnType);
    HEADER("Type checking/inference Done");

    HEADER("Typed AST");
    if (Logger::getInstance().getLevel() == DEBUG) {
        PrettyPrint printer;
        printer.print(rootExpression);
    }
}

ExpPtr
TypeChecker::eval(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    if (expression->expType == ExpressionTypes::PROG)
        return evalProgram(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::LIT)
        return evalLiteral(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::PRIM)
        return evalPrimitive(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::LET)
        return evalLet(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::REF)
        return evalReference(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::BRANCH)
        return evalBranch(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::TYPECLASS)
        return evalTypeclass(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::APP)
        return evalApplication(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::LIST_DEF)
        return evalListDefinition(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::TUPLE_DEF)
        return evalTupleDefinition(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::MATCH)
        return evalMatch(expression, environment, expectedType);
    else if (expression->expType == ExpressionTypes::END)
        return expression;

    printError(expression->token, std::string("Unknown expression type: ") + expression->token.text);

    return expression;
}

ExpPtr
TypeChecker::evalProgram(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto program = std::static_pointer_cast<Program>(expression);

    for (auto & function : program->functions) {
        addName(environment, function->name, function->returnType);
    }

    for (auto & function : program->functions) {
        if (BuiltinDefinitions::isBuiltin(function->name)) {
            function->isBuiltin = true;
            function->builtinEnum = BuiltinDefinitions::getBuiltin(function->name);
            std::static_pointer_cast<Types::FuncType>(function->returnType)->isBuiltin = true;
        }

        Environment functionInnerEnvironment;
        if (environment && !function->isBuiltin) {
            functionInnerEnvironment = std::make_shared<EnvironmentRaw>(*environment);
            functionInnerEnvironment->erase(function->name);
        } else {
            functionInnerEnvironment = std::make_shared<EnvironmentRaw>();
        }

        for (auto & genericParameter : function->genericParameters) {
            if (functionInnerEnvironment->find(genericParameter->identifier) == functionInnerEnvironment->end()) {
                addName(functionInnerEnvironment, genericParameter->identifier, genericParameter);
            }
        }

        for (auto & parameter : function->parameters) {
            if (parameter->returnType->dataType == Types::DataTypes::GEN) {
                auto genericType = std::static_pointer_cast<Types::GenType>(parameter->returnType);
                auto realType = functionInnerEnvironment->find(genericType->identifier)->second;
                addName(functionInnerEnvironment, parameter->name, realType);
            } else {
                addName(functionInnerEnvironment, parameter->name, parameter->returnType);
            }
        }

        std::static_pointer_cast<Types::FuncType>(function->returnType)->functionInnerEnvironment = functionInnerEnvironment;
    }

    return eval(program->body, environment, expectedType);
}

ExpPtr
TypeChecker::evalLiteral(ExpPtr expression, const Environment & environment, Types::TypePtr & expectedType) {
    auto literal = std::static_pointer_cast<Literal>(expression);
    
    if (!compare(literal->returnType, expectedType)) {
        printMismatchError(literal->token, literal->returnType, expectedType);
    }
    
    return literal;
}

ExpPtr
TypeChecker::evalPrimitive(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto primitive = std::static_pointer_cast<Primitive>(expression);

    // unary op:
    //  - NOT for bool only
    //  - PLUS/MINUS for int only
    // 
    // binary op:
    //  - AND/OR for bool only
    //  - ARITH for int only
    //  - COMP for primitives only

    if (Operator::isUnaryOperator(primitive->op)) {
        if (primitive->op == Operator::OperatorTypes::NOT) {
            auto temp = std::make_shared<Temp>(primitive->token, std::make_shared<Types::BoolType>());
            eval(primitive->rightSide, environment, temp->returnType);
            primitive->returnType = std::make_shared<Types::BoolType>();
        } else if (primitive->op == Operator::OperatorTypes::PLUS ||
                   primitive->op == Operator::OperatorTypes::MINUS) {
            auto temp = std::make_shared<Temp>(primitive->token, std::make_shared<Types::IntType>());
            eval(primitive->rightSide, environment, temp->returnType);
            primitive->returnType = std::make_shared<Types::IntType>();
        }
    } else if (Operator::isBinaryBooleanOperator(primitive->op) ||
               Operator::isArithmeticOperator(primitive->op)) {
        if (primitive->op == Operator::OperatorTypes::AND ||
            primitive->op == Operator::OperatorTypes::OR) {
            auto temp = std::make_shared<Temp>(primitive->token, std::make_shared<Types::BoolType>());
            eval(primitive->leftSide, environment, temp->returnType);
            eval(primitive->rightSide, environment, temp->returnType);
            primitive->returnType = std::make_shared<Types::BoolType>();
        } else if (Operator::isArithmeticOperator(primitive->op)) {
            auto temp = std::make_shared<Temp>(primitive->token, std::make_shared<Types::IntType>());
            eval(primitive->leftSide, environment, temp->returnType);
            eval(primitive->rightSide, environment, temp->returnType);
            primitive->returnType = std::make_shared<Types::IntType>();
        } else { // is comparison operator
            auto temp = std::make_shared<Temp>(primitive->token, std::make_shared<Types::UnknownType>());
            eval(primitive->leftSide, environment, temp->returnType);

            if (!Types::isPrimitiveType(primitive->leftSide->returnType)) {
                printError(primitive->token, "Binary operators can only be used on primitive types");
            }

            eval(primitive->rightSide, environment, temp->returnType);
            primitive->returnType = std::make_shared<Types::BoolType>();
        }
    }

    return primitive;
}

ExpPtr
TypeChecker::evalLet(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto let = std::static_pointer_cast<Let>(expression);
    
    eval(let->value, environment, let->valueType);

    Environment afterLetEnvironment;
    if (environment) {
        afterLetEnvironment = std::make_shared<EnvironmentRaw>(*environment);        
    } else {
        printError(let->token, "Error: Bad environment");
        return let->afterLet;
    }

    addName(afterLetEnvironment, let->ident, let->valueType);

    return eval(let->afterLet, afterLetEnvironment, expectedType);
}

ExpPtr
TypeChecker::evalReference(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto reference = std::static_pointer_cast<Reference>(expression);

    auto referenceType = getName(reference->token, environment, reference->ident);
    reference->returnType = referenceType;

    if (referenceType->dataType == Types::DataTypes::TUPLE && !reference->fieldIdent.empty()) {
        auto tupleType = std::static_pointer_cast<Types::TupleType>(referenceType);

        int tupleIndex = -1;
        try {
            tupleIndex = std::stoi(reference->fieldIdent);
        } catch (...) {
            printError(reference->token, std::string("Error: Tuple requires valid index: ") + reference->fieldIdent);
            return reference;
        }
        
        Types::TypePtr tupleElementType;
        try {
            tupleElementType = tupleType->tupleTypes.at(tupleIndex);
        } catch (...) {
            printError(reference->token, std::string("Error: Index not in range of tuple: ") + std::to_string(tupleIndex));
            return reference;
        }

        if (!compare(tupleElementType, expectedType)) {
            printMismatchError(reference->token, tupleElementType, expectedType);
        }   
        
        reference->returnType = tupleElementType;
    } else if (referenceType->dataType == Types::DataTypes::TYPECLASS && !reference->fieldIdent.empty()) {
        auto typeclassIdent = std::static_pointer_cast<Types::TypeclassType>(referenceType)->ident;
        auto typeclassType = std::static_pointer_cast<Types::TypeclassType>(getName(reference->token, environment, typeclassIdent));

        auto fieldIdent = reference->fieldIdent;
        auto fieldTypeIterator = std::find_if(typeclassType->fieldTypes.begin(), typeclassType->fieldTypes.end(),
                                            [&fieldIdent](const std::pair<std::string, Types::TypePtr> & fieldType) {
                                                return (fieldIdent == fieldType.first);
                                            });

        Types::TypePtr fieldType = (fieldTypeIterator != typeclassType->fieldTypes.end()) ? (*fieldTypeIterator).second : std::make_shared<Types::UnknownType>();

        if (fieldType->dataType == Types::DataTypes::UNKNOWN) {
            printError(reference->token, std::string("Error: typeclass ") +
                       typeclassType->ident + std::string(" has no field ") +
                       reference->fieldIdent);
            return reference;
        }
        
        if (!compare(fieldType, expectedType)) {
            printMismatchError(reference->token, fieldType, expectedType);
        }   
        
        reference->returnType = fieldType;
    } else if (!reference->fieldIdent.empty()) {
        printError(reference->token, "Field given for non-typeclass or tuple type");
    }

    auto resolvedReturnType = reference->returnType;
    resolveType(resolvedReturnType, environment);

    auto resolvedExpectedType = expectedType;
    resolveType(resolvedExpectedType, environment);

    if (!compare(resolvedReturnType, resolvedExpectedType)) {
        printMismatchError(reference->token, referenceType, expectedType);
    }

    return reference;
}

ExpPtr
TypeChecker::evalBranch(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto branch = std::static_pointer_cast<Branch>(expression);
    
    auto temp = std::make_shared<Temp>(branch->token, std::make_shared<Types::BoolType>());
    eval(branch->condition, environment, temp->returnType);

    Types::TypePtr elseType = eval(branch->elseBranch, environment, expectedType)->returnType;
    return eval(branch->ifBranch, environment, elseType);
}

ExpPtr
TypeChecker::evalTypeclass(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto typeclass = std::static_pointer_cast<Typeclass>(expression);
    
    if (!compare(typeclass->returnType, expectedType)) {
        printMismatchError(typeclass->token, typeclass->returnType, expectedType);
        return typeclass;
    }

    addName(environment, typeclass->ident, typeclass->returnType);

    return typeclass;       
}

ExpPtr
TypeChecker::evalApplication(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto application = std::static_pointer_cast<Application>(expression);

    auto temp = std::make_shared<Temp>(rootExpression->token, std::make_shared<Types::UnknownType>());
    auto ident = eval(application->ident, environment, temp->returnType);

    if (ident->returnType->dataType == Types::DataTypes::FUNC) {
        auto functionType = std::static_pointer_cast<Types::FuncType>(ident->returnType);
        
        if (ident->expType == Expressions::ExpressionTypes::APP) {
            auto identGenerics = std::static_pointer_cast<Application>(ident)->genericReplacementTypes;
            application->genericReplacementTypes.insert(application->genericReplacementTypes.end(), identGenerics.begin(), identGenerics.end());
        }
        
        if (application->arguments.size() != functionType->argumentTypes.size()) {
            printError(application->token, "Function application does not match signature");
        }

        if (functionType->genericTypes.empty() && !(application->genericReplacementTypes.empty())) {
            printError(application->token, "Types provided for non-templated function");
        }

        if (!(functionType->genericTypes.empty()) && application->genericReplacementTypes.empty()) {
            printError(application->token, "No types provided for templated function");
        }

        Environment functionInnerEnvironment;
        if (!functionType->functionInnerEnvironment) {
            functionType->functionInnerEnvironment = std::make_shared<EnvironmentRaw>();
        }
        functionInnerEnvironment = std::make_shared<EnvironmentRaw>(*(functionType->functionInnerEnvironment));

        for (unsigned int genericIndex = 0; genericIndex < application->genericReplacementTypes.size(); ++genericIndex) {
            addName(functionInnerEnvironment, functionType->genericTypes.at(genericIndex)->identifier, application->genericReplacementTypes.at(genericIndex));
        }

        for (unsigned int argumentIndex = 0; argumentIndex < application->arguments.size(); ++argumentIndex) {
            auto argumentType = copyArgumentType(functionType->argumentTypes.at(argumentIndex));
            auto argument = application->arguments.at(argumentIndex);

            resolveType(argumentType, functionInnerEnvironment);
            eval(argument, environment, argumentType);

            if (!(functionType->argumentNames.empty())) {
                addName(functionInnerEnvironment, functionType->argumentNames.at(argumentIndex), argumentType);
            }
        }

        auto resolvedReturnType = copyArgumentType(functionType->returnType);
        resolveType(resolvedReturnType, functionInnerEnvironment);

        if (application->returnType->resolved == false &&
            !functionType->isBuiltin && 
            !(functionType->genericTypes.empty()) && 
            functionType->functionBody != nullptr) {
            eval(functionType->functionBody, functionInnerEnvironment, resolvedReturnType);
        }

        if (!compare(resolvedReturnType, expectedType)) {
            printMismatchError(application->token, functionType->returnType, expectedType);
        }

        application->returnType = resolvedReturnType;
        application->returnType->resolved = true;

        return application;
    } else if (ident->returnType->dataType == Types::DataTypes::TYPECLASS) {
        auto typeclassType = std::static_pointer_cast<Types::TypeclassType>(ident->returnType);

        if (expectedType->dataType != Types::DataTypes::TYPECLASS) {
            printMismatchError(application->token, typeclassType, expectedType);
        }

        if (typeclassType->ident != std::static_pointer_cast<Types::TypeclassType>(expectedType)->ident) {
            printMismatchError(application->token, typeclassType, expectedType);
        }
        
        if (application->arguments.size() != typeclassType->fieldTypes.size()) {
            printError(application->token, "Typeclass construction does not match signature");
        }

        for (unsigned int argumentIndex = 0; argumentIndex < application->arguments.size(); ++argumentIndex) {
            eval(application->arguments.at(argumentIndex), environment, typeclassType->fieldTypes.at(argumentIndex).second);
        }

        application->returnType = typeclassType;
        return application;
    } else if (ident->returnType->dataType == Types::DataTypes::LIST) {
        auto listType = std::static_pointer_cast<Types::ListType>(ident->returnType);

        if (application->arguments.empty()) {
            printError(application->token, "List access needs integer argument");
            return application;
        }

        auto listTemp = std::make_shared<Temp>(application->token, std::make_shared<Types::IntType>());
        eval(application->arguments.at(0), environment, listTemp->returnType);
        
        listTemp->returnType = std::make_shared<Types::ListType>(expectedType);
        eval(ident, environment, listTemp->returnType);

        application->returnType = listType;
        return application;
    }

    printError(application->token, "Bad function or typeclass application");
    return application;
}

ExpPtr
TypeChecker::evalListDefinition(ExpPtr expression, const Environment & environment, Types::TypePtr & expectedType) {
    auto listDefinition = std::static_pointer_cast<ListDefinition>(expression);
    
    Types::TypePtr expType;
    if (expectedType->dataType == Types::DataTypes::LIST) {
        expType = std::static_pointer_cast<Types::ListType>(expectedType)->listType;
    } else {
        expType = expectedType;
    }
    
    for (auto & listElementExpression : listDefinition->values) {
        if (!compare(listElementExpression->returnType, expType)) {
            printMismatchError(listDefinition->token, listElementExpression->returnType, expType);
        }
    }

    if (!compare(listDefinition->returnType, expectedType)) {
        printMismatchError(listDefinition->token, listDefinition->returnType, expectedType);
    }

    return listDefinition;
}

ExpPtr
TypeChecker::evalTupleDefinition(ExpPtr expression, const Environment & environment, Types::TypePtr & expectedType) {
    auto tupleDefinition = std::static_pointer_cast<TupleDefinition>(expression);
    auto tupleTypeCopy = copyArgumentType(tupleDefinition->returnType);

    if (!compare(tupleTypeCopy, expectedType)) {
        printMismatchError(tupleDefinition->token, tupleDefinition->returnType, expectedType);
    }

    return tupleDefinition;
}

ExpPtr
TypeChecker::evalMatch(ExpPtr expression, Environment & environment, Types::TypePtr & expectedType) {
    auto match = std::static_pointer_cast<Match>(expression);

    auto caseType = getName(match->token, environment, match->ident);

    bool anyOccurred = false;
    for (auto & casePtr : match->cases) {
        if (anyOccurred) {
            printError(casePtr->token, "Warning: case statement below 'any' is always ignored");
        }

        if (casePtr->ident->expType == ExpressionTypes::REF &&
            std::static_pointer_cast<Reference>(casePtr->ident)->ident == std::string("$any")) {
            anyOccurred = true;
            eval(casePtr->body, environment, expectedType);
        } else {
            eval(casePtr->ident, environment, caseType);
            eval(casePtr->body, environment, expectedType);
        }
    }

    return match;
}

void
TypeChecker::resolveType(Types::TypePtr & returnType, Environment & environment) {
    switch (returnType->dataType) {
        case Types::DataTypes::GEN: {
            std::string identifier = std::static_pointer_cast<Types::GenType>(returnType)->identifier;
            auto envType = environment->find(identifier);

            if (envType == environment->end()) {
                break;
            }

            returnType = envType->second;
        }
            break;
        case Types::DataTypes::LIST: {
            auto listType = std::static_pointer_cast<Types::ListType>(returnType);
            resolveType(listType->listType, environment);
        }
            break;
        case Types::DataTypes::TUPLE: {
            auto tupleType = std::static_pointer_cast<Types::TupleType>(returnType);
            for (auto & tupleElementType : tupleType->tupleTypes) {
                resolveType(tupleElementType, environment);
            }
        }
            break;
        case Types::DataTypes::FUNC: {
            auto funcType = std::static_pointer_cast<Types::FuncType>(returnType);
            for (auto & argumentType : funcType->argumentTypes) {
                resolveType(argumentType, environment);
            }
            resolveType(funcType->returnType, environment);
        }
            break;
        default:
            break;
    } 
}

bool 
TypeChecker::compare(Types::TypePtr & leftType, Types::TypePtr & rightType) {
    if (leftType->dataType == Types::DataTypes::UNKNOWN) {
        leftType = rightType;
        return true;
    } else if (rightType->dataType ==  Types::DataTypes::UNKNOWN) {
        rightType = leftType;
        return true;
    }

    return leftType->compare(rightType);
}

void
TypeChecker::addName(Environment & environment, std::string name, Types::TypePtr type) {
    if (environment->count(name)) {
		environment->erase(name);
	}
	environment->insert(std::pair<std::string, Types::TypePtr>(name, type));
}

Types::TypePtr
TypeChecker::getName(const Token & token, Environment & environment, std::string name) {
    auto type = environment->find(name);
    if (type == environment->end()) {
        printError(token, std::string("Error: ") + name + 
                          std::string(" does not exist in this scope"));
        return std::make_shared<Types::UnknownType>();
    }
    return type->second;
}

Types::TypePtr
TypeChecker::copyArgumentType(Types::TypePtr argumentType) {
    auto typeEnum = argumentType->dataType;
    switch (typeEnum) {
        case Types::DataTypes::INT:
            return std::make_shared<Types::IntType>();
            break;
        case Types::DataTypes::BOOL:
            return std::make_shared<Types::BoolType>();
            break;
        case Types::DataTypes::CHAR:
            return std::make_shared<Types::CharType>();
            break;
        case Types::DataTypes::STRING:
            return std::make_shared<Types::StringType>();
            break;
        case Types::DataTypes::NULLVAL:
            return std::make_shared<Types::NullType>();
            break;
        case Types::DataTypes::LIST:
            return std::make_shared<Types::ListType>(std::static_pointer_cast<Types::ListType>(argumentType)->listType);
            break;
        case Types::DataTypes::TUPLE:
            return std::make_shared<Types::TupleType>(std::static_pointer_cast<Types::TupleType>(argumentType)->tupleTypes);
            break;
        case Types::DataTypes::FUNC: {
            auto funcType = std::static_pointer_cast<Types::FuncType>(argumentType);
            auto newFuncType = std::make_shared<Types::FuncType>(funcType->genericTypes, funcType->argumentTypes, funcType->returnType);
            newFuncType->argumentNames = funcType->argumentNames;
            newFuncType->functionBody = funcType->functionBody;
            newFuncType->functionInnerEnvironment = funcType->functionInnerEnvironment;
            return newFuncType;
        }
            break;
        case Types::DataTypes::TYPECLASS: {
            auto typeclassType = std::static_pointer_cast<Types::TypeclassType>(argumentType);
            return std::make_shared<Types::TypeclassType>(typeclassType->ident, typeclassType->fieldTypes);
        }
            break;
        case Types::DataTypes::GEN:
            return std::make_shared<Types::GenType>(std::static_pointer_cast<Types::GenType>(argumentType)->identifier);
            break;
        default:
            break;
    }
    return std::make_shared<Types::UnknownType>();
}

void
TypeChecker::printMismatchError(const Token & token, const Types::TypePtr & type, const Types::TypePtr & expectedType) {
    error = true;

    std::stringstream errorStream;
    errorStream << "Line: " << token.position.fileLine - BuiltinDefinitions::builtinNumber()
                << ", Column: " << token.position.fileColumn << std::endl
                << "Mismatched type: " << type->toString()
                << ", Expected: " << expectedType->toString()
                << std::endl << token.position.currentLineText << std::endl;
    ERROR(errorStream.str());
}

void
TypeChecker::printError(const Token & token, const std::string & errorMessage) {
    error = true;

    std::stringstream errorStream;
    errorStream << "Line: " << token.position.fileLine - BuiltinDefinitions::builtinNumber()
                << ", Column: " << token.position.fileColumn << std::endl
                << errorMessage << std::endl 
                << token.position.currentLineText << std::endl;
    ERROR(errorStream.str());
}