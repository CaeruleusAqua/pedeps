/*****************************************************************************
Copyright (C)  2019  Brecht Sanders  All Rights Reserved
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*****************************************************************************/

#include "pestructs.h"
#include "pedeps.h"
#include "pedeps_version.h"

#include <string.h>
#include <inttypes.h>
#include <set>
#include <string>
#include <iostream>
#include <cstddef>
#include <regex>


#ifdef _WIN32
#define PATHSEPARATOR '\\'
#define ISPATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#define PATHLISTSEPARATOR ';'
#else
#define PATHSEPARATOR '/'
#define ISPATHSEPARATOR(c) ((c) == '/')
#define PATHLISTSEPARATOR ':'
#endif

#if __cplusplus == 201402L

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
#elif  __cplusplus >= 201402L
#include <filesystem>
namespace fs = std::filesystem;
#endif

static std::vector<std::string> explode(const std::string &s, const char &c) {
    std::string buff;
    std::vector<std::string> v;

    for (auto n:s) {
        if (n != c) buff += n;
        else if (n == c && !buff.empty()) {
            v.push_back(buff);
            buff = "";
        }
    }
    if (buff != "")
        v.push_back(buff);

    return v;
}

int file_exists(const std::string &path) {
    if (fs::exists(path) && fs::is_regular_file(path))
        return 1;
    return 0;
}

int folder_exists(const std::string &path) {
    if (fs::exists(path) && fs::is_directory(path))
        return 1;
    return 0;
}

std::string get_filename_from_path(const std::string &path) {
    return fs::path(path).filename().string();
}

struct dependancy_info_struct {
    int recursive;
    int overwrite;
    int dryrun;
    std::set<std::string> filelist;
    std::vector<std::string> pathlist;
    std::vector<std::string> unresolved_modules;
    std::set<std::string> processed_modules;
};

std::string get_base_path(const std::string &path) {
    return fs::path(path).parent_path().string();
}

//returns non-zero path1 is under path2
int is_in_path(const std::string &path1, const std::string &path2) {
    std::string full_path1 = fs::absolute(path1).string();
    std::string full_path2 = fs::absolute(path2).string();

    size_t len1 = full_path1.length();
    size_t len2 = full_path2.length();
    if (len1 >= len2) {
        if (full_path1.substr(0, len2) == full_path2) {
            if (ISPATHSEPARATOR(full_path1[len2]))
                return 1;
        }
    }
    return 0;
}

inline bool isSystemPath(const std::string &path) {
    static std::string windir = getenv("windir");
    return is_in_path(path, windir);
}

std::string search_path(const std::vector<std::string> &pathlist, const std::string &filename) {
    if (file_exists(filename)) {
        return filename;
    }
    //check search path
    for (auto &path : pathlist) {
        std::string filepath = path;
        filepath += PATHSEPARATOR;
        filepath += filename;
        if (file_exists(filepath))
            return filepath;
    }
    return "";
}

int add_dependancies(struct dependancy_info_struct *depinfo, const std::string &filename);

int iterate_dependancies_add(const char *modulename, const char *functionname, void *callbackdata) {
    auto *depinfo = (dependancy_info_struct *) callbackdata;
    if (modulename && depinfo->processed_modules.count(modulename) == 0) {
        std::cout<<modulename<<std::endl;
        depinfo->processed_modules.insert(modulename);
        std::string path;
        if ((path = search_path(depinfo->pathlist, modulename)).length() > 0) {
            //new module, recursively add dependancies if wanted
            if (depinfo->filelist.count(path) == 0) {
                depinfo->filelist.insert(path);
                if (depinfo->recursive) {
                    add_dependancies(depinfo, path);
                }
            }
        } else {
            depinfo->unresolved_modules.push_back(modulename);
            std::cerr<<modulename<<std::endl;
        }
    }
    return 0;
}

int add_dependancies(dependancy_info_struct *depinfo, const std::string &filename) {
    pefile_handle pehandle;
    std::string path;
    //determine path
    if ((path = search_path(depinfo->pathlist, filename)).length() == 0) {
        std::cerr << "Error: unable to locate " << filename << " in PATH" << std::endl;
        return 1;
    }
    //create PE object
    if ((pehandle = pefile_create()) == nullptr) {
        fprintf(stderr, "Error creating PE handle\n");
        return 2;
    }
    //open PE file
    if (pefile_open_file(pehandle, path.c_str()) == 0) {
        //check all dependancies
        pefile_list_imports(pehandle, iterate_dependancies_add, depinfo);
        //close PE file
        pefile_close(pehandle);
    }
    //destroy PE object
    pefile_destroy(pehandle);
    return 0;
}

void replaceAll(std::string &str, const std::string &from, const std::string &to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}


int main(int argc, char *argv[]) {
    struct dependancy_info_struct depinfo;

    if (argc != 4)
        return 0;
    std::string target = argv[1];
    std::string search_path = argv[2];
    std::string destination_path = argv[3];

    std::vector<std::string> filesToProcess;

    if (file_exists(target)) {
        filesToProcess.push_back(target);
    } else if (folder_exists(target)) {
        for(auto& p: fs::recursive_directory_iterator(target)){
            std::string path = p.path().string();
            if(file_exists(path)) {
                pefile_handle pehandle;
                //create PE object
                if ((pehandle = pefile_create()) == nullptr) {
                    fprintf(stderr, "Error creating PE handle\n");
                    return 2;
                }
                // check is pe file
                if (pefile_open_file(pehandle, path.c_str()) == 0) {
                    filesToProcess.push_back(path);
                    depinfo.pathlist.push_back(fs::path(path).filename().string());
                    pefile_close(pehandle);
                } else {
                }
                pefile_destroy(pehandle);
            }
        }
    } else {
        std::cerr << "Target \"" << target << "\" does not exist!" << std::endl;
        return -1;
    }

    //initialize
    depinfo.recursive = 1;
    depinfo.overwrite = 1;
    depinfo.pathlist.push_back(search_path);


    for(const auto &file : filesToProcess){
        add_dependancies(&depinfo, file);
    }
    std::error_code ec;
    destination_path = "C:\\Users\\scholle\\Projects\\tmp";
    replaceAll(destination_path, "/", "\\");

    fs::create_directories(destination_path + "\\",ec);
    std::cout<<ec.message()<<std::endl;
    for (auto path : depinfo.filelist) {
        replaceAll(path, "/", "\\");
        std::cout << "Copy file:" << path << std::endl;

        fs::copy(path, destination_path + "/" + fs::path(path).filename().string(),ec);
        std::cout<<ec.message()<<std::endl;
    }

//    std::cout << "Unresolved Modules:" << std::endl;
//
//    auto systemPathList = explode(getenv("PATH"),PATHLISTSEPARATOR);
//
//    for (auto &module : depinfo.unresolved_modules) {
//        auto path = search_path(systemPathList, module);
//        //std::cout << path << std::endl;
//        if(!isSystemPath(path))
//            std::cout << module << std::endl;
//    }

    return 0;
}
