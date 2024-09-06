namespace UnSospiro
{
    internal class MainProgram
    {
        private static void Main(string[] args)
        {
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

        private static void RunFile(string file)
        {
            string source = File.ReadAllText(Path.GetFullPath(file), System.Text.Encoding.Default);
            Run(source);
        }

        private static void RunPrompt()
        {
            while (true)
            {
                Console.Write("> ");
                string? line = Console.ReadLine();
                if (line == null) break;
                Run(line);
            }
        }

        private static void Run(string source)
        {
            Scanner scanner = new Scanner(source);
            List<Token> tokens = scanner.ScanTokens();

            foreach (var token in tokens)
            {
                Console.WriteLine($"{token}");
            }
        }
    }
}
