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

pair<bool, path> FileSearch(const path& file, const path& parent_dir, bool& parent_dir_for_search, 
                            const vector<path>& include_directories) {
    path found_file_path;

    if (parent_dir_for_search) {
        found_file_path = parent_dir / file;
        if (filesystem::exists(found_file_path)) {
            return { true, found_file_path };
        }
    }
    for (auto& include_directory : include_directories) {
        found_file_path = include_directory / file;
        if (filesystem::exists(found_file_path)) {
            return { true, found_file_path };
        }
    }
    return { false, file };
}

bool PreprocessInner(ifstream& input_file, const path& in_file, ofstream& output_file, const vector<path>& include_directories) {
    static regex include_quotes(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_brackets(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch m;

    if (input_file.is_open()) {

        string line;
        int line_number = 0;

        while (getline(input_file, line)) {
            ++line_number;
            
            path include_file_name;
            bool line_with_include = false;
            bool parent_dir_for_search = false;
            if (regex_match(line, m, include_brackets)) {
                include_file_name = string(m[1]);
                line_with_include = true;
            }
            else if (regex_match(line, m, include_quotes)){
                include_file_name = string(m[1]);
                parent_dir_for_search = true;
                line_with_include = true;
            }
            
            if (line_with_include) {
                auto [include_file_was_found, found_include_file_name_path] = FileSearch(include_file_name,
                    in_file.parent_path(), parent_dir_for_search, include_directories);
                if (!include_file_was_found) {
                    cout << "unknown include file "s << found_include_file_name_path.string() << " at file ";
                    cout << in_file.string() << " at line " << line_number << endl;
                    return false;
                }

                ifstream new_input_file(found_include_file_name_path);
                if (!PreprocessInner(new_input_file, found_include_file_name_path, output_file, include_directories)) {
                    return false;
                }
            }
            else {
                output_file << line << endl;
            }
        }
    }
    else {
        return false;
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    if (ifstream input_file(in_file); input_file.is_open()) {
        ofstream output_file;
        output_file.open(out_file);

        bool preprocess_sucsess = PreprocessInner(input_file, in_file, output_file, include_directories);

        output_file.close();
        return preprocess_sucsess;
    }
    else {
        return false;
    }
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
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
        {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

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