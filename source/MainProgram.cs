namespace UnSospiro
{
    internal class MainProgram
    {
        private static Interpreter interpreter = new Interpreter();
        private static bool hadError = false;
        private static bool hadRuntimeError = false;

        private static void Main(string[] args)
        {
            //TestAstPrinter();

            if (args.Length > 1)
            {
                Console.WriteLine("Usage: unsospiro [script]");
                Environment.Exit(64);
            }
            else if (args.Length == 1)
            {
                RunFile(args[0]);
            }
            else
            {
                RunPrompt();
            }
        }

        // For test
        private static void TestAstPrinter()
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

        private static void RunFile(string file)
        {
            string source = File.ReadAllText(Path.GetFullPath(file), System.Text.Encoding.Default);
            Run(source);

            if (hadError) Environment.Exit(65);
            if (hadRuntimeError) Environment.Exit(70);
        }

        private static void RunPrompt()
        {
            while (true)
            {
                Console.Write("> ");
                string? line = Console.ReadLine();
                if (line == null) break;
                Run(line);
                hadError = false;
            }
        }

        private static void Run(string source)
        {
            Scanner scanner = new Scanner(source);
            List<Token> tokens = scanner.ScanTokens();
            Parser parser = new Parser(tokens);
            List<Stmt> statements = parser.Parse();

            if (hadError) return;

            Resolver resolver = new Resolver(interpreter);
            resolver.Resolve(statements);

            if (hadError) return;

            interpreter.Interpret(statements);
            //Console.WriteLine(new AstPrinter().Print(expression));
            //foreach (var token in tokens) Console.WriteLine($"{token}");
        }

        internal static void Error(int line, string message)
        {
            Report(line, "", message);
        }

        internal static void Error(Token token, string message)
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

        internal static void RuntimeError(RuntimeException err)
        {
            Console.WriteLine($"{err.Message} \n[line {err.token.line}]");
            hadRuntimeError = true;
        }

        private static void Report(int line, string where, string message)
        {
            Console.WriteLine($"[line {line}] Error {where}: {message}");
            hadError = true;
        }
    }
}
