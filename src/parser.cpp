#include <sstream>
#include "parser.hpp"

/**
 * Error on unexpected character
 * @param expected - expecting characters
 * @return - error message
 */
runtime_error Parser::unexpected(const string &expected) {
    return runtime_error("Line number " + to_string(lineNumber) + ": Expect " + expected);
}

/**
 * Parse parameters of function
 * @return - JSON tree of parameters
 */
json Parser::parseParameters() {
    json params = json::array();
    while (declarationIncoming()) {
        Declaration declaration = parseDeclaration("ParameterDeclaration");
        params.push_back(declaration);

        if (lookahead(")")) {
            return params;
        }
        consume(",");
    }
    consume(")");
    return params;
}

/**
 * Parse body of sub-statements
 * @param shouldBeBlock - should sub-statements be block statements
 * @return - JSON tree of body
 */
json Parser::parseBody(bool shouldBeBlock) {
    json statements = json::array();
    if (curr == '{' || shouldBeBlock) { // BlockStatement
        BodyStatement block;
        block.kind = "BlockStatement";
        block.position = lineNumber;
        consume("{");
        if (!comments.empty()) {
            for (const json &comment: comments) {
                statements.push_back(comment);
            }
            comments.clear();
        }
        while (curr && curr != '}') {
            statements.push_back(parseStatement());
            if (!comments.empty()) {
                for (const json &comment: comments) {
                    statements.push_back(comment);
                }
                comments.clear();
            }
        }

        consume("}");
        block.body = statements;
        return block;
    } else { // InlineStatement
        BodyStatement line;
        line.kind = "InlineStatement";
        line.position = lineNumber;
        if (!comments.empty()) {
            for (const json &comment: comments) {
                statements.push_back(comment);
            }
            comments.clear();
        }
        if (!lookahead(";")) {
            statements.push_back(parseStatement());
        }
        line.body = statements;
        return line;
    }
}

/**
 * Parse statement
 * @return - JSON tree of statement
 */
json Parser::parseStatement() {
    if (lookahead("if")) { // IfStatement
        IfStatement statement;
        statement.kind = "IfStatement";
        statement.position = lineNumber;
        consume("(");
        json condition = parseExpression(")");
        if (condition.is_null()) {
            throw unexpected("if condition");
        }
        statement.condition = condition;
        if (lookahead("else")) {
            throw unexpected("if body statement");
        }
        statement.body = parseBody();
        if (lookahead("else")) {
            statement.elseBody = parseBody();
        } else {
            statement.elseBody = nullptr;
        }
        return statement;
    } else if (lookahead("while")) { // WhileStatement
        WhileStatement statement;
        statement.kind = "WhileStatement";
        statement.position = lineNumber;
        consume("(");
        json condition = parseExpression(")");
        if (condition.is_null()) {
            throw unexpected("while condition");
        }
        statement.condition = condition;
        statement.body = parseBody();
        return statement;
    } else if (lookahead("do")) { // DoWhileStatement
        WhileStatement statement;
        statement.kind = "DoWhileStatement";
        statement.position = lineNumber;
        statement.body = parseBody();
        consume("while");
        consume("(");
        json condition = parseExpression(")");
        if (condition.is_null()) {
            throw unexpected("while condition");
        }
        statement.condition = condition;
        consume(";");
        return statement;
    } else if (lookahead("for")) { // ForStatement
        ForStatement statement;
        statement.kind = "ForStatement";
        statement.position = lineNumber;
        consume("(");
        json init = parseStatement();
        string kind = init["kind"];
        if (kind == "VariableDefinition" || kind == "VariableDeclaration") {
            init["kind"] = "For" + kind;
        }
        statement.init = init;
        statement.condition = parseExpression(";");
        statement.step = parseExpression(")");
        statement.body = parseBody();
        return statement;
    } else if (lookahead("return")) { // ReturnStatement
        ReturnStatement statement;
        statement.kind = "ReturnStatement";
        statement.position = lineNumber;
        statement.value = parseExpression(";");
        return statement;
    } else if (lookahead("break")) { // BreakStatement
        InterruptStatement statement;
        statement.kind = "BreakStatement";
        statement.position = lineNumber;
        statement.label = parseExpression(";");
        return statement;
    } else if (lookahead("continue")) { // ContinueStatement
        InterruptStatement statement;
        statement.kind = "ContinueStatement";
        statement.position = lineNumber;
        statement.label = parseExpression(";");
        return statement;
    } else if (declarationIncoming()) { // Declaration
        return parseDefinition(parseDeclaration());
    } else { // ExpressionStatement
        ExpressionStatement statement;
        statement.kind = "ExpressionStatement";
        statement.position = lineNumber;
        statement.expression = parseExpression(";");
        return statement;
    }
}

/**
 * Parse definition
 * @param declaration - original declaration
 * @param isGlobal - is global variable
 * @return - JSON tree of definition
 */
json Parser::parseDefinition(const Declaration &declaration, bool isGlobal) {
    json length;
    bool isArray = false;
    while (lookahead("[")) {
        isArray = true;
        if (!lookahead("]")) {
            length.push_back(parseExpression());
            consume("]");
        } else {
            length.push_back(nullptr);
        }
    }
    Definition definition;
    definition.identifier = declaration.identifier;
    definition.type = declaration.type;
    definition.position = declaration.position;
    if (isArray) { // Array
        definition.length = length;
    }
    if (lookahead("=")) { // Definition
        definition.kind = isArray ? "ArrayDefinition" : "VariableDefinition";
        definition.value = parseExpression();
    } else { // Declaration
        definition.kind = isArray ? "ArrayDeclaration" : "VariableDeclaration";
    }
    if (isGlobal) {
        definition.kind = "Global" + definition.kind;
    }
    if (curr == ',') { // multiple identifiers
        json modifiers = definition.type.modifiers;
        string name;
        for (const json &modifier: modifiers) {
            name += string(modifier) + " ";
        }
        name += definition.type.name;
        source.replace(static_cast<unsigned long>(index), 1, name);
    } else {
        consume(";");
    }
    return definition;
}

/**
 * Parse function
 * @param declaration - original declaration
 * @return - JSON tree of function definition
 */
json Parser::parseFunction(const Declaration &declaration) {
    json parameters = parseParameters();
    if (lookahead(";")) {
        FunctionDeclaration functionDeclaration;
        functionDeclaration.identifier = declaration.identifier;
        functionDeclaration.type = declaration.type;
        functionDeclaration.position = declaration.position;
        functionDeclaration.kind = "FunctionDeclaration";
        functionDeclaration.parameters = parameters;
        return functionDeclaration;
    } else {
        FunctionDefinition functionDefinition;
        functionDefinition.identifier = declaration.identifier;
        functionDefinition.type = declaration.type;
        functionDefinition.position = declaration.position;
        functionDefinition.kind = "FunctionDefinition";
        functionDefinition.parameters = parameters;
        functionDefinition.body = parseBody(true);
        return functionDefinition;
    }
}

/**
 * Parse expression
 * @param end - end character
 * @return - JSON tree of expression
 */
json Parser::parseExpression(const string &end) {
    json expr = parseBinary(parseUnary(), 0);
    if (!end.empty()) {
        consume(end);
    }
    return expr;
}

/**
 * Get incoming operator
 * @return - operator
 */
string Parser::scanBinaryOperator() {
    int _index = index;
    for (json &op: operators) {
        if (lookahead(op)) {
            index = _index;
            curr = source[index];
            return op;
        }
    }
    return "";
}

/**
 * Parse binary expression
 * @param left - left value of binary expression
 * @param minPrecedence - minimum precedence of operator
 * @return - JSON tree of binary expression
 */
json Parser::parseBinary(json left, int minPrecedence) {
    string ahead = scanBinaryOperator();
    while (!ahead.empty() && precedence[ahead] >= minPrecedence) {
        string op = ahead;
        int position = lineNumber;
        consume(op);
        auto right = parseUnary();
        if (right.is_null()) {
            throw unexpected("right value");
        }
        ahead = scanBinaryOperator();

        while (!ahead.empty() && precedence[ahead] > precedence[op]) {
            right = parseBinary(right, precedence[ahead]);
            if (right.is_null()) {
                throw unexpected("right value");
            }
            ahead = scanBinaryOperator();
        }

        BinaryExpression newExpression;
        newExpression.kind = "BinaryExpression";
        newExpression.position = position;
        newExpression.left = left;
        newExpression.right = right;
        newExpression.op = op;
        left = newExpression;
    }
    return left;
}

/**
 * Parse unary expression
 * @return  - JSON tree of unary expression
 */
json Parser::parseUnary() {
    json literal = parseLiteral();
    json indexes;
    while (lookahead("[")) {
        indexes.push_back(parseExpression());
        consume("]");
    }
    if (!indexes.empty()) { // IndexExpression
        IndexExpression indexExpression;
        indexExpression.kind = "IndexExpression";
        indexExpression.position = lineNumber;
        indexExpression.array = literal;
        indexExpression.indexes = indexes;
        return indexExpression;
    } else if (lookahead("(")) { // CallExpression
        if (!literal.is_null()) {
            CallExpression callExpression;
            callExpression.kind = "CallExpression";
            callExpression.position = lineNumber;
            json arguments;

            while (curr) {
                arguments.push_back(parseExpression());

                if (!lookahead(",")) {
                    break;
                }
            }
            consume(")");
            callExpression.arguments = arguments;
            callExpression.callee = literal;
            return callExpression;
        } else { // ParenthesesExpression
            ParenthesesExpression parenthesesExpression;
            parenthesesExpression.kind = "ParenthesesExpression";
            parenthesesExpression.position = lineNumber;
            parenthesesExpression.expression = parseExpression();
            consume(")");
            return parenthesesExpression;
        }
    } else {
        return literal;
    }
}

/**
 * Parse literal value
 * @return - JSON tree of literal
 */
json Parser::parseLiteral() {
    if (lookahead("{")) { // ArrayLiteral
        Literal<json> literal;
        literal.kind = "ArrayLiteral";
        literal.position = lineNumber;
        json entries;
        while (curr) {
            entries.push_back(parseExpression());

            if (!lookahead(",")) {
                break;
            }
        }
        consume("}");
        literal.value = entries;
        return literal;
    } else if (curr == '\'') { // CharLiteral
        next(true, true);
        Literal<string> literal;
        literal.kind = "CharLiteral";
        literal.position = lineNumber;
        string ch = string(1, curr);
        if (curr == '\\') {
            ch = parseEscape();
        } else {
            next(true, true);
        }
        consume("'");
        literal.value = ch;
        return literal;
    } else if (curr == '"') { // StringLiteral
        Literal<string> literal;
        literal.kind = "StringLiteral";
        literal.position = lineNumber;
        literal.value = parseString();
        return literal;
    } else if (lookahead("0x")) { // HexNumberLiteral
        return parseNumber(16);
    } else if (lookahead("-0x")) { // HexNumberLiteral
        Literal<string> literal = parseNumber(16);
        literal.value = "-" + literal.value;
        return literal;
    } else if (isFloat(curr) || curr == '-') { // NumberLiteral
        return parseNumber(10);
    } else if (isIdentifierStart(curr)) { // Identifier
        return parseIdentifier();
    } else {
        return nullptr;
    }
}

/**
 * Determine incoming declaration
 * @return - whether incoming string is declaration
 */
bool Parser::declarationIncoming() {
    int prevIndex = index;
    for (json &modifier: typeModifiers) {
        if (lookahead(modifier)) {
            index = prevIndex;
            curr = source[index];
            return true;
        }
    }
    for (json &name: typeNames) {
        if (lookahead(name)) {
            index = prevIndex;
            curr = source[index];
            return true;
        }
    }
    return false;
}

/**
 * Parse declaration
 * @param kind - declaration kind
 * @return - JSON tree of declaration
 */
Declaration Parser::parseDeclaration(const string &kind) {
    json::array_t modifiers = json::array();
    bool hasModifier;
    Type type;
    type.kind = "Type";
    type.position = lineNumber;
    do {
        hasModifier = false;
        for (json &modifier: typeModifiers) {
            if (lookahead(modifier)) {
                modifiers.push_back(modifier);
                hasModifier = true;
            }
        }
    } while (hasModifier);
    for (json &name: typeNames) {
        if (lookahead(name)) {
            type.modifiers = modifiers;
            type.name = name;
            Declaration declaration;
            declaration.position = lineNumber;
            declaration.identifier = parseIdentifier();
            declaration.type = type;
            if (!kind.empty()) {
                declaration.kind = kind;
            }
            return declaration;
        }
    }
    if (!modifiers.empty()) {
        type.name = modifiers.back();
        modifiers.pop_back();
        type.modifiers = modifiers;
        Declaration declaration;
        declaration.position = lineNumber;
        declaration.identifier = parseIdentifier();
        declaration.type = type;
        if (!kind.empty()) {
            declaration.kind = kind;
        }
        return declaration;
    }
    throw unexpected("correct type name");
}

/**
 * Parse #include statement
 * @return - JSON tree of #include statement
 */
json Parser::parseInclude() {
    IncludeStatement statement;
    statement.kind = "IncludeStatement";
    statement.position = lineNumber;
    string str;
    if (curr == '<') {
        while (curr && curr != '>') {
            str.push_back(curr);
            next(true);
        }
    } else if (curr == '"') {
        do {
            str.push_back(curr);
            next(true);
        } while (curr && curr != '"');
    } else {
        throw unexpected("\" or <");
    }
    str.push_back(curr);
    statement.file = str;
    next(true);
    return statement;
}

/**
 * Parse #define statement
 * @return - JSON tree of #define statement
 */
json Parser::parsePredefine() {
    PredefineStatement statement;
    statement.kind = "PredefineStatement";
    statement.position = lineNumber;
    statement.identifier = parseIdentifier();
    json arguments;
    if (lookahead("(")) {
        while (curr) {
            arguments.push_back(parseExpression());

            if (!lookahead(",")) {
                break;
            }
        }
        consume(")");
    }
    statement.arguments = arguments;
    if (!arguments.is_null() && curr != '(') {
        throw unexpected("(");
    }
    json str = parseExpression();
    statement.value = str;
    return statement;
}

/**
 * Parse string literal
 * @param keepBlanks - should keep spaces
 * @return - string literal
 */
string Parser::parseString(bool keepBlanks) {
    string str;
    next(true, true);
    while (curr && curr != '"') {
        if (curr == '\\') {
            str += parseEscape();
        } else {
            str.push_back(curr);
            next(true, true);
        }
    }
    if (!lookahead("\"", keepBlanks)) {
        throw unexpected("double quote");
    }
    return str;
}

/**
 * Parse escaped char
 * @return - unescaped char
 */
string Parser::parseEscape() {
    index++;
    curr = source[index];
    if (curr == 'x') {
        next(true, true);
        int code = 0;

        for (int i = 0; i < 2; i++) {
            if (isHex(curr)) {
                code = code * 16 + (int) string("0123456789abcdef").find((char) tolower(curr));
                next(true, true);
            }
        }
        return string(1, code);
    } else if (isOct(curr)) {
        int code = 0;

        for (int i = 0; i < 3; i++) {
            if (isOct(curr)) {
                code = code * 8 + (int) string("01234567").find((char) tolower(curr));
                next(true, true);
            }
        }
        return string(1, code);
    } else if (escapes[curr]) {
        string escaped = "\\" + string(1, curr);
        next(true, true);
        return escaped;
    } else {
        throw unexpected("escape sequence");
    }
}

/**
 * Parse identifier
 * @param keepBlanks - should keep spaces
 * @return - JSON tree of identifier
 */
Identifier Parser::parseIdentifier(bool keepBlanks) {
    if (!isIdentifierStart(curr)) {
        throw unexpected("Identifier");
    }
    Identifier identifier;
    identifier.kind = "Identifier";
    identifier.position = lineNumber;
    string name = string(1, curr);
    next(true);
    while (curr && isIdentifierBody(curr)) {
        name.push_back(curr);
        next(true);
    }
    if (!keepBlanks) {
        skipSpaces();
    }
    identifier.name = name;
    return identifier;
}

/**
 * Parse number literal
 * @param digits - valid digits
 * @return - JSON tree of number literal
 */
Literal<string> Parser::parseNumber(int digits) {
    if (digits == 16 && !isHex(curr)) {
        throw unexpected("Number");
    }
    Literal<string> number;
    number.position = lineNumber;
    string type = "NumberLiteral";
    if (digits == 16) {
        type = "HexNumberLiteral";
    }
    if (curr == '.') {
        type = "FloatNumberLiteral";
    }
    string value = string(1, curr);
    next(true);
    while ((curr && (digits == 16 ? isHex(curr) : isFloat(curr)))
           || (digits != 16 && tolower(curr) == 'e')
           || (curr == '-' && digits != 16 && tolower(source[index - 1]) == 'e')) {
        if (curr == '.') {
            type = "FloatNumberLiteral";
        }
        value.push_back(curr);
        next(true);
    }
    if (value[0] == '0' && type != "FloatNumberLiteral") {
        type = "OctNumberLiteral";
    }
    if (tolower(curr) == 'l') {
        type = "Long" + type;
        value.push_back(curr);
        next(true);
    }
    if (tolower(curr) == 'u') {
        type = "Unsigned" + type;
        value.push_back(curr);
        next(true);
    }
    if (digits == 16 && curr == '.') {
        throw unexpected("hex number");
    }
    if (digits == 16) {
        value = "0x" + value;
    }
    skipSpaces();
    number.value = value;
    number.kind = type;
    return number;
}

/**
 * Parse comment
 * @return - JSON tree of comment
 */
json Parser::parseComment() {
    Comment statement;
    string str;
    if (lookahead("/*")) {
        statement.kind = "BlockComment";
        statement.position = lineNumber;
        while (curr != '*' || source[index + 1] != '/') {
            str.push_back(curr);
            next(true, true);
        }
        statement.content = str;
        index += 2;
        curr = source[index];
    } else if (lookahead("//")) {
        statement.kind = "InlineComment";
        statement.position = lineNumber;
        while (curr != '\n') {
            str.push_back(curr);
            next(true, true);
        }
        statement.content = str;
    } else {
        return nullptr;
    }
    return statement;
}

/**
 * Match string ahead
 * @param str - string to match
 * @param keepBlanks - should keep spaces
 * @return - result of matching
 */
bool Parser::lookahead(const string &str, bool keepBlanks) {
    int _index = index;
    for (char ch: str) {
        if (curr != ch) {
            index = _index;
            curr = source[index];
            return false;
        }
        next(true);
    }

    if (isIdentifierStart(curr) && isIdentifier(str)) {
        index = _index;
        curr = source[index];
        return false;
    }

    if (!keepBlanks) {
        skipSpaces();
    }
    return true;
}

/**
 * Consume rest characters
 * @param str - characters to be skipped
 */
void Parser::consume(const string &str) {
    for (char ch: str) {
        if (curr != ch) {
            throw unexpected(str);
        }
        next();
    }
}

/**
 * Skip all spaces
 */
void Parser::skipSpaces() {
    if (isSpace(curr)) {
        next();
    }
}

/**
 * Go to next valid character
 * @param withSpaces - should keep spaces
 * @param withComment - should keep comment
 */
void Parser::next(bool withSpaces, bool withComment) {
    if (curr == '\n') {
        lineNumber++;
    }
    index++;
    curr = source[index];
    bool skipped;
    do {
        skipped = false;
        if (!withSpaces && isSpace(curr)) {
            while (curr && isSpace(curr)) {
                if (curr == '\n') {
                    lineNumber++;
                }
                index++;
                curr = source[index];
            }
            skipped = true;
        }
        if (!withComment) {
            json comment = parseComment();
            if (!comment.is_null()) {
                skipped = true;
                comments.push_back(comment);
            }
            if (isIllegal(curr)) {
                throw unexpected("legal character");
            }
        }
    } while (skipped);
}

/**
 * Parse source code
 * @return
 */
json Parser::parse() {
    next();
    json statements;
    while (curr) {
        skipSpaces();
        if (!comments.empty()) {
            for (const json &comment: comments) {
                statements.push_back(comment);
            }
            comments.clear();
        }
        if (lookahead("#include")) { // IncludeStatement
            statements.push_back(parseInclude());
        } else if (lookahead("#define")) { // PredefineStatement
            statements.push_back(parsePredefine());
        } else if (declarationIncoming()) { // GlobalDeclaration
            Declaration declaration = parseDeclaration();
            if (lookahead("(")) {
                statements.push_back(parseFunction(declaration));
            } else {
                statements.push_back(parseDefinition(declaration, true));
            }
        } else if (lookahead("typedef")) { // TypeDefinition
            Declaration declaration = parseDeclaration("TypeDefinition");
            typeNames.push_back(declaration.identifier.name);
            consume(";");
            statements.push_back(declaration);
        } else if (lookahead("struct")) {
            throw runtime_error("struct is not supported");
        } else if (lookahead("enum")) {
            throw runtime_error("enum is not supported");
        } else {
            throw unexpected("definition");
        }
        if (!comments.empty()) {
            for (const json &comment: comments) {
                statements.push_back(comment);
            }
            comments.clear();
        }
        skipSpaces();
    }

    Program program = {"Program", statements};
    return program;
}

/**
 * Constructor of class
 * @param src - source code
 */
Parser::Parser(string src) : source(move(src)), curr(), lineNumber(1), index(-1) {}
