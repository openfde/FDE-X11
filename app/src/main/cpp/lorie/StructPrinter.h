//
// Created by huyang on 2025/10/16.
//

#ifndef FDE_X11_STRUCTPRINTER_H
#define FDE_X11_STRUCTPRINTER_H
#include <iostream>
#include <iomanip>
#include <sstream>
#include "android.h"

class StructPrinter {
    public:
        static std::string toString(const WindProperty& prop);
        static std::string toString(const Widget& prop);
        static std::string toString(const WindAttribute& prop);

    static void to_string(WindAttribute attr);
};


#endif //FDE_X11_STRUCTPRINTER_H
