namespace UnSospiro
{
    internal class Scanner
    {
        private string source;
        private List<Token> tokens = new();
        private int start = 0;
        private int current = 0;
        private int line = 1;

        public Scanner(string source)
        {
            this.source = source;
        }

        public List<Token> ScanTokens()
        {
            while (!IsAtEnd())
            {
                start = current;
                ScanToken();
            }
            tokens.Add(new Token(TokenType.EOF, "", null, line));
            return tokens;
        }

        private bool IsAtEnd() => current >= source.Length;

        private void ScanToken()
        {
            char c = Advance();
            switch (c)
            {
                case '(': AddToken(TokenType.LEFT_PAREN); break;
                case ')': AddToken(TokenType.RIGHT_PAREN); break;
                case '{': AddToken(TokenType.LEFT_BRACE); break;
                case '}': AddToken(TokenType.RIGHT_BRACE); break;
                case ',': AddToken(TokenType.COMMA); break;
                case '.': AddToken(TokenType.DOT); break;
                case '-': AddToken(TokenType.MINUS); break;
                case '+': AddToken(TokenType.PLUS); break;
                case ';': AddToken(TokenType.SEMICOLON); break;
                case '*': AddToken(TokenType.STAR); break;
                case '!':
                    AddToken(Match('=') ? TokenType.BANG_EQUAL : TokenType.BANG);
                    break;
                case '=':
                    AddToken(Match('=') ? TokenType.EQUAL_EQUAL : TokenType.EQUAL);
                    break;
                case '<':
                    AddToken(Match('=') ? TokenType.LESS_EQUAL : TokenType.LESS);
                    break;
                case '>':
                    AddToken(Match('=') ? TokenType.GREATER_EQUAL : TokenType.GREATER);
                    break;
                case '/':
                    if (Match('/'))
                    {
                        while (Peek() != '\n' && !IsAtEnd()) Advance();
                    }
                    else
                    {
                        AddToken(TokenType.SLASH);
                    }
                    break;
                case ' ': case '\r': case '\t':
                    // Ignore whitespace.
                    break;
                case '\n':
                    ++line;
                    break;
                case '"': ScanString(); break;
                default:
                    if (IsDigit(c))
                    {
                        ScanNumber();
                    }
                    else
                    {
                        MainProgram.Error(line, $"Unexpected character {c}.");
                    }
                    break;
            }
        }

        private char Advance() => source.ElementAt(current++);

        private bool Match(char expected)
        {
            if (IsAtEnd()) return false;
            if (source.ElementAt(current) != expected) return false;
            ++current;
            return true;
        }

        private char Peek() => IsAtEnd() ? '\0' : source.ElementAt(current);

        private char PeekNext() => (current + 1 >= source.Length) ? '\0' : source.ElementAt(current + 1);

        private bool IsDigit(char c) => '0' <= c && c <= '9';

        private void AddToken(TokenType type) => AddToken(type, null);

        private void AddToken(TokenType type, object literal)
        {
            string text = source.Substring(start, current - start);
            tokens.Add(new Token(type, text, literal, line));
        }

        private void ScanString()
        {
            while (Peek() != '"' && !IsAtEnd())
            {
                if (Peek() == '\n') ++line; // Allow multi-line string.
                Advance();
            }
            if (IsAtEnd())
            {
                MainProgram.Error(line, "Unterminated string.");
            }
            Advance(); // Closing quotation mark

            string value = source.Substring(start + 1, current - start - 2);
            AddToken(TokenType.STRING, value);
        }

        private void ScanNumber()
        {
            while (IsDigit(Peek())) Advance();
            if (Peek() == '.' && IsDigit(PeekNext()))
            {
                // Consume '.'
                Advance();
                while (IsDigit(Peek())) Advance();
            }
            AddToken(TokenType.NUMBER, Double.Parse(source.Substring(start, current - start)));
        }

    }
}
