// Hydrogen compiler (simpler version)
// It reads a .hyd file and prints the same program written in C++.
//
// How to use:
//   g++ -std=c++17 hydrogen_simple.cpp -o hydrogen
//   ./hydrogen program.hyd > program.cpp
//   g++ program.cpp -o program && ./program
//
// Kinds of tokens in Hydrogen:
//   KEYWORDS      : if, else, while, print
//   TYPE KEYWORDS : int, float, string, bool
//   IDENTIFIERS   : variable names like x, count, my_var
//   LITERALS      : 12 (int), 3.14 (float), "hi" or 'hi' (string),
//                   true / false (bool)
//   OPERATORS     : arithmetic + - * / %
//                   comparison == != <= >= < >
//                   assignment =
//   DELIMITERS    : ( ) for brackets and print, : at the end of if/while/else
//   INDENTATION   : 4 spaces = one level, it works like { } in C++

#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

enum class TokenType{
    inth, stringh, boolh, floath, unknown
};

struct Line{
    int indent;
    int number;
    string text;
};

// used for compile-time errors (lexical, syntax and semantic errors)
//error says that which line is wrong
runtime_error makeError(int lineNo, const string& message){
    return runtime_error("Line "+to_string(lineNo)+": "+message);
}

// removes spaces, tabs and \r from both ends of a string
// whitespaces between tokens is not important, so we remove it
string trim(const string& s){
    size_t start = s.find_first_not_of(" \t\r");
    if (start == string::npos) return "";         // string was all spaces
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end-start+1);
}

// true if s begin with prefix
// used to recognise the KEYWORD token at start of line (if,while,print,etc)
bool startsWith(const string& s, const string& prefix){
    return s.substr(0, prefix.size()) == prefix;
}

// Hydrogen type -> the word we write in c++ file
// maps a hydrogen type to it's C++ type
string typeName(TokenType t){
    if (t==TokenType::inth) return "int";
    if (t==TokenType::floath) return "float";
    if (t==TokenType::stringh) return "std::string";
    if (t==TokenType::boolh) return "bool";
    return "unknown";
}

// word written in hydrogen (inth, floath, stringh, boolh) -> TokenType
// recognises the TokenType KEYWORD token. If the word is not a TokenType keyword, 
// we return unknown (could be an identifier or something else)
TokenType typeFromWord(const string& word){
    if (word == "int") return TokenType::inth;
    if (word == "float") return TokenType::floath;
    if (word == "string") return TokenType::stringh;
    if (word == "bool") return TokenType::boolh;
    return TokenType::unknown;
}

//checks weather can it be a variable name (letters, digits, _ and not starting with a digit)
// checks the rule for an IDENTIFIER token
// IDENTIFIER -> (letter | _) (letter | digit | _)*
// also makes sure that IDENTIFIER is not a reserved keyword 
bool isValidName(const string& name){
    if (name.empty()) return false;

    // (unsigned char) is only there so isalpha/isalnum are safe for every character 
    // first charater must be a letter or underscore, not a digit
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return false;

    //remaining character can be letters, digits or underscore
    for (char c: name) if (!isalnum((unsigned char)c) && c!='_') return false;

    //these words already mean something in hydrogen 
    //RESERVED WORDS (keywords) cannot be usded as IDENTIFIERS 
    if (name=="if" || name=="else" || name=="while" || name== "print") return false;
    return true;
}

// is it a number (like 23 or 3.14) ? (digit with almost one dot)
// recognises a NUMERIC LITERAL token 
// no dot = integer, one dot = float 
bool isNumber(const string& s){
    if (s.empty()) return false;

    int dots=0;
    for (char c : s){
        if (c=='.') dots++;
        else if (!isdigit((unsigned char)c)) return false;
    }
    return (dots<2);
}

// recognises a STRING LITERAL token (starts and ends with same kind of quote)
bool isStringLiteral(const string& s){
    if (s.size()<2) return false;
    char first = s.front();
    char last = s.back();
    return (first == '"' && last == '"') || (first == '\'' && last == '\'');
}

// int and float can be mixed
// used for type checking of arithmetic and comparison operators
bool isNumberType(TokenType t){
    return t==TokenType::inth || t==TokenType::floath;
}

// can a value of type "value" be stored in a variable of type "target" ?
// same type is fine, and an int can go into a float
// This is type compatibility rule (semantic analysis)
// only int -> float is allowed (implicit widening conversion)
bool canAssign(TokenType target, TokenType value){
    if (target==value) return true;
    return target==TokenType::floath && value==TokenType::inth;
}

// spaces to put in front of a line of the C++ output
// only for code to look neat (not needed necessarily)
string indentSpaces(int level) {
    return string(level * 4, ' ');
}

int findArithmeticOperator(const string& s){
    int depth=0;              // how many brackets are open right now
    bool inString=false;     // are we inside quotes?
    char quote=' ';          // which quote started the string

    for (size_t i=0;i<s.size();i++){
        char c=s[i];
        if(inString){
            if (c==quote) inString=false;  // string ended
            continue;
        }
        if(c=='"'||c=='\''){
            inString=true;
            quote=c;
            continue;
        }
        if(c=='(') depth++;
        if(c==')') depth--;

        bool isOperator = (c=='+' || c=='-' || c=='*' || c=='%');

        // i>0 so that a minus at the very start is not used as an operator
        // it would be unary minus, like -5, not a binary operator
        if (depth==0 && isOperator && i>0) return (int)i;
    }
    return -1;
}

// a print command : print(statement)
// grammar rule : print_statement -> "print" "(" expression ")"
bool isPrintStatement(const string& s){
    return startsWith(s,"print(") && s.size()>=7 && s.back()==')';
}

// declaration looks like : int x=5 (first word is TokenType)
// grammar rule : declaration -> TokenType identifier "=" expression
bool isDeclaration(const string& s){
    size_t firstSpace = s.find(' ');
    if (firstSpace == string::npos) return false;
    return typeFromWord(s.substr(0,firstSpace)) != TokenType::unknown;
}

// assignment has a single "=" (not part of ==)
// Grammar rule : assignment -> identifier "=" expression
// don't confuse ASSIGNMENT OPERATOR "=" with comparison
// operators ==, <=, >=, !=, ==, so look at the character around it 
bool isAssignment(const string& s){
    size_t eq=s.find('=');
    if (eq==string::npos) return false;

    bool equalBefore = (eq>0 && s[eq-1] == '=');
    bool equalAfter = (eq+1<s.size() && s[eq+1] == '=');
    return !equalAfter && !equalBefore;
}

//--------------------------------------
//---------------COMPILER---------------
//--------------------------------------

class HydrogenCompiler{
    vector <Line> lines;   // all the cleaned lines
    size_t pos=0;          // index of next line to compile
                           // (like the "current token" pointer in a parser)
    map <string,TokenType> symbols;  // variable name -> its type
    // This is the SYMBOL TABLE : it stores every identifier 
    // that has been declared and its type.

    //--------Finding the type of an expression---------
    // Semantic analysis for expressions. It works out the type of an
    // expression and throws an error if the types are not compatible.
    // The checks go from the simplest to the more complex kind of expression.
    TokenType infer(const string& raw,int LineNo){
        string s = trim(raw);

        if(s.empty()) throw makeError(LineNo,"Empty Expression...");

        //BOOLEAN LITERAL token (true and false)
        if (s == "true" || s == "false") return TokenType::boolh;

        // STRING LITERAL token
        if (isStringLiteral(s)) return TokenType::stringh;

        // NUMERIC LITERAL token: no dot = int, with dot = float
        if (isNumber(s)) {
            if (s.find('.') == string::npos) return TokenType::inth;
            return TokenType::floath;
        }

        // a single variable
        // IDENTIFIER token: we look it up in the symbol table to get its type
        if (symbols.count(s)) {
            return symbols[s];
        }

        // a single variable
        // IDENTIFIER token: we look it up in the symbol table to get its type
        if (symbols.count(s)) {
            return symbols[s];
        }

        // arithmetic: split at the first operator and check both sides
        // Binary expression:  expression -> expression operator expression
        // The function calls itself on the left and right parts (recursion).
        int opPos = findArithmeticOperator(s);
        if (opPos != -1) {
            char op = s[opPos];
            TokenType left = infer(s.substr(0, opPos), LineNo);
            TokenType right = infer(s.substr(opPos + 1), LineNo);

            // "a" + "b" is a string
            // (+ on two strings means concatenation)
            if (op == '+' && left == TokenType::stringh && right == TokenType::stringh) {
                return TokenType::stringh;
            }

            // number with number: float if either side is float, or if we divide
            // (the result type follows the operand types)
            if (isNumberType(left) && isNumberType(right)) {
                if (op == '/' || left == TokenType::floath || right == TokenType::floath) {
                    return TokenType::floath;
                }
                return TokenType::inth;
            }

            // for example string - int ends up here (type error)
            throw makeError(LineNo, "incompatible arithmetic operands");
        }

        // comparisons (the 2 letter ones come first so <= is not read as <)
        // RELATIONAL OPERATOR tokens. Order matters here: this is the idea of
        // "longest match" (maximal munch) in lexical analysis.
        vector<string> comparisons = {"==", "!=", "<=", ">=", "<", ">"};
        for (string op : comparisons) {
            size_t i = s.find(op);
            if (i != string::npos) {
                TokenType left = infer(s.substr(0, i), LineNo);
                TokenType right = infer(s.substr(i + op.size()), LineNo);

                bool sameType = (left == right);
                bool numbers = isNumberType(left) && isNumberType(right);
                if (!sameType && !numbers) {
                    throw makeError(LineNo, "incompatible comparison types");
                }
                // a comparison always gives true or false
                return TokenType::boolh;
            }
        }

        // nothing matched, so this is not a valid token or expression
        // (for example an undeclared identifier ends up here)
        throw makeError(LineNo, "cannot infer expression type: " + s);
    }

    // ---------- small checks used by if and while ----------

    // Takes the condition out of a line like "if x > 3:" and checks it.
    // keywordLength is 3 for "if " and 6 for "while "
    // Grammar rule:  if_stmt -> "if" expression ":"   (same for while)
    string readCondition(const Line& line, size_t keywordLength, const string& keyword) {
        string s = line.text;

        // the ':' DELIMITER must be at the end of the line (syntax check)
        if (s.back() != ':') {
            throw makeError(line.number, "expected ':' after " + keyword + " condition");
        }

        // everything between the keyword and the ':'
        string condition = trim(s.substr(keywordLength, s.size() - keywordLength - 1));

        // semantic check: the condition has to be a boolean expression
        if (infer(condition, line.number) != TokenType::boolh) {
            throw makeError(line.number, keyword + " condition must be boolean");
        }
        return condition;
    }

    // the next line must be one level deeper (the body of the if/while/else)
    // Indentation is part of the syntax in Hydrogen (like Python).
    // The indent change acts like the { } tokens in C++.
    void checkBodyExists(int lineNo, int level, const string& message) {
        if (pos >= lines.size() || lines[pos].indent != level + 1) {
            throw makeError(lineNo, message);
        }
    }

    // ---------- one function for each kind of line ----------
    // Each function below handles one grammar rule (statement type) and
    // does the code generation for it.

    // Grammar:  if_stmt -> "if" expression ":" block [ "else" ":" block ]
    string compileIf(const Line& line, int level) {
        string prefix = indentSpaces(level);
        string condition = readCondition(line, 3, "if");
        checkBodyExists(line.number, level, "expected an indented block");

        // generated C++ code: if (condition) {
        string out = prefix + "if (" + condition + ") {\n";
        out += compileBlock(level + 1);   // recursion for the inner block

        // is the next line an else for this if?
        // ("else:" is a KEYWORD followed by the ':' DELIMITER)
        if (pos < lines.size() && lines[pos].indent == level && lines[pos].text == "else:") {
            pos++;   // skip the else line
            checkBodyExists(line.number, level, "expected an indented else block");
            out += prefix + "} else {\n";
            out += compileBlock(level + 1);
        }

        out += prefix + "}\n";
        return out;
    }

    // Grammar:  while_stmt -> "while" expression ":" block
    string compileWhile(const Line& line, int level) {
        string prefix = indentSpaces(level);
        string condition = readCondition(line, 6, "while");
        checkBodyExists(line.number, level, "expected an indented block");

        string out = prefix + "while (" + condition + ") {\n";
        out += compileBlock(level + 1);
        out += prefix + "}\n";
        return out;
    }

    // Grammar:  print_stmt -> "print" "(" expression ")"
    string compilePrint(const Line& line, int level) {
        string s = line.text;

        // the part between print( and )
        string value = trim(s.substr(6, s.size() - 7));
        infer(value, line.number);   // only to check that it is a valid expression

        // generated C++ code: std::cout << value << '\n';
        return indentSpaces(level) + "std::cout << " + value + " << '\\n';\n";
    }

    // example: int x = 5
    // Grammar:  declaration -> type identifier "=" expression
    string compileDeclaration(const Line& line, int level) {
        string s = line.text;

        size_t firstSpace = s.find(' ');
        // first token = the TYPE KEYWORD
        TokenType declared = typeFromWord(s.substr(0, firstSpace));

        string rest = trim(s.substr(firstSpace + 1));   // "x = 5"
        size_t eq = rest.find('=');
        if (eq == string::npos) {
            throw makeError(line.number, "expected '=' in declaration");
        }

        // second token = the IDENTIFIER, then the '=' OPERATOR, then the expression
        string name = trim(rest.substr(0, eq));
        string value = trim(rest.substr(eq + 1));

        if (!isValidName(name)) {
            throw makeError(line.number, "invalid variable name");
        }
        // a name can be declared only once (no redeclaration in the symbol table)
        if (symbols.count(name)) {
            throw makeError(line.number, "variable already declared: " + name);
        }

        // we find the type before saving the name, so "int x = x + 1" is an error
        TokenType actual = infer(value, line.number);
        if (!canAssign(declared, actual)) {
            throw makeError(line.number,
                "cannot assign " + typeName(actual) + " to " + typeName(declared));
        }

        // add the identifier to the symbol table
        symbols[name] = declared;
        return indentSpaces(level) + typeName(declared) + " " + name + " = " + value + ";\n";
    }

    // example: x = x + 1
    // Grammar:  assignment -> identifier "=" expression
    string compileAssignment(const Line& line, int level) {
        string s = line.text;

        size_t eq = s.find('=');
        string name = trim(s.substr(0, eq));
        string value = trim(s.substr(eq + 1));

        // the identifier must already be in the symbol table (declared before use)
        if (!symbols.count(name)) {
            throw makeError(line.number, "undeclared variable: " + name);
        }

        // type checking: the value must fit into the variable's type
       TokenType actual = infer(value, line.number);
        TokenType expected = symbols[name];
        if (!canAssign(expected, actual)) {
            throw makeError(line.number, "assignment type mismatch for " + name);
        }

        return indentSpaces(level) + name + " = " + value + ";\n";
    }

    // ---------- compile all the lines of one level ----------

    // Keeps going while the lines have the same indent as "level".
    // For if/while it calls itself for the lines inside (one level deeper).
    // This is the main parsing function (recursive descent).
    // Grammar:  block -> statement*
    // It looks at the first token(s) of each line to decide which kind of
    // statement it is.
    string compileBlock(int level) {
        string out;

        while (pos < lines.size() && lines[pos].indent == level) {
            Line line = lines[pos];
            pos++;   // this line is now being used

            // the order of the checks matters: keywords first, then
            // print, then declaration (starts with a type keyword),
            // then assignment (identifier followed by =)
            if (startsWith(line.text, "if ")) {
                out += compileIf(line, level);
            } else if (startsWith(line.text, "while ")) {
                out += compileWhile(line, level);
            } else if (isPrintStatement(line.text)) {
                out += compilePrint(line, level);
            } else if (isDeclaration(line.text)) {
                out += compileDeclaration(line, level);
            } else if (isAssignment(line.text)) {
                out += compileAssignment(line, level);
            } else {
                // the line does not match any grammar rule (syntax error)
                throw makeError(line.number, "unsupported statement");
            }
        }

        return out;
    }

public:
    // Splits the source text into lines and works out each line's indent
    // This is the scanning part (lexical analysis, simplified):
    // it removes whitespace and comments, and measures the indentation.
    HydrogenCompiler(const string& source) {
        istringstream input(source);
        string row;
        int lineNo = 0;

        while (getline(input, row)) {
            lineNo++;   // counted even for blank lines so the numbers match the file

            string content = trim(row);
            if (content.empty()) continue;       // blank line
            if (content[0] == '#') continue;     // comment (the scanner ignores it,
                                                 // it never reaches the parser)

            // lexical error: tabs are not allowed because indentation uses spaces
            if (row.find('\t') != string::npos) {
                throw makeError(lineNo, "use spaces instead of tabs");
            }

            // count the spaces at the start of the line
            int spaces = 0;
            while (spaces < (int)row.size() && row[spaces] == ' ') {
                spaces++;
            }

            if (spaces % 4 != 0) {
                throw makeError(lineNo, "use multiples of 4 spaces");
            }

            Line line;
            line.indent = spaces / 4;
            line.number = lineNo;
            line.text = content;
            lines.push_back(line);
        }
    }

    // Returns the whole C++ program as a string
    // This starts the parsing at level 0 and then adds the fixed C++ parts
    // (the #include lines and main) around the generated body.
    string compile() {
        if (!lines.empty() && lines[0].indent != 0) {
            throw runtime_error("Top-level code must not be indented");
        }

        string body = compileBlock(0);

        // if some lines are left, an indent was wrong somewhere
        if (pos != lines.size()) {
            throw runtime_error("Unexpected indentation");
        }

        // code generation: the final C++ program
        string result = "#include <iostream>\n";
        result += "#include <string>\n\n";
        result += "int main() {\n";
        result += body;
        result += "    return 0;\n";
        result += "}\n";
        return result;
    }
};  

//-------------------------------------------
//------------ Main Function ----------------
//-------------------------------------------
int main(int argc, char* argv[]){
    if (argc!=2){
        cerr<<"Incorrect Usage. Correct usage is... "<<endl;
        cerr<<"hydrogen <file.hyd"<<endl;
        return EXIT_FAILURE;
    }

    ifstream file(argv[1]);
    if (!file) {
        cerr << "Cannot open file: " << argv[1] << '\n';
        return 1;
    }

    // read the whole file into one string
    stringstream buffer;
    buffer << file.rdbuf();

    // any lexical, syntax or semantic error is thrown as an exception
    // and caught here, then printed as a compiler error message
    try {
        HydrogenCompiler compiler(buffer.str());
        cout << compiler.compile();
    } catch (const exception& e) {
        cerr << "Hydrogen compiler error: " << e.what() << '\n';
        return 1;
    } 

    return EXIT_SUCCESS;
}