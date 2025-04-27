#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <stack>
#include <set>

using namespace std;

struct Token {
    int line;
    string type;
    string value;
};

struct GrammarRule {
    string lhs;
    vector<string> rhs;
};

class LL1Parser {
private:
    vector<GrammarRule> grammar;
    set<string> terminals, nonTerminals;
    map<string, set<string>> firstSet, followSet;
    map<pair<string, string>, GrammarRule> parseTable;
    string startSymbol;

public:
    void loadGrammar(const string& filename);
    void computeFirst();
    void computeFollow();
    void buildParseTable();
    bool parseTokens(const vector<Token>& tokens, const string& outputErrFile);

private:
    void split(const string& line, vector<string>& out);
    set<string> computeFirstOf(const vector<string>& symbols);
    bool isTerminal(const string& symbol);
    set<string> computeFollowOf(const string& nonTerminal);
};

// Load grammar rules from file
void LL1Parser::loadGrammar(const string& filename) {
    ifstream infile(filename);
    string line;
    while (getline(infile, line)) {
        if (line.empty()) continue;
        vector<string> parts;
        split(line, parts);
        if (parts.size() < 3 || parts[1] != "->") continue;
        GrammarRule rule;
        rule.lhs = parts[0];
        for (size_t i = 2; i < parts.size(); ++i) rule.rhs.push_back(parts[i]);
        grammar.push_back(rule);
        nonTerminals.insert(rule.lhs);
        for (const string& sym : rule.rhs) {
            if (!isupper(sym[0])) terminals.insert(sym);
            else nonTerminals.insert(sym);
        }
        if (startSymbol.empty()) startSymbol = rule.lhs;
    }
}

// Split a grammar rule line into tokens
void LL1Parser::split(const string& line, vector<string>& out) {
    stringstream ss(line);
    string token;
    while (ss >> token) out.push_back(token);
}

// Check if a symbol is a terminal
bool LL1Parser::isTerminal(const string& symbol) {
    return terminals.find(symbol) != terminals.end();
}

// Compute the FIRST set for a given symbol sequence
set<string> LL1Parser::computeFirstOf(const vector<string>& symbols) {
    set<string> first;
    for (const string& symbol : symbols) {
        if (isTerminal(symbol)) {
            first.insert(symbol);
            break;
        }
        else {
            set<string> firstOfNonTerminal = firstSet[symbol];
            first.insert(firstOfNonTerminal.begin(), firstOfNonTerminal.end());
            if (firstOfNonTerminal.find("epsilon") == firstOfNonTerminal.end()) break;
        }
    }
    return first;
}

// Compute the FIRST sets for all non-terminals
void LL1Parser::computeFirst() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const GrammarRule& rule : grammar) {
            set<string> firstOfLHS = firstSet[rule.lhs];
            set<string> firstOfRHS = computeFirstOf(rule.rhs);
            size_t oldSize = firstOfLHS.size();
            firstOfLHS.insert(firstOfRHS.begin(), firstOfRHS.end());
            firstSet[rule.lhs] = firstOfLHS;
            if (firstOfLHS.size() > oldSize) changed = true;
        }
    }
}

// Compute the FOLLOW set for a given non-terminal
set<string> LL1Parser::computeFollowOf(const string& nonTerminal) {
    set<string> follow;
    if (nonTerminal == startSymbol) follow.insert("$");

    for (const GrammarRule& rule : grammar) {
        for (size_t i = 0; i < rule.rhs.size(); ++i) {
            if (rule.rhs[i] == nonTerminal) {
                if (i + 1 < rule.rhs.size()) {
                    set<string> firstOfNext = computeFirstOf(vector<string>(rule.rhs.begin() + i + 1, rule.rhs.end()));
                    follow.insert(firstOfNext.begin(), firstOfNext.end());
                    if (firstOfNext.find("epsilon") != firstOfNext.end()) {
                        set<string> followOfLHS = followSet[rule.lhs];
                        follow.insert(followOfLHS.begin(), followOfLHS.end());
                    }
                }
                else {
                    set<string> followOfLHS = followSet[rule.lhs];
                    follow.insert(followOfLHS.begin(), followOfLHS.end());
                }
            }
        }
    }
    return follow;
}

// Compute the FOLLOW sets for all non-terminals
void LL1Parser::computeFollow() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const GrammarRule& rule : grammar) {
            for (size_t i = 0; i < rule.rhs.size(); ++i) {
                if (nonTerminals.find(rule.rhs[i]) != nonTerminals.end()) {
                    set<string> followOfRHS = computeFollowOf(rule.rhs[i]);
                    size_t oldSize = followSet[rule.rhs[i]].size();
                    followSet[rule.rhs[i]].insert(followOfRHS.begin(), followOfRHS.end());
                    if (followSet[rule.rhs[i]].size() > oldSize) changed = true;
                }
            }
        }
    }
}

// Build the parse table using FIRST and FOLLOW sets
void LL1Parser::buildParseTable() {
    for (const GrammarRule& rule : grammar) {
        set<string> firstOfRHS = computeFirstOf(rule.rhs);
        for (const string& terminal : firstOfRHS) {
            if (terminal != "epsilon") {
                parseTable[{rule.lhs, terminal}] = rule;
            }
        }
        if (firstOfRHS.find("epsilon") != firstOfRHS.end()) {
            set<string> followOfLHS = followSet[rule.lhs];
            for (const string& terminal : followOfLHS) {
                parseTable[{rule.lhs, terminal}] = rule;
            }
        }
    }
}

// Parse the token list using the LL(1) table
bool LL1Parser::parseTokens(const vector<Token>& tokens, const string& outputErrFile) {
    stack<string> parseStack;
    parseStack.push("$");
    parseStack.push(startSymbol);
    size_t index = 0;
    vector<Token> input = tokens;
    input.push_back({ -1, "$", "$" });
    ofstream errFile(outputErrFile);

    while (!parseStack.empty()) {
        string top = parseStack.top();
        string currentToken = input[index].type;
        if (top == "$" && currentToken == "$") {
            cout << "YES" << endl;
            return true;
        }
        else if (top == currentToken) {
            parseStack.pop();
            ++index;
        }
        else if (isTerminal(top)) {
            errFile << "Syntax error at line " << input[index].line << ": expected '" << top << "' but found '" << input[index].value << "'\n";
            cout << "NO" << endl;
            return false;
        }
        else {
            auto it = parseTable.find({ top, currentToken });
            if (it == parseTable.end()) {
                errFile << "Syntax error at line " << ((input[index].line < 0) ? input[index - 1].line : input[index].line) << ": unexpected token '" << ((input[index].value == "$") ? input[index - 1].value : input[index].value) << "'\n";
                cout << "NO" << endl;
                return false;
            }
            parseStack.pop();
            const GrammarRule& rule = it->second;
            for (auto it = rule.rhs.rbegin(); it != rule.rhs.rend(); ++it) {
                if (*it != "epsilon") parseStack.push(*it);
            }
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    LL1Parser parser;
    parser.loadGrammar(argv[1]);
    parser.computeFirst();
    parser.computeFollow();
    parser.buildParseTable();

    vector<Token> tokens;
    ifstream tokenFile(argv[2]);
    string line;
    while (getline(tokenFile, line)) {
        stringstream ss(line);
        Token tok;
        ss >> tok.line >> tok.type >> tok.value;
        tokens.push_back(tok);
    }

    parser.parseTokens(tokens, argv[3]);
    return 0;
}
