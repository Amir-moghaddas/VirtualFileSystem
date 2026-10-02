
#include<iostream>
#include<vector>
#include<string>
#include<sstream>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <stdexcept>
using namespace std;

const int SECTOR_SIZE = 64;

class File{
public:
    File(string n , string c):name(n) , content(c){
        size = content.size();
        time_t now= time(0);
        ltm = *localtime(&now);
    }
    const string&  get_name() const {return name;}
    const int get_size() const {return size;}
    const string& get_content() const {return content;}
    vector<int>& get_sectors() {return sectors;}
    const tm& get_ltm() const{return ltm;}
    void set_sectors(const vector<int> s){sectors = s;}
private:

    string name;
    int size;
    string content;
    vector<int>sectors;
    tm ltm;
};

class Directory{
public:
    Directory(string n , Directory* p = nullptr):name(n) , parent(p){}
    ~Directory(){
        for(File* f:files)
            delete f;
        for(Directory*d : subdir)
            delete d;
    }
    const string& get_name() const { return name; }
    Directory* get_parent() const { return parent; }
    vector<Directory*>& get_subdirs() { return subdir; }
    vector<File*>& get_files() { return files; }
    
private:
    string name;
    Directory* parent;
    vector<Directory*>subdir;
    vector<File*>files;
};

class Disk{
public:

    int total_sec;
    Disk(int sector):total_sec(sector){
        allocated.assign(total_sec , false);
    }
    vector<int>allocate(int count){
        vector<int>sec;
        for(int i = 0 ; i<count && sec.size()<count ; i++){
            if(!allocated[i]){
                allocated[i] = true;
                sec.push_back(i);
            }
        }
        if(sec.size()<count){
            for(int s:sec){
                allocated[s] = false;
                throw runtime_error("Not enough space on disk");
            }
        }
        return sec;
    }
    void free(const vector<int>&sec){
        for(int s:sec){
            allocated[s] = false;
        }
    }
    void defrag(vector<File*>& all_file){
        fill(allocated.begin() , allocated.end() , false);
        for(File* f : all_file){
            int need = (f->get_size() + SECTOR_SIZE -1)/ SECTOR_SIZE;
            vector<int>new_sec = allocate(need);
            f->set_sectors(new_sec);

        }
    }
    void status()const{
        cout<<"Disk allocation: ";
        for(bool use : allocated) cout<<(use ? '1' : '0');
        cout<<endl;
    }

    
private:
    vector<bool>allocated;
};

class FileSystem{
public:
    FileSystem(int sector){
        root = new Directory("/");
        current = root;
        current_path = "/";
        disk = new Disk(sector);
    }
    ~FileSystem(){
        delete root;
        delete disk;
    }

    const string get_current_path() const {return current_path;}

    void pwd(){
        if(current_path == "/")
            cout<<"/"<<endl;
        else
            cout<<current_path.substr(0 , current_path.size() -1)<<endl;
    }
    void cd(const string path) {
        string target = path.empty() ? "/" : path;
        string abs_path = normal_path(target);
        Directory* dir = find_directory(abs_path);

        if(dir) {
            current = dir;
            current_path = (abs_path == "/") ? "/" : abs_path + "/";
        }else{
            cout << "bash: cd: " << path << ": No such file or directory" << endl;
        }
    }
    void ls(const string path=""){
        Directory* target =current;
        if(!path.empty()){
            string abs = normal_path(path);
            File* f = find_file(abs);
            if(f){
                char buf[80];
                strftime(buf , sizeof(buf) , "%Y-%m-%d  %H:%M:%S" ,&f->get_ltm() );
                cout << "\033[1;32m" << f->get_name() << "\033[0m\t"
                     << f->get_size() << " bytes\t" << buf << endl;
                return;
            }
            target = find_directory(abs);
            if(!target){
                cout<<"ls: cannot access '" << path << "': No such file or directory" << endl;
                return ;
            }
        }
        vector<Directory*> dirs = target->get_subdirs();
        vector<File*> files = target->get_files();

        sort(dirs.begin() , dirs.end() , [](Directory* a , Directory* b){
            return a->get_name() < b->get_name();
        });
        sort(files.begin() , files.end() , [](File* a , File* b){
            return a->get_name() < b->get_name();
        });

        for(Directory* d : dirs)
            cout<<"\033[1;34m" << d->get_name() << "/\033[0m" << endl;
        for (File* f : files) 
            cout << f->get_name() << endl;
    }

    void mkdir(const string path){
        if(path.empty()){
            cout<<"mkdir: missing operand" << endl;
            return;
        }

        string abs = normal_path(path);
        string  parent_path = get_parent_path(abs);
        string name = abs.substr(abs.find_last_of('/') + 1);

        Directory* parent = find_directory(parent_path);
        if(!parent){
             cout << "mkdir: cannot create directory '" << path << "': No such file or directory" << endl;
            return;
        }
        for(Directory* d :parent->get_subdirs()){
            if(d->get_name() == name){
                cout<<"mkdir: cannot create directory "<<path<< ": File exists"<<endl;
                return;
            }
        }
        Directory* newdir= new Directory(name  , parent);
        parent->get_subdirs().push_back(newdir);
    }
    void rm(string path , bool rec = false){
        string abs = normal_path(path);

   
        File* f = find_file(abs);
        if(f){
            Directory* parent_dir = find_directory(get_parent_path(abs));
            vector<File*>& file = parent_dir->get_files();
            file.erase(remove(file.begin(), file.end(), f), file.end());
            disk->free(f->get_sectors());
            delete f;
            return;
        }
    
        Directory* d= find_directory(abs);
        if(d && d != root){
            if(!rec){
                cout << "rm: cannot remove '" << path << "': Is a directory" << endl;
                return;
            }

            for(File* f : d->get_files()){
                disk->free(f->get_sectors());
                delete f;
            }
            d->get_files().clear();

            for(Directory* sub : d->get_subdirs()){
                rm(normal_path(abs + "/" + sub->get_name()), true);
            }
            d->get_subdirs().clear();

            Directory* parent_dir = d->get_parent();
            vector<Directory*>& dir = parent_dir->get_subdirs();
            dir.erase(remove(dir.begin(), dir.end(), d), dir.end());
            delete d;
            return;
        }

        cout << "rm: cannot remove '" << path << "': No such file or directory" << endl;

    }
    void cp(string src, string des) {
    string abs_src = normal_path(src);
    string abs_des = normal_path(des);

    File* f_src = find_file(abs_src);
    Directory* d_src = find_directory(abs_src);

    File* f_des = find_file(abs_des);
    Directory* d_des = find_directory(abs_des);

    if (f_src && d_des) {
        try{
            int need = (f_src->get_size() + SECTOR_SIZE - 1) / SECTOR_SIZE;
            vector<int> new_secs = disk->allocate(need);
            File* new_file = new File(f_src->get_name(), f_src->get_content());
            new_file->get_sectors() = new_secs;
            d_des->get_files().push_back(new_file);

            cout << "File copied to directory " << des << endl;
        }catch(runtime_error& e) {
            cout << e.what() << endl;
        }
        return;
    }

    if(f_src && !d_des) {
        try {
            int need = (f_src->get_size() + SECTOR_SIZE - 1) / SECTOR_SIZE;
            vector<int> new_secs = disk->allocate(need);
            File* new_file = new File(f_src->get_name(), f_src->get_content());
            new_file->get_sectors() = new_secs;

            Directory* parent_dir = find_directory(get_parent_path(abs_des));
            parent_dir->get_files().push_back(new_file);

            cout << "File copied to file " << des << endl;
        } catch (runtime_error& e) {
            cout << e.what() << endl;
        }
        return;
    }

    if(d_src && d_des) {
        Directory* newdir = new Directory(d_src->get_name());
        d_des->get_subdirs().push_back(newdir);

        for(File* f_dir : d_src->get_files()){
            cp(abs_src + "/" + f_dir->get_name(), abs_des + "/" + d_src->get_name());
        }

        for (Directory* sub_dir : d_src->get_subdirs()) {
            cp(abs_src + "/" + sub_dir->get_name(), abs_des + "/" + d_src->get_name());
        }

        cout << "Directory copied to directory " << des << endl;
        return;
    }

    cout << "cp: cannot stat '" << src << "': No such file or directory" << endl;
    }

    void mv(string src , string des){

        cp(src, des);

        rm(src, true); 
    }
    void put(const string name, const string real_path){
        ifstream in(real_path);
        if(!in){
            cout << "put: cannot open " << real_path << endl;
            return;
        }
        for(File* f : current->get_files()){
            if(f->get_name() == name)
                cout << "put: file '" << name << "' already exists" << endl;
                return;
        }

        string content((istreambuf_iterator<char>(in)), {});
        in.close();

        File* f = new File(name, content);
        current->get_files().push_back(f);

        cout << "put: imported " << real_path << " as " << name << endl;
    }


    void get(const string vir_path){
        File* f = find_file(normal_path(vir_path));
        if (!f) {
            cout << "get: " << vir_path << " not found" << endl;
            return;
        }

        ofstream out(f->get_name());
        if (!out) {
            cout << "get: cannot create " << f->get_name() << endl;
            return;
        }

        out << f->get_content();
        out.close();

        cout << "get: exported " << f->get_name() << endl;
    }
    void defrag() {
        vector<File*> all_files;
        all_file(root, all_files);
        disk->defrag(all_files);
    }

    void status() {
        disk->status();
    }



private:
    Directory* root;
    Directory* current;
    string current_path;
    Disk* disk;
    
    void all_file(Directory* dir , vector<File*>& files){
        for(File* f:dir->get_files())
            files.push_back(f);
        for(Directory* d : dir->get_subdirs())
            all_file(d , files);
    }
    string normal_path(string path){
        if(path.empty() || path =="~") 
            return "/";
        if(path[0] != '/')
            path = current_path + path;
        
        vector<string> parts;
        stringstream ss(path);
        string part;
        while(getline(ss , part , '/')){
            if(part.empty() || path ==".")
                continue;
            if(part == ".."){
                if(!parts.empty())
                    parts.pop_back();
            }else{
                parts.push_back(part);
            }
        }
        string result = "/";
        for(int i = 0 ; i<parts.size() ; i++){
            result+=parts[i];
            if(i<parts.size() - 1)
                result +="/";
        }
        return result;
        
    }
    Directory* find_directory(const string& path){
        string abs_path = normal_path(path);
        if(abs_path == "/")
            return root;

        Directory* dir = root;
        stringstream ss(abs_path.substr(1));
        string part;
        while(getline(ss , part ,'/')){
            if(part.empty())
                continue;
            bool found = false;
            for(Directory* sub :dir->get_subdirs()){
                if(sub->get_name() == part){
                    dir = sub;
                    found = true;
                    break;
                }
            }
            if(!found)
                return nullptr;
        }
        return dir;
    }
    
    File* find_file(const string path){
        string abs_path = normal_path(path);
        int pos = abs_path.find_last_of('/');
        if(pos == string::npos || pos == abs_path.size() -1)
            return nullptr;
        
        string dir_path = abs_path.substr(0,pos);
        string filename = abs_path.substr(pos+1);

        Directory* dir = find_directory(dir_path.empty() ? "/" : dir_path); 
        if(!dir)
            return nullptr;
        for(File* f : dir->get_files()){
            if(f->get_name() == filename)
                return f;
        }
        return nullptr;

    } 
    string get_parent_path(const string path){
        string full_path = normal_path(path);
        if(full_path == "/") return "/";
        int pos = full_path.find_last_of("/");
        if(pos == 0)
            return "/";
        return full_path.substr(0 , pos);
    } 


};

int main(){

int disk_sectors;
    cout << "Enter disk size in sectors (each 64 bytes): ";
    cin >> disk_sectors;
    cin.ignore();

    FileSystem fs(disk_sectors);

    string line;
    while (true) {
        cout << "\033[1;32muser@virtualfs\033[0m:";

        const string& curr_path = fs.get_current_path();
        string display_path = (curr_path == "/")
            ? "\033[1;34m~\033[0m"
            : "\033[1;34m" + curr_path.substr(0, curr_path.length() - 1) + "\033[0m";

        cout << display_path << "\033[0m$ ";

        if (!getline(cin, line)) break;
        if (line.empty()) continue;

        stringstream ss(line);
        string cmd;
        ss >> cmd;

        if (cmd == "exit" || cmd == "quit") break;
        else if (cmd == "pwd") fs.pwd();
        else if (cmd == "cd") {
            string path; getline(ss, path);
            path.erase(0, path.find_first_not_of(" \t"));
            fs.cd(path);
        }
        else if (cmd == "ls") {
            string path; getline(ss, path);
            path.erase(0, path.find_first_not_of(" \t"));
            fs.ls(path);
        }
        else if (cmd == "mkdir") {
            string path; getline(ss, path);
            path.erase(0, path.find_first_not_of(" \t"));
            if (path.empty()) cout << "mkdir: missing operand" << endl;
            else fs.mkdir(path);
        }
        else if (cmd == "rm") {
            string rest; getline(ss, rest);
            rest.erase(0, rest.find_first_not_of(" \t"));
            bool recursive = false;
            if (rest.rfind("-r", 0) == 0) {
                recursive = true;
                rest = rest.substr(2);
                rest.erase(0, rest.find_first_not_of(" \t"));
            }
            fs.rm(rest, recursive);
        }
        else if (cmd == "cp") { string s, d; ss >> s >> d; fs.cp(s, d); }
        else if (cmd == "mv") { string s, d; ss >> s >> d; fs.mv(s, d); }
        else if (cmd == "put") { string v, r; ss >> v >> r; fs.put(v, r); }
        else if (cmd == "get") {
            string p; getline(ss, p);
            p.erase(0, p.find_first_not_of(" \t"));
            fs.get(p);
        }
        else if (cmd == "defrag") fs.defrag();
        else if (cmd == "status") fs.status();
        else cout << "bash: " << cmd << ": command not found" << endl;
    }

    cout << "Goodbye!\n";
    return 0;
}
