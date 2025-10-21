#include "token.hpp"

namespace bolt::frontend
{
    std::string_view toString(TokenKind kind)
    {
        switch (kind)
        {
        case TokenKind::EndOfFile: return "endOfFile";
        case TokenKind::Identifier: return "identifier";
        case TokenKind::IntegerLiteral: return "integerLiteral";
        case TokenKind::StringLiteral: return "stringLiteral";

        case TokenKind::KeywordPackage: return "package";
        case TokenKind::KeywordModule: return "module";
        case TokenKind::KeywordImport: return "import";
        case TokenKind::KeywordBlueprint: return "blueprint";
        case TokenKind::KeywordEnumeration: return "enumeration";
        case TokenKind::KeywordInterface: return "interface";
        case TokenKind::KeywordFunction: return "function";
        case TokenKind::KeywordConstant: return "constant";
        case TokenKind::KeywordMutable: return "mutable";
        case TokenKind::KeywordFixed: return "fixed";
        case TokenKind::KeywordAlias: return "alias";
        case TokenKind::KeywordMatch: return "match";
        case TokenKind::KeywordGuard: return "guard";
        case TokenKind::KeywordReturn: return "return";
        case TokenKind::KeywordBreak: return "break";
        case TokenKind::KeywordContinue: return "continue";
        case TokenKind::KeywordPublic: return "public";
        case TokenKind::KeywordUse: return "use";
        case TokenKind::KeywordExtern: return "extern";
        case TokenKind::KeywordIntrinsic: return "intrinsic";
        case TokenKind::KeywordNew: return "new";
        case TokenKind::KeywordDelete: return "delete";
        case TokenKind::KeywordConstructor: return "constructor";
        case TokenKind::KeywordDestructor: return "destructor";
        case TokenKind::KeywordTrue: return "true";
        case TokenKind::KeywordFalse: return "false";
        case TokenKind::KeywordNull: return "null";
        case TokenKind::KeywordVoid: return "void";
        case TokenKind::KeywordLink: return "link";
        case TokenKind::KeywordIf: return "if";
        case TokenKind::KeywordElse: return "else";
        case TokenKind::KeywordWhile: return "while";
        case TokenKind::KeywordFor: return "for";
        case TokenKind::KeywordSwitch: return "switch";
        case TokenKind::KeywordCase: return "case";

        case TokenKind::LeftBrace: return "leftBrace";
        case TokenKind::RightBrace: return "rightBrace";
        case TokenKind::LeftParen: return "leftParen";
        case TokenKind::RightParen: return "rightParen";
        case TokenKind::LeftBracket: return "leftBracket";
        case TokenKind::RightBracket: return "rightBracket";
        case TokenKind::Comma: return "comma";
        case TokenKind::Colon: return "colon";
        case TokenKind::Semicolon: return "semicolon";
        case TokenKind::Dot: return "dot";
        case TokenKind::Arrow: return "arrow";
        case TokenKind::Equals: return "equals";
        case TokenKind::Plus: return "plus";
        case TokenKind::Minus: return "minus";
        case TokenKind::PlusEquals: return "plusEquals";
        case TokenKind::MinusEquals: return "minusEquals";
        case TokenKind::PlusPlus: return "plusPlus";
        case TokenKind::MinusMinus: return "minusMinus";
        case TokenKind::Asterisk: return "asterisk";
        case TokenKind::Slash: return "slash";
        case TokenKind::Percent: return "percent";
        case TokenKind::Ampersand: return "ampersand";
        case TokenKind::AmpersandAmpersand: return "ampersandAmpersand";
        case TokenKind::Pipe: return "pipe";
        case TokenKind::Caret: return "caret";
        case TokenKind::Bang: return "bang";
        case TokenKind::LessThan: return "lessThan";
        case TokenKind::GreaterThan: return "greaterThan";
        case TokenKind::LessEquals: return "lessEquals";
        case TokenKind::GreaterEquals: return "greaterEquals";
        case TokenKind::EqualsEquals: return "equalsEquals";
        case TokenKind::BangEquals: return "bangEquals";
        }

        return "unknown";
    }
} // namespace bolt::frontend

