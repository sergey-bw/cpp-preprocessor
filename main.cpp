#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

const regex directive1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
const regex directive2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

// напишите эту функцию
bool Preprocess(ifstream& in, ofstream& out, const path& in_file, const vector<path>& include_directories) {
    string line;
    int line_num = 0;
    smatch m;

    while (getline(in, line)) {
        ++line_num;
        if (regex_match(line, m, directive1)) {
            path include_path = in_file.parent_path() / string(m[1]);
            ifstream include_file(include_path);
            if (include_file) {
                Preprocess(include_file, out, include_path, include_directories);
                continue;
            }
        } else if (!regex_match(line, m, directive2)) {
            out << line << "\n";
            continue;
        }

        //перебираем каталоги
        bool is_file_exist = false;
        for (const path& include_path : include_directories) {
            ifstream include_file(path(include_path / string(m[1])));
            if (include_file) {
                is_file_exist = true;
                Preprocess(include_file, out, include_path, include_directories);
                break;
            }
        }
        
        if (!is_file_exist) {
            cout << "unknown include file " << string(m[1]) << " at file " << in_file.string() << " at line " << line_num << endl;
            return false;
        }
    }
    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input) {
        return false;
    }

    ofstream output(out_file);
    if (!output) {
        return false;
    }

    return Preprocess(input, output, in_file, include_directories);
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}