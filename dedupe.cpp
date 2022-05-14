/*
    TODO
    Better map
    HTML with thumbnails?
    Sanity check?

*/


#include <iostream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>

#include <md5.h>
#include <sha2.h>

#include <set>
#include <sstream>

//#define NOCHANGE
//#define DEBUGPRINT

using namespace std;

int rc; //return code
string cmd;
bool reportDuplicates;

struct fileInfo {
    uintmax_t bytes;
    char *md5Sum;
    char *sha256Sum;
    char *fileName;

    fileInfo() { 
        md5Sum = nullptr;
        sha256Sum = nullptr;
        fileName = nullptr;
    }

    ~fileInfo() {
        if (md5Sum != nullptr) {
            free(md5Sum);
        }

        if (sha256Sum != nullptr) {
            free(sha256Sum);
        }

        if (fileName != nullptr) {
            free(fileName);
        }
    }
};

bool operator==(const fileInfo &f1, const fileInfo &f2) {
    return (f1.bytes == f2.bytes) && (strcmp(f1.md5Sum, f2.md5Sum) == 0) && (strcmp(f1.sha256Sum, f2.sha256Sum) == 0);
}

class fileInfoPointerCompare {
  public:

    bool operator()(const fileInfo *f1, const fileInfo *f2) const {
        if (f1->bytes < f2->bytes) {
            return true;
        }
        if (f1->bytes > f2->bytes) {
            return false;
        }


        int c1 = strcmp(f1->md5Sum, f2->md5Sum);
        if (c1 < 0) {
            return true;
        }
        if (c1 > 0) {
            return false;
        }

        int c2 = strcmp(f1->sha256Sum, f2->sha256Sum);
        if (c2 < 0) {
            return true;
        }
        if (c2 > 0) {
            return false;
        }

        return false;
    }

};


ostream &operator<<(ostream &os, const fileInfo &info) {
    os << "{" << info.bytes << " " << info.md5Sum << " " << info.sha256Sum << "}";
    return os;
}


// inPath, filePath
filesystem::path cutCommon(const filesystem::path &f1, const filesystem::path &f2) {
    auto it1 = f1.begin();
    auto it2 = f2.begin();

    while (it1 != f1.end() && it2 != f2.end()) {
        if (*it1 != *it2) {
            break;
        }
        it1++;
        it2++;
    }


    filesystem::path newPath("");
    while (it2 != f2.end()) {
        newPath = newPath / *it2;
        it2++;
    }

    return newPath;
}

int main(int argc, char **argv) {
    uintmax_t bytesProcessed = 0;
    reportDuplicates = false;

    set<fileInfo *, fileInfoPointerCompare> seenFiles;

    vector<string> inPaths;
    filesystem::path outputPath;

    int parseState = 0; //0 -- collect input paths; 1 -- find output path
    for (int i = 1; i < argc; i++) {
        string arg(argv[i]);

        if (arg == "-d") {
            reportDuplicates = true;
            continue;
        }

        if (arg == "-o") {
            parseState = 1;
            continue;
        }

        if (parseState == 0) {
            inPaths.push_back(arg);
        } else if (parseState == 1) {
            outputPath = filesystem::path(arg);
        }

    }

    if (parseState == 0 || outputPath.string() == "" || inPaths.size() == 0) {
        cout << "Usage:" << endl;
        cout << argv[0] << " [input file paths] -o <output file path>" << endl;
        return 0;
    }


    ofstream duplicateReport;

    if (reportDuplicates) {
        duplicateReport.open("duplicates.txt");
    }

    int nextNumber = -1;
    for (string inPath : inPaths) {
        nextNumber++;
        filesystem::path outBase(to_string(nextNumber));

        //iterate over all files
        filesystem::path basePath(inPath);

        for (auto f : filesystem::recursive_directory_iterator(inPath)) {
            {
                cout << "DONE: ";
                double bytes = double(bytesProcessed);

                if (bytes < 1000.0) {
                    cout << bytes << " B" << endl;
                } else if (bytes < 1000.0 * 1000.0) {
                    cout << bytes / 1000.0 << " KB" << endl;
                } else if (bytes < 1000.0 * 1000.0 * 1000.0) {
                    cout << bytes / (1000.0 * 1000.0) << " MB" << endl;
                } else {
                    cout << bytes / (1000.0 * 1000.0 * 1000.0) << " GB" << endl;
                }
            }

            filesystem::path filePath = f.path();
            cout << filePath << endl;


            //handle directories differently (allow empty directories)
            if (f.is_directory()) {
                //filesystem::path path2 = cutCommon(outputPath, filePath);
                filesystem::path path2 = outputPath / outBase / cutCommon(filesystem::path(inPath), filePath);

                cmd = (string("mkdir -p ") + string("\"") + path2.string() + string("\""));

                cout << cmd << endl;

                #if defined(NOCHANGE)
                #else
                rc = system(cmd.c_str());

                if (rc != 0) {
                    cerr << "Return code not 0" << endl;
                    while (1) {
                    }
                }

                #endif

                continue;
            }

            //compute file information
            #if defined(DEBUGPRINT)
            cout << "Computing file information..." << endl;
            #endif

            fileInfo *fi = new fileInfo();

            fi->bytes = filesystem::file_size(filePath);

            {
                size_t bufferSize = strlen(filePath.string().c_str()) + 1;
                fi->fileName = (char *) malloc(bufferSize);

                fi->fileName[bufferSize - 1] = 0;
                memcpy(fi->fileName, filePath.string().c_str(), bufferSize - 1);
            }

            #if defined(DEBUGPRINT)
            cout << "Got bytes: " << fi->bytes << endl;
            #endif

            fi->md5Sum = MD5File(filePath.string().c_str(), nullptr);

            #if defined(DEBUGPRINT)
            cout << "Got md5: " << fi->md5Sum << endl;
            #endif

            fi->sha256Sum = SHA256File(filePath.string().c_str(), nullptr);

            #if defined(DEBUGPRINT)
            cout << "Got sha: " << fi->sha256Sum << endl;

            cout << *fi << endl;
            #endif

            bytesProcessed += fi->bytes;

            //check table for file information, copy to new directory
            auto found = seenFiles.find(fi);
            if (found != seenFiles.end()) {
                delete fi;
                #if defined(DEBUGPRINT)
                cout << "File already exists" << endl;
                #endif
                cout << "File already exists" << endl;
                duplicateReport << "\"" << filePath.string() << "\"" <<  " EXISTS AS " << "\"" << (*found)->fileName << "\"" << endl;

            } else {
                seenFiles.insert(fi);
                #if defined(DEBUGPRINT)
                cout << "Found new file" << endl;
                #endif

                //Make parent directory for this new file
                filesystem::path path2 = outputPath / outBase / cutCommon(filesystem::path(inPath), filePath.parent_path());

                cmd = (string("mkdir -p ") + string("\"") + path2.string() + string("\""));
                cout << cmd << endl;
                #if defined(NOCHANGE)
                #else
                rc = system(cmd.c_str());

                if (rc != 0) {
                    cerr << "Return code not 0" << endl;
                    while (1) {
                    }
                }
                #endif

                //Copy the file
                //filesystem::path path3 = cutCommon(outputPath, filePath);
                filesystem::path path3 = outputPath / outBase / cutCommon(filesystem::path(inPath), filePath);

                cmd = (string("cp -n ") + string("\"") + filePath.string() + string("\"") + string(" ") + string("\"") + path3.string() + string("\""));
                cout << cmd << endl;
                #if defined(NOCHANGE)
                #else
                rc = system(cmd.c_str());

                if (rc != 0) {
                    cerr << "Return code not 0" << endl;
                    while (1) {
                    }
                }
                #endif

            }

            cout << endl;
        }



    }

    if (reportDuplicates) {
        duplicateReport.close();
    }

    cout << "Done! Cleaning up set..." << endl;
    for (auto it = seenFiles.begin(); it != seenFiles.end(); it++) {
        delete *it;
    }

    return 0;
}
