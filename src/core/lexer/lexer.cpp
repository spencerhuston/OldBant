#include "lexer.hpp"

const std::string
Lexer::CHAR_DELIMS{"[](){}=->:;,*/+<!'\".&|%"};

const std::set<std::string>
Lexer::KEYWORDS{
	"if", "else",
	"func",
    "typeclass", "type",
	"val", "List", "Tuple",
	"true", "false", 
    "int", "bool", "char", "null", "string",
	"case", "match", "any",
    "import", ".."
};

const std::set<std::string> 
Lexer::DELIMITERS{
	"[", "]", "(", ")", "{", "}",
	"=", "->", ":", ";", ",", "'", "\"", ".",
	"+", "-", "/", "*", "%",
	"<", ">", "!", "&&", "||",
	"==", "!=", "<=", ">="
};

const std::regex
Lexer::IDENT_REGEX("^[a-zA-z_][a-zA-Z0-9_]*$");

bool Lexer::inQuotes = false;

std::string
Lexer::readFile(const std::string & sourceFileName) {
    const std::string extension(".bnt");
    if (sourceFileName.size() <= extension.size() ||
        sourceFileName.compare(sourceFileName.size() - extension.size(), extension.size(), extension) != 0) {
        ERROR(std::string("Error: Files require .bnt extension: ") + sourceFileName);
        return std::string("");
    }

    std::ifstream sourceFile(sourceFileName);
    
    std::string sourceStream;
    if (sourceFile.is_open()) {
        std::ostringstream sourceFileStream;
        sourceFileStream << sourceFile.rdbuf();
        sourceStream = sourceFileStream.str();
    } else
        ERROR(std::string("Error: Could not open file: ") + sourceFileName);

    return sourceStream;
}

Lexer::Lexer(const std::string & sourceStream) 
: sourceStream(sourceStream),
  currentPosition(1, 1, "") {
    HEADER("Source text");
    HEADER(sourceStream);
    HEADER("Lexing Errors");
}

const std::vector<Token>
Lexer::makeTokenStream() {
    for (const auto character : sourceStream)
        lexCharacter(character);

    if (!currentTokenBlock.empty())
        lexCharacter('\n');

    HEADER("Tokens");
    
    std::stringstream tokenStringStream;
    for (const auto & token : tokenStream) {
        tokenStringStream << token.toString() << std::endl;
    }

    HEADER(tokenStringStream.str());

    return tokenStream;
}

void
Lexer::lexCharacter(const char character) {
    if (filterComments(character)) {
        if (filterWhitespace(character)) {
            std::string tokenString;
            for (auto tokenIter = currentTokenBlock.begin(); tokenIter < currentTokenBlock.end(); ++tokenIter) {
                if ((isCharDelimiter(*tokenIter) && !inQuotes) || *tokenIter == '\"' || *tokenIter == '\'') {
                    if (!tokenString.empty()) {
                        tokenStream.push_back(makeToken(tokenString));
                        tokenString = "";
                    }

                    if (isCharDelimiter(*std::next(tokenIter))) {
                        std::string delimString{*tokenIter, *std::next(tokenIter)};
                        if (isDelimiter(delimString)) {
                            tokenStream.push_back(makeToken(delimString));
                        } else {
                            tokenStream.push_back(makeToken(std::string(1, *tokenIter)));
                            tokenStream.push_back(makeToken(std::string(1, *std::next(tokenIter))));
                        }
                        ++tokenIter;
                    } else {
                        tokenStream.push_back(makeToken(std::string(1, *tokenIter)));
                    }
                } else {
                    tokenString += *tokenIter;
                }
            }

            if (!tokenString.empty()) {
                tokenStream.push_back(makeToken(tokenString));
                tokenString = "";
            }
            currentTokenBlock = "";
        }
    }

    if (updateFilePosition) {
        currentPosition.fileLine += 1;
        currentPosition.fileColumn = 1;
        currentPosition.currentLineText = "";

        updateFilePosition = false;
    }
}

bool
Lexer::filterComments(const char character) {
    switch (character) {
        case '#': {
            inComment = (!inQuotes);
            return (inQuotes);
        }
            break;
        case '\n': {
            inComment = false;
            return true;
        }
            break;
        default:
            break;
    }
    if (inComment) 
        return false;
    return true;
}

bool
Lexer::filterWhitespace(const char character) {
    switch (character) {
        case ' ': {
            currentPosition.fileColumn += 1;
            currentPosition.currentLineText += character;

            if (inQuotes)
                currentTokenBlock += character;

            return !inQuotes; // if in quotes, return false
        }
            break;
        case '\t': {
            currentPosition.fileColumn += 8;
            currentPosition.currentLineText += character;

            if (inQuotes)
                currentTokenBlock += character;

            return !inQuotes; // if in quotes, return false
        }
            break;
        case '\r': {
            return false;
        }
            break;
        case '\n': {
            updateFilePosition = true;

            if (inQuotes)
                printError(std::string({character}));
            
            return true;
        }
            break;
        case '\"': 
        case '\'': {
            currentPosition.fileColumn += 1;
            currentPosition.currentLineText += character;
            currentTokenBlock += character;

            if (inQuotes) {
                inQuotes = false;
                return true;
            }

            inQuotes = true;
            return false;
        }
            break;
        default: {
            if (isValidCharacter(character) || inQuotes) {
                currentPosition.fileColumn += 1;
                currentPosition.currentLineText += character;
                currentTokenBlock += character;
                return false;
            } else {
                currentPosition.fileColumn += 1;
                currentPosition.currentLineText += character;
                printError(std::string({character}));
                return false;
            }
        }
    }
}

const Token
Lexer::makeToken(const std::string & tokenString) {
    FilePosition filePosition(currentPosition.fileLine, 
                            currentPosition.fileColumn - tokenString.length(),
                            currentPosition.currentLineText);
    if (isDelimiter(tokenString)) {
        return Token(Token::TokenType::DELIM, filePosition, tokenString);
    }
    else if (isKeyword(tokenString)) {
        return Token(Token::TokenType::KEYWORD, filePosition, tokenString);
    }
    else if (isValue(tokenString)) {
        return Token(Token::TokenType::VAL, filePosition, tokenString);
    }
    else if (isIdentity(tokenString)) {
        return Token(Token::TokenType::IDENT, filePosition, tokenString);
    }
    
    printError(tokenString);
    return Token(Token::TokenType::ERROR, filePosition, tokenString);
}

bool
Lexer::isValidCharacter(const char character) {
    return (isalnum(character) || isCharDelimiter(character) || character == '_' || character == '\\');
}

bool
Lexer::isCharDelimiter(const char character) {
    return (CHAR_DELIMS.find(character) != std::string::npos);
}

bool
Lexer::isDelimiter(const std::string & tokenString) {
    if (tokenString == "'" || tokenString == "\"")
		inQuotes = !inQuotes;
    return (DELIMITERS.find(tokenString) != DELIMITERS.end());
}

bool
Lexer::isKeyword(const std::string & tokenString) {
    return (KEYWORDS.find(tokenString) != KEYWORDS.end());
}

bool
Lexer::isValue(const std::string & tokenString) {
    for (char const & character : tokenString) {
        if (std::isdigit(character) == 0) 
            return false;
    }
    return true;
}

bool
Lexer::isIdentity(const std::string & tokenString) {
    return (std::regex_match(tokenString, IDENT_REGEX) || inQuotes);
}

void
Lexer::printError(const std::string & culprit) {
    error = true;
    std::stringstream errorStream;
    errorStream << "Line: " << currentPosition.fileLine
                << ", Column: " << currentPosition.fileColumn - 1 << std::endl
                << "Unexpected character: " << culprit << std::endl << std::endl
                << currentPosition.currentLineText << std::endl
                << std::string(currentPosition.fileColumn - culprit.length() - 1, ' ') << "^";
    ERROR(errorStream.str());
}