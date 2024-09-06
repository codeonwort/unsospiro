namespace UnSospiro
{
    internal enum TokenType
    {
        // Tokens of single char
        LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
        COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR,

        // Tokens of 1 or 2 chars
        BANG, BANG_EQUAL,
        EQUAL, EQUAL_EQUAL,
        GREATER, GREATER_EQUAL,
        LESS, LESS_EQUAL,

        // Literals
        IDENTIFIER, STRING, NUMBER,

        // Keywords
        AND, CLASS, ELSE, FALSE, FUN, FOR, IF, NIL, OR,
        PRINT, RETURN, SUPER, THIS, TRUE, VAR, WHILE,

        EOF
    }

    internal class Token
    {
        TokenType type;
        string lexeme;
        object literal;
        int line;

        public Token(TokenType type, string lexeme, object literal, int line)
        {
            this.type = type;
            this.lexeme = lexeme;
            this.literal = literal;
            this.line = line;
        }

        public override string ToString()
        {
            return $"{type} {lexeme} {literal}";
        }
    }
}
