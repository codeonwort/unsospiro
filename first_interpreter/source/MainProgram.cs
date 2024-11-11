namespace UnSospiro
{
    internal interface IErrorListener
    {
        public void Error(int line, string message);
        public void Error(Token token, string message);
        public void RuntimeError(RuntimeException err);
    }

    internal class MainProgram : IErrorListener
    {
        private Interpreter interpreter;
        private bool hadError = false;
        private bool hadRuntimeError = false;

        private static void Main(string[] args)
        {
            var instance = new MainProgram();

            if (args.Length > 1)
            {
                Console.WriteLine("Usage: unsospiro [script]");
                Environment.Exit(64);
            }
            else if (args.Length == 1)
            {
                instance.RunFile(args[0]);
            }
            else
            {
                instance.RunPrompt();
            }
        }

        private MainProgram()
        {
            interpreter = new Interpreter(this);
        }

        // For test
        private void TestAstPrinter()
        {
            Expr expression = new Expr.Binary(
                new Expr.Unary(
                    new Token(TokenType.MINUS, "-", null, 1),
                    new Expr.Literal(123)),
                new Token(TokenType.STAR, "*", null, 1),
                new Expr.Grouping(
                    new Expr.Literal(45.67)));
            var printer = new AstPrinter();
            Console.WriteLine(printer.Print(expression));
        }

        private void RunFile(string file)
        {
            string source = File.ReadAllText(Path.GetFullPath(file), System.Text.Encoding.Default);
            Run(source);

            if (hadError) Environment.Exit(65);
            if (hadRuntimeError) Environment.Exit(70);
        }

        private void RunPrompt()
        {
            while (true)
            {
                Console.Write("> ");
                string? line = Console.ReadLine();
                if (line == null) break;
                // TODO: Use RunAstPrinter() instead of Run() to debug AST.
                //RunAstPrinter(line);
                Run(line);
                hadError = false;
            }
        }

        private void Run(string source)
        {
            Scanner scanner = new Scanner(source, this);
            List<Token> tokens = scanner.ScanTokens();
            Parser parser = new Parser(tokens, this);
            List<Stmt> statements = parser.Parse();

            if (hadError) return;

            Resolver resolver = new Resolver(interpreter, this);
            resolver.Resolve(statements);

            if (hadError) return;

            interpreter.Interpret(statements);
        }

        private void RunAstPrinter(string source)
        {
            Scanner scanner = new Scanner(source, this);
            List<Token> tokens = scanner.ScanTokens();
            Parser parser = new Parser(tokens, this);
            List<Stmt> statements = parser.Parse();

            if (hadError) return;

            AstPrinter astPrinter = new();
            foreach (var stmt in statements)
            {
                string result = astPrinter.Print(stmt);
                Console.WriteLine(result);
            }
        }

        #region IErrorListener
        public void Error(int line, string message)
        {
            Report(line, "", message);
        }

        public void Error(Token token, string message)
        {
            if (token.type == TokenType.EOF)
            {
                Report(token.line, " at end", message);
            }
            else
            {
                Report(token.line, $" at '{token.lexeme}'", message);
            }
        }

        public void RuntimeError(RuntimeException err)
        {
            Console.WriteLine($"{err.Message} \n[line {err.token.line}]");
            hadRuntimeError = true;
        }

        private void Report(int line, string where, string message)
        {
            Console.WriteLine($"[line {line}] Error {where}: {message}");
            hadError = true;
        }
        #endregion
    }
}
