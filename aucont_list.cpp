#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char *argv[]) {
    fstream fin("containers");

    if (!fin.is_open()) {
        exit(0);
    }

    string line;
    while (getline(fin, line)) {
        cout << line << endl;
    }
}