#ifndef CLASREADER_H
#define CLASREADER_H

#include <string>
#include "CWellLog.h"

class CLASReader {
public:
    bool LoadFromFile(const std::string& FilePath, CWellLog& WellLog);
};

#endif // CLASREADER_H
