#pragma once

#include <string>

class FilePosition {
    public:
        int fileLine, fileColumn = 0;
        std::string currentLineText = "";
        
        FilePosition(const int line, const int column, const std::string & text);
};