#include <iostream>
#include <string>

int main() {
int a = 10;
int b = 20;
int total = a + b;
std::cout << total << '\n';
std::string message = "Hello, Hydrogen!";
std::cout << message << '\n';
if (total > 20) {
    std::cout << "Total is greater than 20" << '\n';
} else {
    std::cout << "Total is not greater than 20" << '\n';
}
int i = 0;
while (i < 3) {
    std::cout << i << '\n';
    i = i + 1;
}
    return 0;
}
