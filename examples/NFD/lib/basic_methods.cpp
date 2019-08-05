/**********************************************************************************
                                               NFD project
   A C++ based NF developing framework designed by Wenfei's group
   from IIIS, Tsinghua University, China.
******************************************************************************/

/************************************************************************************
* Filename:   basic_methods.cpp
* Author:     Hongyi Huang(hhy17 AT mails.tsinghua.edu.cn), Bangwen Deng, Wenfei Wu
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    This file is a supprot file for NFD project, containing the methods maybe
              used in NFD NF.
*************************************************************************************/

#include "basic_classes.h"

/* function for spliting string by seperator*/
std::vector<std::string>
split(const std::string &text, char sep) {
        std::vector<std::string> tokens;
        std::size_t start = 0, end = 0;
        while ((end = text.find(sep, start)) != std::string::npos) {
                tokens.push_back(text.substr(start, end - start));
                start = end + 1;
        }
        tokens.push_back(text.substr(start));
        return tokens;
}
